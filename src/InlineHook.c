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

#ifdef ZYAN_WINDOWS

ZyanStatus ZyrexMigrateThread(HANDLE thread_handle, const void* source, ZyanUSize source_length,
    const void* destination, ZyanUSize destination_length,
    const ZyrexInstructionTranslationMap* translation_map,
    ZyrexThreadMigrationDirection direction)
{
    ZYAN_UNUSED(destination_length);

    ZYAN_ASSERT(thread_handle);
    ZYAN_ASSERT(source);
    ZYAN_ASSERT(source_length);
    ZYAN_ASSERT(destination);
    ZYAN_ASSERT(destination_length);
    ZYAN_ASSERT(translation_map);

    ZyanStatus status = ZYAN_STATUS_SUCCESS;

    const DWORD suspend_count = SuspendThread(thread_handle);
    if (suspend_count == (DWORD)(-1))
    {
        CloseHandle(thread_handle);
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    CONTEXT context = { 0 };
    //ZYAN_MEMSET(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(thread_handle, &context))
    {
        status = ZYAN_STATUS_BAD_SYSTEMCALL;
        goto CleanupAndResume;
    }

#if defined(ZYAN_X64)
    const ZyanUPointer current_ip = context.Rip;
#elif defined(ZYAN_X86)
    const ZyanUPointer current_ip = context.Eip;
#else
#   error "Unsupported architecture detected"
#endif
    if ((current_ip < (ZyanUPointer)source) || (current_ip > (ZyanUPointer)source + source_length))
    {
        goto CleanupAndResume;
    }

    const ZyanUPointer source_offset = (ZyanUPointer)source - current_ip;
    for (ZyanUSize i = 0; i < translation_map->count; ++i)
    {
        switch (direction)
        {
        case ZYREX_THREAD_MIGRATION_DIRECTION_SRC_DST:
            if (translation_map->items[i].offset_source == (ZyanU8)source_offset)
            {
#if defined(ZYAN_X64)
                context.Rip = (ZyanUPointer)destination + translation_map->items[i].offset_destination;
#elif defined(ZYAN_X86)
                context.Eip = (ZyanUPointer)destination + translation_map->items[i].offset_destination;
#else
#   error "Unsupported architecture detected"
#endif

                if (!SetThreadContext(thread_handle, &context))
                {
                    status = ZYAN_STATUS_BAD_SYSTEMCALL;
                }

                goto CleanupAndResume;
            }
            break;
        case ZYREX_THREAD_MIGRATION_DIRECTION_DST_SRC:
            if (translation_map->items[i].offset_destination == (ZyanU8)source_offset)
            {
#if defined(ZYAN_X64)
                context.Rip = (ZyanUPointer)destination + translation_map->items[i].offset_source;
#elif defined(ZYAN_X86)
                context.Eip = (ZyanUPointer)destination + translation_map->items[i].offset_source;
#else
#   error "Unsupported architecture detected"
#endif

                if (!SetThreadContext(thread_handle, &context))
                {
                    status = ZYAN_STATUS_BAD_SYSTEMCALL;
                }

                goto CleanupAndResume;
            }
            break;
        default:
            ZYAN_UNREACHABLE;
        }
    }

    // This should never happen
    ZYAN_UNREACHABLE;

CleanupAndResume:
    while (ZYAN_TRUE)
    {
        const DWORD value = ResumeThread(thread_handle);
        if (value == (DWORD)(-1))
        {
            CloseHandle(thread_handle);
            return ZYAN_STATUS_BAD_SYSTEMCALL;
        }
        if (value <= suspend_count + 1)
        {
            break;
        }
    }

    return status;
}

#endif

/* ---------------------------------------------------------------------------------------------- */
/* Attaching and detaching                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
