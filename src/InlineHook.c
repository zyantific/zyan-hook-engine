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

#include <Zycore/LibC.h>
#include <Zyrex/Internal/InlineHook.h>
#include <Zyrex/Internal/Utils.h>

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Runtime thread migration                                                                       */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexMigrateThread(ZyanThreadId thread_id, const void* source, ZyanUSize source_length,
    const void* destination, ZyanUSize destination_length,
    const ZyrexInstructionTranslationMap* translation_map, ZyrexThreadMigrationDirection direction)
{
    ZYAN_UNUSED(destination_length);

    ZYAN_ASSERT(source);
    ZYAN_ASSERT(source_length);
    ZYAN_ASSERT(destination);
    ZYAN_ASSERT(translation_map);

    // The thread is already suspended by the caller (the transaction). Read its instruction pointer.
    ZyanUPointer ip;
    ZYAN_CHECK(ZyanThreadGetInstructionPointer(thread_id, &ip));

    // Nothing to migrate unless the thread is executing inside the source code range.
    if ((ip < (ZyanUPointer)source) || (ip >= (ZyanUPointer)source + source_length))
    {
        return ZYAN_STATUS_SUCCESS;
    }

    const ZyanU8 offset = (ZyanU8)(ip - (ZyanUPointer)source);
    for (ZyanUSize i = 0; i < translation_map->count; ++i)
    {
        switch (direction)
        {
        case ZYREX_THREAD_MIGRATION_DIRECTION_SRC_DST:
            if (translation_map->items[i].offset_source == offset)
            {
                return ZyanThreadSetInstructionPointer(thread_id,
                    (ZyanUPointer)destination + translation_map->items[i].offset_destination);
            }
            break;
        case ZYREX_THREAD_MIGRATION_DIRECTION_DST_SRC:
            if (translation_map->items[i].offset_destination == offset)
            {
                return ZyanThreadSetInstructionPointer(thread_id,
                    (ZyanUPointer)destination + translation_map->items[i].offset_source);
            }
            break;
        default:
            ZYAN_UNREACHABLE;
        }
    }

    // The instruction pointer is inside the range but not at a mapped instruction boundary; leave
    // it unchanged rather than moving it to an unrelated offset.
    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Attaching and detaching                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
