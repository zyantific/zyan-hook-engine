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

#ifndef ZYREX_UTILS_H
#define ZYREX_UTILS_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>

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
    *(ZyanI32*)(instr) =
        (ZyanI32)(destination - ((ZyanUPointer)address + ZYREX_SIZEOF_RELATIVE_JUMP));
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
#if defined(ZYREX_X64)
    *(ZyanI32*)(instr) =
        (ZyanI32)(destination - ((ZyanUPointer)address + ZYREX_SIZEOF_ABSOLUTE_JUMP));
#else
    *(ZyanU32*)(instr) = (ZyanU32)destination;
#endif
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif
#endif /* ZYREX_UTILS_H */
