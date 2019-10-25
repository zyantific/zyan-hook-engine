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
#include <Zyrex/Internal/Utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Constants                                                                                      */
/* ============================================================================================== */

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
 * @brief   Defines an additional amount of bytes to reserve in the trampoline code buffer which
 *          is required in order to rewrite certain kinds of instructions.
 */
#define ZYREX_TRAMPOLINE_MAX_CODE_SIZE_BONUS \
    8

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
 * @brief   Defines an additional amount of slots to reserve in the instruction translation map
 *          which is required in order to rewrite certain kinds of instructions.
 */
#define ZYREX_TRAMPOLINE_MAX_INSTRUCTION_COUNT_BONUS \
    2

/**
 * @brief   Defines the trampoline region signature.
 */
#define ZYREX_TRAMPOLINE_REGION_SIGNATURE   'zrex'

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Translation map                                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZyrexInstructionTranslationItem` struct.
 */
typedef struct ZyrexInstructionTranslationItem_
{
    /**
     * @brief   The offset of a single instruction relative to the beginning of the source buffer.
     */
    ZyanU8 offset_source;
    /**
     * @brief   The offset of a single instruction relative to the beginning of the destination
     *          buffer.
     */
    ZyanU8 offset_destination;
} ZyrexInstructionTranslationItem;

/**
 * @brief   Defines the `ZyrexInstructionTranslationMap` struct.
 */
typedef struct ZyrexInstructionTranslationMap_
{
    /**
     * @brief   The number of items in the translation map.
     */
    ZyanU8 count;
    /**
     * @brief   The translation items.
     */
    ZyrexInstructionTranslationItem items[ZYREX_TRAMPOLINE_MAX_INSTRUCTION_COUNT + 
                                          ZYREX_TRAMPOLINE_MAX_INSTRUCTION_COUNT_BONUS];
} ZyrexInstructionTranslationMap;

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

#ifdef ZYAN_X64
    /**
     * @brief   The address of the jump to the callback function.
     */
    const void* address_of_callback_jump;
#endif

    /**
     * @brief   A pointer to the first instruction of the trampoline code.
     */
    const void* address_of_trampoline_code;
    /**
     * @brief   A pointer to the first instruction of the saved original code instructions.
     */
    const void* address_of_original_code;
    /**
     * @brief   The number of instruction bytes in the trampoline code buffer.
     */
    ZyanU8 size_of_trampoline_code;
    /**
     * @brief   The number of instruction bytes saved from the hooked function.
     */
    ZyanU8 size_of_original_code;
    /**
     * @brief   A pointer to the instruction translation map.
     */
    const ZyrexInstructionTranslationMap* translation_map;
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
