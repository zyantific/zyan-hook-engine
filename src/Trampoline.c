/***************************************************************************************************

  Zyan Hook Engine (Zyrex)
  Version 1.0

  Remarks         : Freeware, Copyright must be included

  Original Author : Florian Bernd
  Modifications   :

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <Windows.h>
#undef IN  // Windows.h
#undef OUT // Windows.h
#include <Zyrex/Trampoline.h>
#include <Zyrex/Utils.h>
#include <Zydis/Zydis.h>

/* ============================================================================================== */
/* Defines                                                                                        */
/* ============================================================================================== */

/**
 * @brief   Defines the maximum amount of instruction bytes that can be saved to a trampoline.
 */
#define ZYREX_TRAMPOLINE_CODE_SIZE          27

/**
 * @brief   Defines the maximum amount of instruction bytes that can be restored from a trampoline.
 */
#define ZYREX_TRAMPOLINE_RESTORE_SIZE       19

/**
 * @brief   Defines the maximum amount of items in the trampoline instruction translation map. 
 *          Each saved instruction requires one slot in the map.
 */
#define ZYREX_TRAMPOLINE_INSTRUCTION_COUNT  5

/**
 * @brief   Defines the trampoline region signature.
 */
#define ZYREX_TRAMPOLINE_REGION_SIGNATURE   'zrex'

/* ============================================================================================== */
/* Structs                                                                                        */
/* ============================================================================================== */

/**
 * @brief   Defines an instruction map item.
 */
typedef struct ZyrexInstructionMapItem
{
    uint8_t offsetCode;
    uint8_t offsetTrampoline;
} ZyrexInstructionMapItem;

/**
 * @brief   Defines the trampoline struct.
 */
typedef struct ZyrexTrampoline
{
    uint8_t code[ZYREX_TRAMPOLINE_CODE_SIZE + ZYREX_SIZEOF_JUMP_ABSOLUTE];
    uint8_t codeSize;
    uint8_t restore[ZYREX_TRAMPOLINE_RESTORE_SIZE];
    uint8_t restoreSize;
    ZyrexInstructionMapItem instructionMap[ZYREX_TRAMPOLINE_INSTRUCTION_COUNT];
    uint8_t instructionCount;
    uint8_t callbackJump[ZYREX_SIZEOF_JUMP_ABSOLUTE];
    const void* callbackAddress;
    const void* backjumpAddress;
    struct ZyrexTrampoline* next;
} ZyrexTrampoline;

/**
 * @brief   Defines the the zyrex trampoline region struct.
 */
typedef struct ZyrexTrampolineRegion
{
    uint32_t signature;
    ZyrexTrampoline* freeTrampolines;
    size_t freeTrampolineCount;
    struct ZyrexTrampolineRegion* next;
} ZyrexTrampolineRegion;

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/**
 * @brief   Clips the given @c size if it is larger than the commited memory region that contains 
 *          @c address.
 *          
 * @param   address The address.
 * @param   size    Receives the clipped size, if the function succeeded.
 *       
 * @return  @c ZYREX_ERROR_SUCCESS if succeeded, or an other zyrex status code, if not.   
 *
 * This method ensures no invalid memory gets accessed.
 */
