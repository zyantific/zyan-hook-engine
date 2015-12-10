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

#ifndef _ZYREX_UTILS_H_
#define _ZYREX_UTILS_H_

#include <stdint.h>
#include <string.h>
#include <Zyrex/Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Jumps                                                                                          */
/* ============================================================================================== */

/**
 * @brief   Defines the size of a 32 bit relative jump.
 */
#define ZYREX_SIZEOF_JUMP_RELATIVE32 5

/**
 * @brief   Defines the size of an absolute jump.
 */
#define ZYREX_SIZEOF_JUMP_ABSOLUTE   6

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Writes a 32 bit relative jump instruction at the given @c address.
 *
 * @param   address     The address of the jump instruction.
 * @param   destination The desired destination address.
 *                      
 * This function does not perform any checks, like if the target destination is within the range 
 * of a 32 bit relative jump.
 */
ZYREX_INLINE void ZyrexWriteJumpRelative32(void* address, uintptr_t destination)
{
#pragma pack(push, 1)
    /**
     * @brief   Represents the assembly equivalent of a 5 byte relative 32 bit jump.
     */
    typedef struct ZyrexJumpRelative32
    {
        uint8_t opcodeJMP;
        int32_t distance;
    } ZyrexJumpRelative32;
#pragma pack(pop)
    ZyrexJumpRelative32* jump = (ZyrexJumpRelative32*)address;
    jump->opcodeJMP = 0xE9;
    jump->distance = (int32_t)(destination - ((uintptr_t)address + 5));
}

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief	Writes an absolute jump instruction at the given @c address.
 *          
 * @param   address     The address of the jump instruction.
 * @param   destination The memory address that contains the absolute destination for the jump.
 */
ZYREX_INLINE void ZyrexWriteAbsoluteJump(void* address, uintptr_t destination)
{
#pragma pack(push, 1)
    /**
     * @brief   Represents the assembly equivalent of an absolute jump.
     */
    typedef struct ZyrexJumpAbsolute
    {
        uint8_t opcodeJMP[2];
        int32_t m_offset;
    } ZyrexJumpAbsolute;
#pragma pack(pop)
    ZyrexJumpAbsolute* jump = (ZyrexJumpAbsolute*)address;
    jump->opcodeJMP[0] = 0xFF; 
    jump->opcodeJMP[1] = 0x25;
#ifdef ZYREX_X86
    jump->m_offset = destination;
#else
    jump->m_offset = (int32_t)(destination - ((uintptr_t)address + 6));
#endif
}

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif
#endif /*_ZYREX_UTILS_H_ */
