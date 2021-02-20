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

#ifndef ZYREX_INLINE_HOOK_H
#define ZYREX_INLINE_HOOK_H

#include <Zycore/Defines.h>
#ifdef ZYAN_WINDOWS
#   include <windows.h>
#endif
#include <Zycore/Types.h>
#include <Zyrex/Internal/Trampoline.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

typedef enum ZyrexThreadMigrationDirection_
{
    /**
     * @brief   Uses the 'source' offsets in the translation map to map them to the 'destination'
     *          offsets.
     */
    ZYREX_THREAD_MIGRATION_DIRECTION_SRC_DST,
    /**
     * @brief   Uses the 'destination' offsets in the translation map to map them to the 'source'
     *          offsets.
     */
    ZYREX_THREAD_MIGRATION_DIRECTION_DST_SRC,
} ZyrexThreadMigrationDirection;

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Attaching and detaching                                                                        */
/* ---------------------------------------------------------------------------------------------- */

#ifdef ZYAN_WINDOWS

ZyanStatus ZyrexMigrateThread(HANDLE thread_handle, const void* source, ZyanUSize source_length,
    const void* destination, ZyanUSize destination_length,
    const ZyrexInstructionTranslationMap* translation_map, 
    ZyrexThreadMigrationDirection direction);

#endif

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_INLINE_HOOK_H */
