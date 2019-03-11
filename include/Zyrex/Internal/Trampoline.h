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

/* ---------------------------------------------------------------------------------------------- */
/* Flags                                                                                          */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZyrexTrampolineFlags` datatype.
 */
typedef ZyanU8 ZyrexTrampolineFlags;

/**
 * @brief   Controls rewriting of the `CALL` instruction.
 *
 * If a `CALL` instruction needs to be saved to the trampoline and this flag is passed, the code
 * will be transformed into a `PUSH JMP` combination with the backjump address as destination.
 *
 * This allows the hook to be safely removed in all situations, as the codeflow will never return
 * to the trampoline.
 */
#define ZYREX_TRAMPOLINE_FLAG_REWRITE_CALL  0x01

/**
 * @brief   Controls rewriting of the `JCX/JECXZ/JRCXZ` instruction.
 *
 * If a `JCX/JECXZ/JRCXZ` instruction needs to be saved to the trampoline and this flag is
 * passed, the code will be transformed into equivalent instructions during relocation.
 *
 * This is required because no `rel32` version of this instruction exists in the x86/x86-64 ISA
 * and the `rel8` version can't reach its target after relocating the code.
 */
#define ZYREX_TRAMPOLINE_FLAG_REWRITE_JCXZ  0x02

/**
 * @brief   Controls rewriting of the `LOOP/LOOPE/LOOPNE` instruction.
 *
 * If a `LOOP/LOOPE/LOOPNE` instruction needs to be saved to the trampoline and this flag is
 * passed, the code will be transformed into equivalent instructions during relocation.
 *
 * This is required because no `rel32` version of this instruction exists in the x86/x86-64 ISA
 * and the `rel8` version can't reach its target after relocating the code.
 */
#define ZYREX_TRAMPOLINE_FLAG_REWRITE_LOOP  0x04

/* ---------------------------------------------------------------------------------------------- */
/* Trampoline                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/**
 * @brief   Creates a new trampoline.
 *
 * @param   address             The address of the function to create the trampoline for.
 * @param   callback            The address of the callback function the hook will redirect to.
 * @param   min_bytes_to_reloc  Specifies the minimum amount of  bytes that need to be relocated
 *                              to the trampoline (usually equals the size of the branch
 *                              instruction used for hooking).
 *                              This function might copy more bytes on demand to keep individual
 *                              instructions intact.
 * @param   trampoline          Receives information about the newly created trampoline.
 *
 * @return  A zyan status code.
 */
ZyanStatus ZyrexTrampolineCreate(const void* address, const void* callback,
    ZyanUSize min_bytes_to_reloc, ZyrexTrampoline* trampoline);

/**
 * @brief   Creates a new trampoline.
 *
 * @param   address             The address of the function to create the trampoline for.
 * @param   callback            The address of the callback function the hook will redirect to.
 * @param   min_bytes_to_reloc  Specifies the minimum amount of  bytes that need to be relocated
 *                              to the trampoline (usually equals the size of the branch
 *                              instruction used for hooking).
 *                              This function might copy more bytes on demand to keep individual
 *                              instructions intact.
 * @param   flags               Trampoline creation flags.
 * @param   trampoline          Receives information about the newly created trampoline.
 *
 * @return  A zyan status code.
 */
ZyanStatus ZyrexTrampolineCreateEx(const void* address, const void* callback,
    ZyanUSize min_bytes_to_reloc, ZyrexTrampolineFlags flags, ZyrexTrampoline* trampoline);

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
