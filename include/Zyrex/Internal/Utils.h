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
/* Jumps                                                                                          */
/* ============================================================================================== */

    #define ZYREX_SIZEOF_JUMP_RELATIVE 5
#define ZYREX_SIZEOF_JUMP_ABSOLUTE 6

/**
 * @brief   Writes a relative jump at the given `address`.
 *
 * @param   address     The memory address.
 * @param   destination The absolute destination address of the jump.
 *
 * @return  Returns the number of bytes written.
 *
 * This function does not perform any checks, like if the target destination is within the range
 * of a relative jump.
 */
ZYAN_INLINE ZyanU8 ZyrexWriteRelativeJump(void* address, ZyanUPointer destination)
{
#pragma pack(push, 1)
    /**
     * @brief   Represents the assembly equivalent of a 5 byte relative 32 bit jump.
     */
    typedef struct ZyrexJumpRelative32_
    {
        ZyanU8 opcode;
        ZyanI32 distance;
    } ZyrexJumpRelative32;
#pragma pack(pop)

    ZYAN_STATIC_ASSERT(sizeof(ZyrexJumpRelative32) == 5);

    ZyrexJumpRelative32* jump = (ZyrexJumpRelative32*)address;
    jump->opcode   = 0xE9;
    jump->distance =
        (ZyanI32)(destination - ((ZyanUPointer)address + sizeof(ZyrexJumpRelative32)));
}

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief	Writes an absolute indirect jump instruction at the given `address`.
 *
 * @param   address     The memory address.
 * @param   destination The memory address that contains the absolute destination for the jump.
 */
ZYAN_INLINE void ZyrexWriteAbsoluteJump(void* address, ZyanUPointer destination)
{
#pragma pack(push, 1)
    /**
     * @brief   Represents the assembly equivalent of an absolute jump.
     */
    typedef struct ZyrexJumpAbsolute_
    {
        ZyanU16 opcode;
        ZyanI32 address;
    } ZyrexJumpAbsolute;
#pragma pack(pop)

    ZYAN_STATIC_ASSERT(sizeof(ZyrexJumpAbsolute) == 6);

    ZyrexJumpAbsolute* jump = (ZyrexJumpAbsolute*)address;
    jump->opcode = 0x25FF;
#ifdef ZYREX_X86
    jump->address = destination;
#else
    jump->address = (int32_t)(destination - ((uintptr_t)address + 6));
#endif
}

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif
#endif /* ZYREX_UTILS_H */