static ZyrexStatus ZyrexClipRegionSize(const void* address, size_t* size)
{
    MEMORY_BASIC_INFORMATION memInfo;
    memset(&memInfo, 0, sizeof(memInfo));
    if (!VirtualQuery(address, &memInfo, sizeof(memInfo)))
    {
        return ZYREX_ERROR_SYSTEMCALL;
    }
    if (memInfo.State != MEM_COMMIT)
    {
        *size = 0;
        return ZYREX_ERROR_SUCCESS;
    }
    size_t usableRegionSize = (uintptr_t)(address) - 
        (uintptr_t)(memInfo.BaseAddress) + memInfo.RegionSize;
    if (*size > usableRegionSize)
    {
        *size = usableRegionSize;
    }
    return ZYREX_ERROR_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Rounds the given @c address up to the desired @c alignment value.
 *
 * @param   address     The address.
 * @param   alignment   The alignment.
 *
 * @return  The aligned address.
 */
static uintptr_t ZyrexAddressRoundUp(uintptr_t address, const uintptr_t alignment)
{
    return (address + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief   Rounds the given @c address down to the desired @c alignment value.
 *
 * @param   address     The address.
 * @param   alignment   The alignment.
 *
 * @return  The aligned address.
 */
static uintptr_t ZyrexAddressRoundDown(uintptr_t address, const uintptr_t alignment)
{
    return (address - 1) & ~(alignment - 1);
}

/**
 * @brief   Searches upwards for a suitable trampoline memory region beginning at the lower address 
 *          limit and allocates the memory if succeeded.
 *
 * @param   addressLo   The lower address limit.
 * @param   addressHi   The upper address limit.
 * @param   region      Receives a pointer to the newly allocated trampoline region.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
static ZyrexStatus ZyrexAllocateTrampolineRegionFromLo(uintptr_t addressLo, uintptr_t addressHi, 
    void** region)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    // Guarantee correctly aligned addresses
    const uint8_t* allocAddressLo = 
        (const uint8_t*)ZyrexAddressRoundUp(addressLo, systemInfo.dwAllocationGranularity);
    const uint8_t* allocAddressHi = 
        (const uint8_t*)ZyrexAddressRoundDown(addressHi, systemInfo.dwAllocationGranularity); 

    // Find suitable trampoline region
    const uint8_t* allocAddress = allocAddressLo;
    while (allocAddress < allocAddressHi)
    {
        // Skip reserved address regions
        if (allocAddress < (const uint8_t*)systemInfo.lpMinimumApplicationAddress)
        {
            allocAddress = (const uint8_t*)systemInfo.lpMinimumApplicationAddress;
            continue;
        }
        if (allocAddress > (const uint8_t*)systemInfo.lpMaximumApplicationAddress)
        {
            allocAddress = (const uint8_t*)systemInfo.lpMaximumApplicationAddress;
            continue;
        }

        // Check region and allocate memory
        MEMORY_BASIC_INFORMATION memoryInfo;
        memset(&memoryInfo, 0, sizeof(memoryInfo));
        if (!VirtualQuery(allocAddress, &memoryInfo, sizeof(memoryInfo)))
        {
            return ZYREX_ERROR_SYSTEMCALL;
        }
        if ((memoryInfo.State == MEM_FREE) &&
            (memoryInfo.RegionSize >= systemInfo.dwAllocationGranularity))
        {
            *region = VirtualAlloc((void*)allocAddress, systemInfo.dwAllocationGranularity,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (*region)
            {
                return ZYREX_ERROR_SUCCESS;
            }
        }
        allocAddress = (const uint8_t*)ZyrexAddressRoundUp((uintptr_t)memoryInfo.BaseAddress + 
            memoryInfo.RegionSize, systemInfo.dwAllocationGranularity);    
    }
    return ZYREX_ERROR_TRAMPOLINE_ADDRESS;
}

/**
 * @brief   Searches downwards for a suitable trampoline memory region beginning at the upper 
 *          address limit and allocates the memory if succeeded.
 *
 * @param   addressLo   The lower address limit.
 * @param   addressHi   The upper address limit.
 * @param   region      Receives a pointer to the newly allocated trampoline region.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
static ZyrexStatus ZyrexAllocateTrampolineRegionFromHi(uintptr_t addressLo, uintptr_t addressHi,
    void** region)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    // Guarantee correctly aligned addresses
    const uint8_t* allocAddressLo = 
        (const uint8_t*)ZyrexAddressRoundUp(addressLo, systemInfo.dwAllocationGranularity);
    const uint8_t* allocAddressHi = 
        (const uint8_t*)ZyrexAddressRoundDown(addressHi, systemInfo.dwAllocationGranularity); 

    // Find suitable trampoline region
    const uint8_t* allocAddress = allocAddressHi;
    while (allocAddress >= allocAddressLo)
    {
        // Skip reserved address regions
        if (allocAddress < (const uint8_t*)systemInfo.lpMinimumApplicationAddress)
        {
            allocAddress = (const uint8_t*)systemInfo.lpMinimumApplicationAddress;
            continue;
        }
        if (allocAddress > (const uint8_t*)systemInfo.lpMaximumApplicationAddress)
        {
            allocAddress = (const uint8_t*)systemInfo.lpMaximumApplicationAddress;
            continue;
        }

        // Check region and allocate memory
        MEMORY_BASIC_INFORMATION memoryInfo;
        memset(&memoryInfo, 0, sizeof(memoryInfo));
        if (!VirtualQuery(allocAddress, &memoryInfo, sizeof(memoryInfo)))
        {
            return ZYREX_ERROR_SYSTEMCALL;
        }
        if ((memoryInfo.State == MEM_FREE) &&
            (memoryInfo.RegionSize >= systemInfo.dwAllocationGranularity))
        {
            *region = VirtualAlloc((void*)allocAddress, systemInfo.dwAllocationGranularity,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (*region)
            {
                return ZYREX_ERROR_SUCCESS;
            }
        }
        allocAddress = (const uint8_t*)ZyrexAddressRoundUp((uintptr_t)memoryInfo.BaseAddress - 
            memoryInfo.RegionSize, systemInfo.dwAllocationGranularity);    
    }
    return ZYREX_ERROR_TRAMPOLINE_ADDRESS;
}

/* ---------------------------------------------------------------------------------------------- */

static ZyrexTrampolineRegion* g_trampolineRegions = NULL;

/**
 * @brief   Changes the memory protection of a trampoline region.
 *
 * @param   region      The trampoline region.
 * @param   protection  The new memory protection value.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
static ZyrexStatus ZyrexChangeRegionProtection(ZyrexTrampolineRegion* region, DWORD protection)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    DWORD oldProtection;
    if (!VirtualProtect(region, systemInfo.dwAllocationGranularity, protection, &oldProtection))
    {
        return ZYREX_ERROR_SYSTEMCALL;
    }
    return ZYREX_ERROR_SUCCESS;
}

/**
 * @brief   Acquires a suitable trampoline location.
 *
 * @param   codeAddress The code address.
 * @param   trampoline  Receives a pointer to the trampoline.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 *
 * This function searches all existing trampoline regions for a suitable trampoline location or 
 * tries to allocate a new one.
 */
static ZyrexStatus ZyrexAcquireTrampoline(const void* codeAddress, ZyrexTrampoline** trampoline)
{
    // Search location in existing regions
    ZyrexTrampolineRegion* bestRegion = NULL;
    ZyrexTrampolineRegion* currentRegion = g_trampolineRegions;
    while (currentRegion)
    {
        if (currentRegion->freeTrampolineCount > 0)
        {
            const intptr_t currentDistance = 
                abs((int)((intptr_t)currentRegion - (intptr_t)codeAddress));
            if (currentDistance < 0x7FFFFFFF)
            {
                if (currentDistance < abs((int)((intptr_t)bestRegion - (intptr_t)codeAddress)))
                {
                    bestRegion = currentRegion;
                }
            }
        }
        currentRegion = currentRegion->next;
    };
    if (bestRegion)
    {
        *trampoline = bestRegion->freeTrampolines;

        // Enable write access to the trampoline region
        ZYREX_CHECK(ZyrexChangeRegionProtection(bestRegion, PAGE_EXECUTE_READWRITE));

        bestRegion->freeTrampolineCount--;
        bestRegion->freeTrampolines = bestRegion->freeTrampolines->next;
        memset(*trampoline, 0xCC, sizeof(ZyrexTrampoline));

        // Make trampoline region read-only again
        ZYREX_CHECK(ZyrexChangeRegionProtection(bestRegion, PAGE_EXECUTE_READ));

        return ZYREX_ERROR_SUCCESS;
    } 

    // Allocate a new trampoline region
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    void* memory;
    if (ZyrexAllocateTrampolineRegionFromLo((uintptr_t)codeAddress, 
        (uintptr_t)codeAddress + 0x7FFFFFFF, &memory) != ZYREX_ERROR_SUCCESS)
    {
        ZYREX_CHECK(ZyrexAllocateTrampolineRegionFromHi((uintptr_t)codeAddress,
            (uintptr_t)codeAddress - 0x7FFFFFFF, &memory));
    }

    ZyrexTrampolineRegion* newRegion = (ZyrexTrampolineRegion*)memory;
    newRegion->signature = ZYREX_TRAMPOLINE_REGION_SIGNATURE;
    newRegion->next = g_trampolineRegions;
    newRegion->freeTrampolines = NULL;
    newRegion->freeTrampolineCount = 
        (systemInfo.dwAllocationGranularity / sizeof(ZyrexTrampoline)) - 2;
    ZyrexTrampoline* trampolines = (ZyrexTrampoline*)newRegion + 1;
    ZyrexTrampoline* nextTrampoline = NULL;
    for (size_t i = newRegion->freeTrampolineCount; i > 0; --i)
    {
        trampolines[i].next = nextTrampoline;
        nextTrampoline = &trampolines[i];
    }
    newRegion->freeTrampolines = nextTrampoline;
    g_trampolineRegions = newRegion;

    *trampoline = trampolines;
    memset(*trampoline, 0xCC, sizeof(ZyrexTrampoline));

    // Make trampoline region read-only
    ZYREX_CHECK(ZyrexChangeRegionProtection(newRegion, PAGE_EXECUTE_READ));

    return ZYREX_ERROR_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the instruction map.
 *          
 * An instruction map maps the offset of each saved instruction in the original code to the offset 
 * of the corresponding instruction in the trampoline.
 */
typedef ZyrexInstructionMapItem* ZyrexInstructionMap;

/**
 * @brief   Recalculates the offset of a relocated relative instruction.
 *
 * @param   instrSource     The source address of the relative instruction.
 * @param   instrTarget     The target address of the relative instruction.
 * @param   instrLength     The length of the relative instruction.
 * @param   sourceOffset    The original relative offset.
 * @param   rebasedOffset   Receives the rebased offset.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
static ZyrexStatus ZyrexCalcRelativeOffset(const void* instrSource, const void* instrTarget,
    uint8_t instrLength, int32_t sourceOffset, int32_t* rebasedOffset)
{
    intptr_t targetAddress = (intptr_t)instrSource + instrLength + sourceOffset;
    intptr_t targetIP      = (intptr_t)instrTarget + instrLength;

#define _ZYREX_HALF_MAX_SIGNED(type) ((type)1 << (sizeof(type) * 8 - 2))
#define _ZYREX_MAX_SIGNED(type) (_ZYREX_HALF_MAX_SIGNED(type) - 1 + _ZYREX_HALF_MAX_SIGNED(type))
#define _ZYREX_MIN_SIGNED(type) (-1 - _ZYREX_MAX_SIGNED(type))
#define _ZYREX_DOES_OVERFLOW(type, a, b) \
    (((a > 0) && (b > _ZYREX_MAX_SIGNED(type) - a)) || \
    ((a < 0) && (b < _ZYREX_MIN_SIGNED(type) - a)))

    if (_ZYREX_DOES_OVERFLOW(intptr_t, targetAddress, -targetIP))
    {
        return ZYREX_ERROR_NOT_RELOCATEABLE;
    }

#undef _ZYREX_DOES_OVERFLOW
#undef _ZYREX_MIN_SIGNED
#undef _ZYREX_MAX_SIGNED
#undef _ZYREX_HALF_MAX_SIGNED

    *rebasedOffset = (int32_t)(targetAddress - targetIP);
    return ZYREX_ERROR_SUCCESS;
}

/**
 * @brief   Copies instructions from the @c source address, to the @c target destination.
 *
 * @param   source                  The source address.
 * @param   target                  The target address.
 * @param   sourceBufferSize        The size of the source buffer.
 * @param   targetBufferSize        The size of the target buffer.
 * @param   detourPayloadSize       The minimum amount of instructions bytes that needs to be 
 *                                  copied.
 * @param   bytesRead               The actual amount of instruction bytes read from the @c source 
 *                                  address.
 * @oaram   bytesWritten            The actual amount of instruction bytes written to the @c target 
 *                                  address.
 * @param   instructionMap          Pointer to a buffer that receives the instruction map.
 * @param   instructionMapCapacity  The capacity of the instruction map.
 * @param   instructionCount        The number of instruction written to the target buffer.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 *          
 * This function tries to relocate 32-bit relative instructions and automatically enlarges smaller
 * ones.
 */
static ZyrexStatus ZyrexCopyInstructions(const void* source, void* target, 
    uint8_t sourceBufferSize, uint8_t targetBufferSize, uint8_t detourPayloadSize, 
    uint8_t* bytesRead, uint8_t* bytesWritten, ZyrexInstructionMap instructionMap, 
    uint8_t instructionMapCapacity, uint8_t* instructionCount)
{
    assert(source);
    assert(target);
    assert(detourPayloadSize > 0);

//    // Initialize disassembler
//    Zydis::InstructionInfo info;
//    Zydis::InstructionDecoder decoder;
//#ifdef ZYREX_X86
//    decoder.setDisassemblerMode(Zydis::DisassemblerMode::M32BIT);
//#else
//    decoder.setDisassemblerMode(Zydis::DisassemblerMode::M64BIT);
//#endif
//    Zydis::MemoryInput input(source, sourceBufferSize);
//    decoder.setDataSource(&input);
//    decoder.setInstructionPointer(0);
//
//    // Copy instructions
//    uint8_t instrCount = 0;
//    const uint8_t* instrSource = (const uint8_t*)source;
//    uint8_t* instrTarget = (uint8_t*)target;
//    for (;;)
//    {
//        if (instructionMap && (instrCount >= instructionMapCapacity))
//        {
//            // Translation map is full
//            return ZYREX_ERROR_NOT_HOOKABLE;
//        }
//        if ((instrTarget - (uint8_t*)target) >= targetBufferSize)
//        {
//            // Target buffer is full
//            return ZYREX_ERROR_NOT_HOOKABLE;
//        }
//        if (!decoder.decodeInstruction(info))
//        {
//            // Source buffer is empty
//            return ZYREX_ERROR_NOT_HOOKABLE;
//        }
//        if ((info.flags & Zydis::IF_ERROR_MASK))
//        {
//            // Invalid instruction
//            return ZYREX_ERROR_DISASSEMBLER;
//        }
//        memcpy(instrTarget, instrSource, info.length);
//        int8_t offset = 0;
//        if (info.flags & Zydis::IF_RELATIVE)
//        {
//            if (info.operand[0].type == Zydis::OperandType::REL_IMMEDIATE)
//            {
//                if (info.operand[0].size != 32)
//                {
//                    // Enlarge relative instruction to 32 bit
//                    uint8_t* enlargeTarget = (uint8_t*)instrTarget;
//                    offset = 6 - info.length;
//                    *(enlargeTarget) = 0x0F;
//                    switch (info.mnemonic)
//                    {
//                    case Zydis::InstructionMnemonic::JMP:
//                        *(enlargeTarget) = 0xE9;
//                        offset = 5 - info.length;
//                        break;
//                    case Zydis::InstructionMnemonic::JO:
//                        *(++enlargeTarget) = 0x80;
//                        break;
//                    case Zydis::InstructionMnemonic::JNO:
//                        *(++enlargeTarget) = 0x81;
//                        break;
//                    case Zydis::InstructionMnemonic::JB:
//                        *(++enlargeTarget) = 0x82;
//                        break;
//                    case Zydis::InstructionMnemonic::JNB:
//                        *(++enlargeTarget) = 0x83;
//                        break;
//                    case Zydis::InstructionMnemonic::JE:
//                        *(++enlargeTarget) = 0x84;
//                        break;
//                    case Zydis::InstructionMnemonic::JNE:
//                        *(++enlargeTarget) = 0x85;
//                        break;
//                    case Zydis::InstructionMnemonic::JBE:
//                        *(++enlargeTarget) = 0x86;
//                        break;
//                    case Zydis::InstructionMnemonic::JA:
//                        *(++enlargeTarget) = 0x87;
//                        break;
//                    case Zydis::InstructionMnemonic::JS:
//                        *(++enlargeTarget) = 0x88;
//                        break;
//                    case Zydis::InstructionMnemonic::JNS:
//                        *(++enlargeTarget) = 0x89;
//                        break;
//                    case Zydis::InstructionMnemonic::JP:
//                        *(++enlargeTarget) = 0x8A;
//                        break;
//                    case Zydis::InstructionMnemonic::JNP:
//                        *(++enlargeTarget) = 0x8B;
//                        break;
//                    case Zydis::InstructionMnemonic::JL:
//                        *(++enlargeTarget) = 0x8C;
//                        break;
//                    case Zydis::InstructionMnemonic::JGE:
//                        *(++enlargeTarget) = 0x8D;
//                        break;
//                    case Zydis::InstructionMnemonic::JLE:
//                        *(++enlargeTarget) = 0x8E;
//                        break;
//                    case Zydis::InstructionMnemonic::JG:
//                        *(++enlargeTarget) = 0x8F;
//                        break;
//                    default:
//                        return ZYREX_ERROR_NOT_ENLARGEABLE;
//                    }
//                    int32_t sourceOffset = 0;
//                    switch (info.operand[0].size)
//                    {
//                    case 8:
//                        sourceOffset = info.operand[0].lval.sbyte;
//                        break;
//                    case 16:
//                        sourceOffset = info.operand[0].lval.sword;
//                        break;
//                    default:
//                        assert(0);
//                    }
//                    int32_t rebasedOffset;
//                    ZYREX_CHECK(ZyrexCalcRelativeOffset(instrSource + info.length, 
//                        instrTarget + offset + info.length, 0, sourceOffset, &rebasedOffset));
//                    *(int32_t*)++enlargeTarget = (int32_t)rebasedOffset;
//                } else
//                {
//                    // Update relative offset
//                    assert(info.mnemonic == Zydis::InstructionMnemonic::JMP ||
//                        info.mnemonic == Zydis::InstructionMnemonic::CALL);
//                    // TODO: Add option
//                    if (info.mnemonic == Zydis::InstructionMnemonic::CALL)
//                    {
//                        return ZYREX_ERROR_NOT_HOOKABLE;
//                    }
//                    int32_t rebasedOffset;
//                    ZYREX_CHECK(ZyrexCalcRelativeOffset(instrSource, instrTarget, info.length, 
//                        info.operand[0].lval.sdword, &rebasedOffset));
//                    *(int32_t*)((uintptr_t)instrTarget + info.operand[0].lvalElementOffset) = 
//                        rebasedOffset;
//                }
//            } else
//            {
//                // RIP-relative instruction
//                for (unsigned i = 0; i < 4; ++i)
//                {
//                    if ((info.operand[i].type == Zydis::OperandType::MEMORY) &&
//                        (info.operand[i].base == Zydis::Register::RIP))
//                    {
//                        int32_t rebasedOffset;
//                        ZYREX_CHECK(ZyrexCalcRelativeOffset(instrSource, instrTarget, info.length, 
//                            info.operand[i].lval.sdword, &rebasedOffset));
//                        *(int32_t*)((uintptr_t)instrTarget + info.operand[i].lvalElementOffset) = 
//                            rebasedOffset;
//                        break;
//                    }
//                }
//            }
//        }
//        if (instructionMap)
//        {
//            instructionMap[instrCount].offsetCode = (uint8_t)info.instrAddress;
//            instructionMap[instrCount].offsetTrampoline = (uint8_t)(instrTarget - (uint8_t*)target);
//        }
//        ++instrCount;
//        instrSource += info.length;
//        instrTarget += info.length + offset;
//        if (info.instrPointer >= detourPayloadSize)
//        {
//            if (bytesRead)
//            {
//                *bytesRead = (uint8_t)info.instrPointer;
//            }
//            if (bytesWritten)
//            {
//                *bytesWritten = (uint8_t)(instrTarget - (uint8_t*)target);
//            }
//            if (instructionCount)
//            {
//                *instructionCount = instrCount;
//            }
//            return ZYREX_ERROR_SUCCESS;
//        }
//    }
}

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyrexStatus ZyrexCreateTrampoline(const void* codeAddress, uint8_t detourPayloadSize, 
    const void* callbackAddress, const void** trampoline)
{
    if (!codeAddress  || (detourPayloadSize < 1) || !callbackAddress || !trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }

    size_t sourceBufferSize = ZYREX_TRAMPOLINE_RESTORE_SIZE;
    ZYREX_CHECK(ZyrexClipRegionSize(codeAddress, &sourceBufferSize));

    if (sourceBufferSize < detourPayloadSize)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }

    ZyrexTrampoline* trampolineData = NULL;

    // Search location in existing regions
    ZyrexTrampolineRegion* region = NULL;
    ZyrexTrampolineRegion* currentRegion = g_trampolineRegions;
    while (currentRegion)
    {
        if (currentRegion->freeTrampolineCount > 0)
        {
            const intptr_t currentDistance = 
                abs((int)((intptr_t)currentRegion - (intptr_t)codeAddress));
            if (currentDistance < 0x7FFFFFFF)
            {
                if (currentDistance < abs((int)((intptr_t)region - (intptr_t)codeAddress)))
                {
                    region = currentRegion;
                }
            }
        }
        currentRegion = currentRegion->next;
    };
    if (region)
    {
        trampolineData = region->freeTrampolines;

        // Enable write access to the trampoline region
        ZYREX_CHECK(ZyrexChangeRegionProtection(region, PAGE_EXECUTE_READWRITE));

        region->freeTrampolineCount--;
        region->freeTrampolines = region->freeTrampolines->next;
    } else
    {
        // Allocate a new trampoline region
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);

        void* memory;
        if (ZyrexAllocateTrampolineRegionFromLo((uintptr_t)codeAddress, 
            (uintptr_t)codeAddress + 0x7FFFFFFF, &memory) != ZYREX_ERROR_SUCCESS)
        {
            ZYREX_CHECK(ZyrexAllocateTrampolineRegionFromHi((uintptr_t)codeAddress,
                (uintptr_t)codeAddress - 0x7FFFFFFF, &memory));
        }

        region = (ZyrexTrampolineRegion*)memory;
        region->signature = ZYREX_TRAMPOLINE_REGION_SIGNATURE;
        region->next = g_trampolineRegions;
        region->freeTrampolines = NULL;
        region->freeTrampolineCount = 
            (systemInfo.dwAllocationGranularity / sizeof(ZyrexTrampoline)) - 2;
        ZyrexTrampoline* trampolines = (ZyrexTrampoline*)region + 1;
        ZyrexTrampoline* nextTrampoline = NULL;
        for (size_t i = region->freeTrampolineCount; i > 0; --i)
        {
            trampolines[i].next = nextTrampoline;
            nextTrampoline = &trampolines[i];
        }
        region->freeTrampolines = nextTrampoline;
        g_trampolineRegions = region;
        trampolineData = trampolines;
    }
    memset(trampolineData, 0xCC, sizeof(ZyrexTrampoline));

    ZyrexStatus status = ZyrexCopyInstructions(
        codeAddress,
        &trampolineData->code,
        (uint8_t)sourceBufferSize,
        ZYREX_TRAMPOLINE_CODE_SIZE,
        detourPayloadSize,
        &trampolineData->restoreSize,
        &trampolineData->codeSize,
        &trampolineData->instructionMap[0],
        ZYREX_TRAMPOLINE_INSTRUCTION_COUNT,
        &trampolineData->instructionCount);
    if (!ZYREX_SUCCESS(status))
    {
        ZyrexFreeTrampoline(trampolineData);
        return status;
    }
    memcpy(&trampolineData->restore, codeAddress, trampolineData->restoreSize);

    ZyrexWriteAbsoluteJump(&trampolineData->code[trampolineData->codeSize], 
        (uintptr_t)&trampolineData->backjumpAddress);
    ZyrexWriteAbsoluteJump(&trampolineData->callbackJump, 
        (uintptr_t)&trampolineData->callbackAddress);

    trampolineData->callbackAddress = callbackAddress;
    trampolineData->backjumpAddress = 
        (const void*)((uintptr_t)codeAddress + trampolineData->restoreSize);

    // Make trampoline region read-only
    ZYREX_CHECK(ZyrexChangeRegionProtection(region, PAGE_EXECUTE_READ));

    if (!FlushInstructionCache(GetCurrentProcess, &trampolineData->code, trampolineData->codeSize) 
        || !FlushInstructionCache(GetCurrentProcess, &trampolineData->callbackJump, 
            sizeof(&trampolineData->callbackJump)))
    {
        ZyrexFreeTrampoline(trampolineData);
        return ZYREX_ERROR_SYSTEMCALL;
    }

    *trampoline = trampolineData;
    return ZYREX_ERROR_SUCCESS;
}

ZyrexStatus ZyrexFreeTrampoline(const void* trampoline)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    ZyrexTrampolineRegion* region = (ZyrexTrampolineRegion*)((uintptr_t)trampoline 
        & ~((uintptr_t)systemInfo.dwAllocationGranularity - 1));
    if (region->signature != ZYREX_TRAMPOLINE_REGION_SIGNATURE)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    
    // Enable write access to the trampoline regions
    ZYREX_CHECK(ZyrexChangeRegionProtection(region, PAGE_EXECUTE_READWRITE));

    ZyrexTrampoline* trampolineData = (ZyrexTrampoline*)trampoline;
    trampolineData->next = region->freeTrampolines;
    region->freeTrampolines = trampolineData;
    region->freeTrampolineCount++;

    if (region->freeTrampolineCount == 
        (systemInfo.dwAllocationGranularity / sizeof(ZyrexTrampoline)) - 1)
    {
        if (g_trampolineRegions == region)
        {
            g_trampolineRegions = g_trampolineRegions->next;
        } else
        {
            ZyrexTrampolineRegion* currentRegion = g_trampolineRegions;
            while (currentRegion)
            {
                if (currentRegion->next == region)
                {
                    currentRegion->next = currentRegion->next->next;
                    break;
                }
                currentRegion = currentRegion->next;
            }
        }
        if (!VirtualFree(region, 0, MEM_RELEASE))
        {
            return ZYREX_ERROR_SYSTEMCALL;
        }
    } else
    {
        // Make trampoline regions read-only again
        ZYREX_CHECK(ZyrexChangeRegionProtection(region, PAGE_EXECUTE_READ));
    }

    return ZYREX_ERROR_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

ZyrexStatus ZyrexGetTrampolineInfo(const void* trampoline, ZyrexTrampolineInfo* info)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    const ZyrexTrampoline* trampolineData = (ZyrexTrampoline*)trampoline;
    info->addressOfCode = (uint8_t*)trampolineData->backjumpAddress - trampolineData->restoreSize;
    info->addressOfCallback = trampolineData->callbackAddress;
    info->addressOfCodeTrampoline = &trampolineData->code;
    info->addressOfCallbackTrampoline = &trampolineData->callbackJump;
    info->numberOfSavedInstructionBytes = trampolineData->restoreSize;
    return ZYREX_ERROR_SUCCESS;
}
/* ---------------------------------------------------------------------------------------------- */

ZyrexStatus ZyrexRestoreCodeFromTrampoline(const void* trampoline)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    const ZyrexTrampoline* trampolineData = (ZyrexTrampoline*)trampoline;
    void* codeAddress = (uint8_t*)trampolineData->backjumpAddress - trampolineData->restoreSize;
    DWORD oldProtection;
    if (!VirtualProtect(codeAddress, trampolineData->restoreSize, PAGE_EXECUTE_READWRITE, 
        &oldProtection))
    {
        return ZYREX_ERROR_SYSTEMCALL;
    }
    memcpy(codeAddress, &trampolineData->restore, trampolineData->restoreSize);
    if (!VirtualProtect(codeAddress, trampolineData->restoreSize, oldProtection, &oldProtection) ||
        !FlushInstructionCache(GetCurrentProcess, codeAddress, trampolineData->restoreSize))
    {
        return ZYREX_ERROR_SYSTEMCALL;
    }
    return ZYREX_ERROR_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

ZyrexStatus ZyrexInstructionAddressFromCode(const void* trampoline, 
    uintptr_t addressOfCodeInstruction, uintptr_t* addressOfTrampolineInstruction)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    ZyrexTrampoline* trampolineData = (ZyrexTrampoline*)trampoline;

    ZyrexInstructionMap map = trampolineData->instructionMap;
    for (unsigned i = 0; i < trampolineData->instructionCount; ++i)
    {
        if ((uintptr_t)trampolineData->backjumpAddress - trampolineData->restoreSize + 
            map->offsetCode == addressOfCodeInstruction)
        {
            *addressOfTrampolineInstruction = 
                (uintptr_t)trampolineData->code + map->offsetTrampoline;
            return ZYREX_ERROR_SUCCESS;
        }
    }

    return ZYREX_ERROR_NOT_FOUND;    
}

