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

#ifndef ZYREX_INTERNAL_UTILS_H
#define ZYREX_INTERNAL_UTILS_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/Zydis.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Macros                                                                                         */
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

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Utility functions                                                                              */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Calculates a relative offset from the given `source` to the given `destination`
 *          address.
 *
 * @param   instruction_length  The length of the instruction the offset is calculated for.
 * @param   source_address      The source address of the instruction.
 * @param   destination_address The destination address.
 *
 * @return  The relative offset from the given `source` to the given `destination` address.
 */
ZYAN_INLINE ZyanI32 ZyrexCalculateRelativeOffset(ZyanU8 instruction_length, 
    ZyanUPointer source_address, ZyanUPointer destination_address)
{
    return (ZyanI32)(destination_address - source_address - instruction_length);    
}

/* ---------------------------------------------------------------------------------------------- */
/* Jumps                                                                                          */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Writes a relative jump instruction at the given `address`.
 *
 * @param   address     The jump address.
 * @param   destination The absolute destination address of the jump.
 *
 * This function does not perform any checks, like if the target destination is within the range
 * of a relative jump.
 */
ZYAN_INLINE void ZyrexWriteRelativeJump(void* address, ZyanUPointer destination)
{
    ZyanU8* instr = (ZyanU8*)address;

    *instr++ = 0xE9;
    *(ZyanI32*)(instr) = ZyrexCalculateRelativeOffset(ZYREX_SIZEOF_RELATIVE_JUMP, 
        (ZyanUPointer)address, destination);
}

/**
 * @brief	Writes an absolute indirect jump instruction at the given `address`.
 *
 * @param   address     The jump address.
 * @param   destination The memory address that contains the absolute destination for the jump.
 */
ZYAN_INLINE void ZyrexWriteAbsoluteJump(void* address, ZyanUPointer destination)
{
    ZyanU16* instr = (ZyanU16*)address;

    *instr++ = 0x25FF;
#if defined(ZYAN_X64)
    *(ZyanI32*)(instr) = ZyrexCalculateRelativeOffset(ZYREX_SIZEOF_ABSOLUTE_JUMP, 
        (ZyanUPointer)address, destination);
#else
    *(ZyanU32*)(instr) = (ZyanU32)destination;
#endif
}

/* ---------------------------------------------------------------------------------------------- */
/* Instruction decoding                                                                           */
/* ---------------------------------------------------------------------------------------------- */

// TODO: Integrate this function in Zydis `ZyrexCalcAbsoluteAddressRaw`

/**
 * @brief   Calculates the absolute target address value for a relative-branch instruction
 *          or an instruction with `EIP/RIP`-relative memory operand.
 *
 * @param   instruction     A pointer to the `ZydisDecodedInstruction` struct.
 * @param   runtime_address The runtime address of the instruction.
 * @param   result_address  A pointer to the memory that receives the absolute address.
 *
 * @return  A zyan status code.
 */
ZYAN_INLINE ZyanStatus ZyrexCalcAbsoluteAddress(const ZydisDecodedInstruction* instruction,
    ZyanU64 runtime_address, ZyanU64* result_address)
{
    ZYAN_ASSERT(instruction);
    ZYAN_ASSERT(result_address);
    ZYAN_ASSERT(instruction->attributes & ZYDIS_ATTRIB_IS_RELATIVE);

    // Instruction with EIP/RIP-relative memory operand
    if ((instruction->attributes & ZYDIS_ATTRIB_HAS_MODRM) &&
        (instruction->raw.modrm.mod == 0) &&
        (instruction->raw.modrm.rm == 5))
    {
        if (instruction->address_width == 32)
        {
            *result_address = ((ZyanU32)runtime_address + instruction->length +
                (ZyanU32)instruction->raw.disp.value);

            return ZYAN_STATUS_SUCCESS;
        }
        if (instruction->address_width == 64)
        {
            *result_address = (ZyanU64)(runtime_address + instruction->length +
                instruction->raw.disp.value);

            return ZYAN_STATUS_SUCCESS;
        }
    }

    // Relative branch instruction
    if (instruction->raw.imm[0].is_signed && instruction->raw.imm[0].is_relative)
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

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_INTERNAL_UTILS_H */
