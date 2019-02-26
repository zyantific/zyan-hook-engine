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

#ifndef ZYREX_TRAMPOLINE_H
#define ZYREX_TRAMPOLINE_H

#include <Zycore/Types.h>
#include <Zyrex/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * @brief   Defines the `ZyrexTrampoline` struct.
 */
typedef struct ZyrexTrampoline_
{
    /**
     * @brief   The address of the hooked function.
     */
    const void* address_of_code;
    /**
     * @brief   The address of the callback function.
     */
    const void* address_of_callback;
    /**
     * @brief   A pointer to the first instruction of the trampoline.
     */
    const void* address_of_trampoline_code;
    /**
     * @brief   The number of instruction bytes saved from the hooked function.
     */
    ZyanU8 saved_instruction_bytes;
} ZyrexTrampoline;

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/**
 * @brief   Creates a new trampoline.
 *
 * @param   address     The address of the function to create the trampoline for.
 * @param   size        Specifies the minimum amount of instruction bytes that need to be saved
 *                      to the trampoline (usually equals the amount of bytes overwritten by the
 *                      branch instruction).
 *                      This function might copy more bytes on demand to keep instructions intact.
 * @param   callback    The callback address.
 * @param   trampoline  Receives basic information about the newly created trampoline.
 *
 * @return  A zyan status code.
 */
ZyanStatus ZyrexTrampolineCreate(const void* address, ZyanUSize size, const void* callback,
    const ZyrexTrampoline* trampoline);

/**
 * @brief   Destroys the given trampoline.
 *
 * @param   trampoline  The trampoline.
 *
 * @return  A zyan status code.
 */
ZyanStatus ZyrexTrampolineFree(const ZyrexTrampoline* trampoline);

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif
#endif /* ZYREX_TRAMPOLINE_H */