ZyrexStatus ZyrexInstructionAddressFromTrampoline(const void* trampoline, 
    uintptr_t addressOfTrampolineInstruction, uintptr_t* addressOfCodeInstruction)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    ZyrexTrampoline* trampolineData = (ZyrexTrampoline*)trampoline;

    if (addressOfTrampolineInstruction == (uintptr_t)&trampolineData->callbackJump)
    {
        *addressOfCodeInstruction = 
            (uintptr_t)trampolineData->backjumpAddress - trampolineData->restoreSize;
        return ZYREX_ERROR_SUCCESS;
    }

    ZyrexInstructionMap map = trampolineData->instructionMap;
    for (unsigned i = 0; i < trampolineData->instructionCount; ++i)
    {
        if ((uintptr_t)&trampolineData->code + map->offsetTrampoline == 
            addressOfTrampolineInstruction)
        {
            *addressOfCodeInstruction = 
                (uintptr_t)trampolineData->backjumpAddress - trampolineData->restoreSize + 
                    map->offsetCode;
            return ZYREX_ERROR_SUCCESS;
        }
    }

    return ZYREX_ERROR_NOT_FOUND;
}

ZyrexStatus ZyrexCodeOffsetToTrampolineOffset(const void* trampoline, uint8_t codeOffset,
    uint8_t* trampolineOffset)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    ZyrexInstructionMap map = ((ZyrexTrampoline*)trampoline)->instructionMap;
    for (unsigned i = 0; i < ((ZyrexTrampoline*)trampoline)->instructionCount; ++i)
    {
        if (map->offsetCode == codeOffset)
        {
            *trampolineOffset = map->offsetTrampoline;
            return ZYREX_ERROR_SUCCESS;
        }
    }
    return ZYREX_ERROR_NOT_FOUND;
}

ZyrexStatus ZyrexTrampolineOffsetToCodeOffset(const void* trampoline, uint8_t trampolineOffset,
    uint8_t* codeOffset)
{
    if (!trampoline)
    {
        return ZYREX_ERROR_INVALID_PARAMETER;
    }
    ZyrexTrampoline* trampolineData = (ZyrexTrampoline*)trampoline;

    ZyrexInstructionMap map = trampolineData->instructionMap;
    for (unsigned i = 0; i < trampolineData->instructionCount; ++i)
    {
        if (map->offsetTrampoline == trampolineOffset)
        {
            *codeOffset = map->offsetCode;
            return ZYREX_ERROR_SUCCESS;
        }
    }
    return ZYREX_ERROR_NOT_FOUND;
}

/* ============================================================================================== */
