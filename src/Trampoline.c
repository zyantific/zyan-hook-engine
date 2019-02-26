/***************************************************************************************************

  Zyan Hook Library (Zyrex)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

#include <Windows.h>
#include <Zycore/Vector.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Internal/Trampoline.h>
#include <Zyrex/Internal/Utils.h>

/* ============================================================================================== */
/* Internal macros                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Constants                                                                                      */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   The size of the relative jump instruction (in bytes).
 */
#define ZYREX_SIZEOF_RELATIVE_JUMP      5

/**
 * @brief   The size of the absolute jump instruction (in bytes).
 */
#define ZYREX_SIZEOF_ABSOLUTE_JUMP      6

/**
 * @brief   The target range of the relative jump instruction.
 */
#define ZYREX_RANGEOF_RELATIVE_JUMP     0x7FFFFFFF

/**
 * @brief   Defines the maximum amount of instruction bytes that can be saved to a trampoline.
 *
 * This formula is based on the following edge case consideration:
 * - If `SIZEOF_SAVED_INSTRUCTIONS == SIZEOF_RELATIVE_JUMP - 1 == 4`
 *   - We have to save exactly one additional instruction
 *   - We already saved 4 bytes
 *   - The additional instructions maximum length is 15 bytes
 */
#define ZYREX_TRAMPOLINE_MAX_CODE_SIZE \
    (ZYDIS_MAX_INSTRUCTION_LENGTH + ZYREX_SIZEOF_RELATIVE_JUMP - 1)

/**
 * @brief   Defines the maximum amount of instruction bytes that can be saved to a trampoline
 *          including the backjump.
 */
#define ZYREX_TRAMPOLINE_MAX_CODE_SIZE_WITH_BACKJUMP \
    (ZYREX_TRAMPOLINE_MAX_CODE_SIZE + ZYREX_SIZEOF_ABSOLUTE_JUMP)

/**
 * @brief   Defines the maximum amount of instructions that can be saved to a trampoline.
 */
#define ZYREX_TRAMPOLINE_MAX_INSTRUCTION_COUNT \
    (ZYREX_SIZEOF_RELATIVE_JUMP)

/**
 * @brief   Defines the trampoline region signature.
 */
#define ZYREX_TRAMPOLINE_REGION_SIGNATURE   'zrex'

/* ---------------------------------------------------------------------------------------------- */
/* Macros                                                                                         */
/* ---------------------------------------------------------------------------------------------- */



/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * @brief   Defines the `ZyrexTrampolineChunk` struct.
 */
typedef struct ZyrexTrampolineChunk_
{
    /**
     * @brief   Signals, if the trampoline chunk is used.
     */
    ZyanBool is_used;

#if defined(ZYAN_X64)

    /**
     * @brief   The address of the callback function.
     */
    ZyanUPointer callback_address;
    /**
     * @brief   The absolute jump to the callback function.
     */
    ZyanU8 callback_jump[ZYREX_SIZEOF_ABSOLUTE_JUMP];

#endif

    /**
     * @brief   The backjump address.
     */
    ZyanUPointer backjump_address;
    /**
     * @brief   The trampoline code including the backjump to the hooked function.
     */
    ZyanU8 trampoline[ZYREX_TRAMPOLINE_MAX_CODE_SIZE_WITH_BACKJUMP];
} ZyrexTrampolineChunk;

// MSVC does not like the C99 flexible-array extension
#ifdef ZYAN_MSVC
#   pragma warning(push)
#   pragma warning(disable:4200)
#endif

/**
 * @brief   Defines the `ZyrexTrampolineRegion` struct.
 */
typedef struct ZyrexTrampolineRegion_
{
    /**
     * @brief   The signature of the trampoline region.
     */
    ZyanU32 signature;
    /**
     * @brief   The total number of trampoline chunks.
     */
    ZyanUSize number_of_chunks;
    /**
     * @rief    The number of unused trampoline chunks.
     */
    ZyanUSize number_of_unused_chunks;
    /**
     * @brief   The actual trampoline chunks.
     */
    ZyrexTrampolineChunk chunks[];
} ZyrexTrampolineRegion;

