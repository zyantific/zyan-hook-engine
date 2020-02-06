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

ZyanStatus ZyrexMigrateThread(DWORD thread_id, const void* source, ZyanUSize source_length, 
    const void* destination, ZyanUSize destination_length, 
    const ZyrexInstructionTranslationMap* translation_map)
{
    ZYAN_UNUSED(destination_length);

    ZyanStatus status = ZYAN_STATUS_SUCCESS;

    HANDLE const handle = 
        OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, 
            (BOOL)ZYAN_FALSE, thread_id);
    if (!handle)
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;    
    }

    const DWORD suspend_count = SuspendThread(handle);
    if (suspend_count == (DWORD)(-1))
    {
        CloseHandle(handle);
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    CONTEXT context = { 0 };
    //ZYAN_MEMSET(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(handle, &context))
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
        if (translation_map->items[i].offset_source == (ZyanU8)source_offset)
        {
#if defined(ZYAN_X64)
    context.Rip = (ZyanUPointer)destination + translation_map->items[i].offset_destination;
#elif defined(ZYAN_X86)
    context.Eip = (ZyanUPointer)destination + translation_map->items[i].offset_destination;
#else
#   error "Unsupported architecture detected"
#endif

            if (!SetThreadContext(handle, &context))
            {
                status = ZYAN_STATUS_BAD_SYSTEMCALL;
            }

            goto CleanupAndResume;
        }
    }

    // This should never happen
    ZYAN_UNREACHABLE;

CleanupAndResume:
    while (ZYAN_TRUE)
    {
        const DWORD value = ResumeThread(handle);
        if (value == (DWORD)(-1))
        {
            CloseHandle(handle);
            return ZYAN_STATUS_BAD_SYSTEMCALL;
        }
        if (value <= suspend_count + 1)
        {
            break;
        }
    }

    if (!CloseHandle(handle))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return status;
}

#endif

/* ---------------------------------------------------------------------------------------------- */
/* Attaching and detaching                                                                        */
/* ---------------------------------------------------------------------------------------------- */


/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
