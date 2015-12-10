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

#ifndef _ZYREX_TRAMPOLINE_H_
#define _ZYREX_TRAMPOLINE_H_

#include <stdint.h>
#include <Zyrex/Status.h>

/* ============================================================================================== */
/* Structs                                                                                        */
/* ============================================================================================== */

typedef struct ZyrexTrampolineInfo
{
    const void* addressOfCode;
    const void* addressOfCallback;
    const void* addressOfCodeTrampoline;
    const void* addressOfCallbackTrampoline;
    uint8_t numberOfSavedInstructionBytes;
} ZyrexTrampolineInfo;

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/**
 * @brief   Creates a new trampoline.
 *
 * @param   codeAddress         The code address.
 * @param   detourPayloadSize   The size of the detour payload. This argument specifies the minimum 
 *                              amount of instruction bytes that needs to be saved to the 
 *                              trampoline.
 * @param   callbackAddress     The callback address.
 * @param   trampoline          Receives a pointer to the newly created trampoline.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZyrexStatus ZyrexCreateTrampoline(const void* codeAddress, uint8_t detourPayloadSize, 
    const void* callbackAddress, const void** trampoline);

/**
 * @brief   Destroys the given trampoline.
 *
 * @param   trampoline  The trampoline.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZyrexStatus ZyrexFreeTrampoline(const void* trampoline);

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Queries basic information about the given @c trampoline.
 *
 * @param   trampoline  The trampoline.
 * @param   info        Pointer to the @c ZyrexTrampolineInfo struct that receives the trampoline 
 *                      information.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZyrexStatus ZyrexGetTrampolineInfo(const void* trampoline, ZyrexTrampolineInfo* info);

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Restores all saved instructions from the given @c trampoline and flushes the instruction 
 *          cache.
 *
 * @param   trampoline  The trampoline.
 * @param   codeAddress The code address.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZyrexStatus ZyrexRestoreCodeFromTrampoline(const void* trampoline);

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Translates the given instruction address to the corresponding address in the trampoline.
 *
 * @param   trampoline                      The trampoline.
 * @param   addressOfCodeInstruction        The address of the instruction.
 * @param   addressOfTrampolineInstruction  Receives the address of the corresponding instruction
 *                                          in the trampoline.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZyrexStatus ZyrexInstructionAddressFromCode(const void* trampoline, 
    uintptr_t addressOfCodeInstruction, uintptr_t* addressOfTrampolineInstruction);

/**
 * @brief   Translates the given instruction address to the corresponding address in the code.
 *
 * @param   trampoline                      The trampoline.
 * @param   addressOfTrampolineInstruction  The address of the instruction.
 * @param   addressOfCodeInstruction        Receives the address of the corresponding instruction
 *                                          in the code.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZyrexStatus ZyrexInstructionAddressFromTrampoline(const void* trampoline, 
    uintptr_t addressOfTrampolineInstruction, uintptr_t* addressOfCodeInstruction);

/* ============================================================================================== */

#endif /*_ZYREX_TRAMPOLINE_H_ */