#ifdef ZYAN_MSVC
#   pragma warning(pop)
#endif

/* ============================================================================================== */
/* Globals                                                                                        */
/* ============================================================================================== */

// Thread-safety is implicitly guaranteed by the transactional API as only one transaction can be
// started at a time.

/**
 * @brief   Signals, if the global trampoline-region list is initialized.
 */
static ZyanBool   g_initialized;

/**
 * @brief   Contains a list of all owned trampoline-regions.
 */
static ZyanVector g_trampoline_regions;

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Helper functions                                                                               */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Returns the size of the allocated memory region starting at the given `address` up to a
 *          maximum size of `size`.
 *
 * @param   address The memory address.
 * @param   size    Receives the size of the allocated memory region which contains `address` and
 *                  defines the upper limit.
 *
 * @return  A zyan status code.
 *
 * This function is used to ensure no invalid memory gets accessed.
 */
static ZyanStatus ZyrexGetSizeOfAllocatedMemoryRegion(const void* address, ZyanUSize* size)
{
    ZYAN_ASSERT(address);
    ZYAN_ASSERT(size);

    MEMORY_BASIC_INFORMATION info;

    ZyanU8* current_address = (ZyanU8*)address;
    ZyanUSize current_size = 0;
    while (current_size < *size)
    {
        memset(&info, 0, sizeof(info));
        if (!VirtualQuery(current_address, &info, sizeof(info)))
        {
            return ZYAN_STATUS_BAD_SYSTEMCALL;
        }
        if (info.State != MEM_COMMIT)
        {
            *size = current_size;
            return ZYAN_STATUS_SUCCESS;
        }
        current_address = (ZyanU8*)info.BaseAddress + info.RegionSize;
        if (current_size == 0)
        {
            current_size = (ZyanUPointer)address - (ZyanUPointer)info.BaseAddress;
            continue;
        }
        current_size += info.RegionSize;
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

#ifdef ZYAN_X64

// TODO: Integrate this function in Zydis `ZyrexCalcAbsoluteAddressRaw`

/**
 * @brief   Calculates the absolute address value for the given instruction.
 *
 * @param   instruction     A pointer to the `ZydisDecodedInstruction` struct.
 * @param   runtime_address The runtime address of the instruction.
 * @param   result_address  A pointer to the memory that receives the absolute address.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexCalcAbsoluteAddress(const ZydisDecodedInstruction* instruction,
    ZyanU64 runtime_address, ZyanU64* result_address)
{
    ZYAN_ASSERT(instruction);
    ZYAN_ASSERT(result_address);
    ZYAN_ASSERT(instruction->attributes & ZYDIS_ATTRIB_IS_RELATIVE);

    // Instruction with EIP/RIP-relative memory operand
    if ((instruction->attributes & ZYDIS_ATTRIB_HAS_MODRM) &&
        (instruction->raw.modrm.mod == 0) &&
        (instruction->raw.modrm.rm  == 5))
    {
        if (instruction->address_width == ZYDIS_ADDRESS_WIDTH_32)
        {
            *result_address = ((ZyanU32)runtime_address + instruction->length +
                (ZyanU32)instruction->raw.disp.value);

            return ZYAN_STATUS_SUCCESS;
        }
        if (instruction->address_width == ZYDIS_ADDRESS_WIDTH_64)
        {
            *result_address = (ZyanU64)(runtime_address + instruction->length +
                instruction->raw.disp.value);

            return ZYAN_STATUS_SUCCESS;
        }
    }

    // Relative branch instruction
    if (instruction->raw.imm[0].is_signed &&
        instruction->raw.imm[0].is_relative)
    {
        *result_address = (ZyanU64)((ZyanI64)runtime_address + instruction->length +
            instruction->raw.imm[0].value.s);
        switch (instruction->machine_mode)
        {
        case ZYDIS_MACHINE_MODE_LONG_COMPAT_16:
        case ZYDIS_MACHINE_MODE_LEGACY_16:
        case ZYDIS_MACHINE_MODE_REAL_16:
        case ZYDIS_MACHINE_MODE_LONG_COMPAT_32:
        case ZYDIS_MACHINE_MODE_LEGACY_32:
            if (instruction->operand_width == 16)
            {
                *result_address &= 0xFFFF;
            }
            break;
        case ZYDIS_MACHINE_MODE_LONG_64:
            break;
        default:
            ZYAN_UNREACHABLE;
        }

        return ZYAN_STATUS_SUCCESS;
    }

    ZYAN_UNREACHABLE;
}

/**
 * @brief   Disassembles the code at the given `address` and returns the lowest and highest
 *          absolute address of all relative branch instructions and `EIP/RIP`-relative memory
 *          operands.
 *
 * @param   address     The address of the code.
 * @param   size        The minimum size of bytes to disassemble.
 * @param   address_lo  Receives the lowest absolute target address.
 * @param   address_hi  Receives the highest absolute target address.
 *
 * @return  `ZYAN_STATUS_TRUE` if at least one instruction with a relative operand was found,
 *          `ZYAN_STATUS_FALSE` if not, or a generic zyan status code if an error occured.
 */
static ZyanStatus ZyrexGetAddressRangeOfRelativeInstructions(const void* address, ZyanUSize size,
    ZyanUPointer* address_lo, ZyanUPointer* address_hi)
{
    ZyanStatus result = ZYAN_STATUS_FALSE;

    ZydisDecoder decoder;
#if defined(ZYAN_X86)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_ADDRESS_WIDTH_32);
#elif defined(ZYAN_X64)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
#else
#   error "Unsupported architecture detected"
#endif

    ZyanUSize size_max = ZYREX_TRAMPOLINE_MAX_CODE_SIZE;
    ZYAN_CHECK(ZyrexGetSizeOfAllocatedMemoryRegion(address, &size_max));

    ZyanUPointer lo = 0;
    ZyanUPointer hi = 0;
    ZydisDecodedInstruction instruction;
    ZyanUSize offset = 0;
    while (offset < size)
    {
        const ZyanStatus status =
            ZydisDecoderDecodeBuffer(&decoder, (ZyanU8*)address + offset, size_max - offset,
                &instruction);

        ZYAN_CHECK(status);

        if (!(instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE))
        {
            offset += instruction.length;
            continue;
        }
        result = ZYAN_STATUS_TRUE;

        ZyanU64 result_address;
        ZYAN_CHECK(ZyrexCalcAbsoluteAddress(&instruction, (ZyanU64)address + offset,
            &result_address));

        if (result_address < lo)
        {
            lo = result_address;
        }
        if (result_address > hi)
        {
            hi = result_address;
        }

        offset += instruction.length;
    }

    *address_lo = lo;
    *address_hi = hi;

    return result;
}

#endif

/* ---------------------------------------------------------------------------------------------- */
/* Global memory list                                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Searches the given trampoline-region for an unused `ZyrexTrampolineChunk` item that
 *          lies in a +/-2GiB range to both given address bounds.
 *
 * @param   region      A pointer to the `ZyrexTrampolineRegion` struct.
 * @param   address_lo  The lower bound memory address to be used as search condition.
 * @param   address_hi  The upper bound memory address to be used as search condition.
 * @param   chunk       Receives a pointer to a matching `ZyrexTrampolineChunk` struct.
 *
 * This function assumes the address lies above the given chunk region.
 */
static ZyanBool ZyrexTrampolineRegionFindChunkInRegion(ZyrexTrampolineRegion* region,
    ZyanUPointer address_lo, ZyanUPointer address_hi, ZyrexTrampolineChunk** chunk)
{
    ZYAN_ASSERT(region);
    ZYAN_ASSERT(chunk);

    // Are there any free chunks?
    if (region->number_of_unused_chunks == 0)
    {
        return ZYAN_FALSE;
    }

    // Do we have at least one chunk that lies in the desired range?
    const ZyanIPointer region_base = (ZyanIPointer)&region->chunks[0];

    const ZyanIPointer distance_lo =
        region_base - (ZyanIPointer)address_lo + (ZyanIPointer)address_lo < region_base
            ? sizeof(ZyrexTrampolineChunk)
            : sizeof(ZyrexTrampolineChunk) * (region->number_of_chunks - 1);
    if ((ZYAN_ABS(distance_lo) > ZYREX_RANGEOF_RELATIVE_JUMP))
    {
        return ZYAN_FALSE;
    }

    const ZyanIPointer distance_hi =
        region_base - (ZyanIPointer)address_hi + (ZyanIPointer)address_hi < region_base
            ? sizeof(ZyrexTrampolineChunk)
            : sizeof(ZyrexTrampolineChunk) * (region->number_of_chunks - 1);
    if ((ZYAN_ABS(distance_hi) > ZYREX_RANGEOF_RELATIVE_JUMP))
    {
        return ZYAN_FALSE;
    }

    // Find a free chunk that lies in the desired range
    for (ZyanUSize i = 0; i < region->number_of_chunks; ++i)
    {
        const ZyanIPointer chunk_base = (ZyanIPointer)&region->chunks[i];

        const ZyanIPointer distance_lo_chunk = chunk_base - (ZyanIPointer)address_lo +
            (((ZyanIPointer)address_lo < chunk_base) ? sizeof(ZyrexTrampolineChunk) : 0);
        if ((ZYAN_ABS(distance_lo_chunk) > ZYREX_RANGEOF_RELATIVE_JUMP))
        {
            continue;
        }

        const ZyanIPointer distance_hi_chunk = chunk_base - (ZyanIPointer)address_hi +
            (((ZyanIPointer)address_hi < chunk_base) ? sizeof(ZyrexTrampolineChunk) : 0);
        if ((ZYAN_ABS(distance_hi_chunk) > ZYREX_RANGEOF_RELATIVE_JUMP))
        {
            continue;
        }

        *chunk = &region->chunks[i];
        return ZYAN_TRUE;
    }

    return ZYAN_FALSE;
}

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Inserts a new `ZyrexTrampolineRegion` item to the global trampoline-region list.
 *
 * @param   region  A pointer to the `ZyrexTrampolineRegion` item.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexTrampolineRegionInsert(ZyrexTrampolineRegion* region)
{
    ZYAN_ASSERT(region);

    ZyanUSize found_index;
    const ZyanStatus status =
        ZyanVectorBinarySearch(&g_trampoline_regions, &region, &found_index,
            (ZyanComparison)&ZyanComparePointer);
    ZYAN_CHECK(status);

    ZYAN_ASSERT(status == ZYAN_STATUS_FALSE);
    return ZyanVectorInsert(&g_trampoline_regions, found_index, &region);
}

/**
 * @brief   Removes the given `ZyrexTrampolineRegion` item from the global trampoline-region list.
 *
 * @param   region  A pointer to the `ZyrexTrampolineRegion` item.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexTrampolineRegionRemove(ZyrexTrampolineRegion* region)
{
    ZYAN_ASSERT(region);

    ZyanUSize found_index;
    const ZyanStatus status =
        ZyanVectorBinarySearch(&g_trampoline_regions, &region, &found_index,
            (ZyanComparison)&ZyanComparePointer);
    ZYAN_CHECK(status);

    ZYAN_ASSERT(status == ZYAN_STATUS_TRUE);
    return ZyanVectorDelete(&g_trampoline_regions, found_index);
}

/**
 * @brief   Searches the global trampoline-region list for an unused `ZyrexTrampolineChunk` item
 *          that lies in a +/-2GiB range to both given address bounds.
 *
 * @param   address_lo  The lower bound memory address to be used as search condition.
 * @param   address_hi  The upper bound memory address to be used as search condition.
 * @param   region      Receives a pointer to a matching `ZyrexTrampolineRegion` struct.
 * @param   chunk       Receives a pointer to a matching `ZyrexTrampolineChunk` struct.
 *
 * @return  `ZYAN_STATUS_TRUE` if a valid chunk was found in an already allocated trampoline region,
 *          `ZYAN_STATUS_FALSE` if not, or a generic zyan status code if an error occured.
 */
static ZyanStatus ZyrexTrampolineRegionFindChunk(ZyanUPointer address_lo, ZyanUPointer address_hi,
    ZyrexTrampolineRegion** region, ZyrexTrampolineChunk** chunk)
{
    ZYAN_ASSERT(region);
    ZYAN_ASSERT(chunk);

    ZyanUSize size;
    ZYAN_CHECK(ZyanVectorGetSize(&g_trampoline_regions, &size));
    if (size == 0)
    {
        goto RegionNotFound;
    }

    const ZyanUSize middle = (address_lo + address_hi) / 2;
    ZyanUSize found_index;
    const ZyanStatus status =
        ZyanVectorBinarySearch(&g_trampoline_regions, &middle, &found_index,
            (ZyanComparison)&ZyanComparePointer);
    ZYAN_CHECK(status);

    ZyanUSize lo = found_index;
    ZyanUSize hi = found_index + 1;
    ZyrexTrampolineRegion* element;
    while (ZYAN_TRUE)
    {
        ZyanU8 c = 0;

        if (lo >= 0)
        {
            element = (ZyrexTrampolineRegion*)ZyanVectorGet(&g_trampoline_regions, lo--);
            ZYAN_ASSERT(element);
            if (ZyrexTrampolineRegionFindChunkInRegion(element, address_lo, address_hi, chunk))
            {
                break;
            }
            ++c;
        }
        if (hi < size)
        {
            element = (ZyrexTrampolineRegion*)ZyanVectorGet(&g_trampoline_regions, hi++);
            ZYAN_ASSERT(element);
            if (ZyrexTrampolineRegionFindChunkInRegion(element, address_lo, address_hi, chunk))
            {
                break;
            }
            ++c;
        }

        if (c == 0)
        {
            goto RegionNotFound;
        }
    }

    *region = element;
    return ZYAN_STATUS_TRUE;

RegionNotFound:
    return ZYAN_STATUS_FALSE;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Public functions                                                                               */
/* ============================================================================================== */

ZyanStatus ZyrexTrampolineCreate(const void* address, ZyanUSize size, const void* callback,
    const ZyrexTrampoline* trampoline)
{
    if (!address || (size < 1) || !callback || !trampoline)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    // Check if the memory region of the target function has enough space for the hook code
    ZyanUSize source_size = ZYREX_TRAMPOLINE_MAX_CODE_SIZE;
    ZYAN_CHECK(ZyrexGetSizeOfAllocatedMemoryRegion(address, &source_size));
    if (source_size < size)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    if (!g_initialized)
    {
        ZYAN_CHECK(ZyanVectorInit(&g_trampoline_regions, sizeof(ZyrexTrampolineRegion*), 8));
        g_initialized = ZYAN_TRUE;
    }

#ifdef ZYAN_X64

    // Determine lower and upper address bounds required to find a suitable memory region for the
    // trampoline
    ZyanUPointer lo;
    ZyanUPointer hi;
    ZYAN_CHECK(ZyrexGetAddressRangeOfRelativeInstructions(address, size, &lo, &hi));

    const ZyanUPointer address_value = (ZyanUPointer)address;
    if (address_value < lo)
    {
        lo = address_value;
    }
    if (address_value > hi)
    {
        hi = address_value;
    }

    if ((hi - lo) > ZYREX_RANGEOF_RELATIVE_JUMP)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

#endif

    ZyrexTrampolineRegion* region;
    ZyrexTrampolineChunk* chunk;
    ZYAN_CHECK(ZyrexTrampolineRegionFindChunk(lo, hi, &region, &chunk));

//#ifdef ZYAN_MSVC
//    __try
//    {
//
//    }
//#endif
//
//#ifdef ZYAN_MSVC
//    __except(EXCEPTION_EXECUTE_HANDLER)
//    {
//
//    }
//#endif

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTrampolineFree(const ZyrexTrampoline* trampoline)
{
    ZYAN_UNUSED(trampoline);

    ZYAN_ASSERT(g_initialized);

    ZyanUSize size;
    ZYAN_CHECK(ZyanVectorGetSize(&g_trampoline_regions, &size));
    if (size == 0)
    {
        ZYAN_CHECK(ZyanVectorDestroy(&g_trampoline_regions, ZYAN_NULL));
        g_initialized = ZYAN_FALSE;
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
