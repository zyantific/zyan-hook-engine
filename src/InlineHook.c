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

#include <windows.h>
#include <Zyrex/Internal/InlineHook.h>
#include <Zyrex/Internal/Utils.h>

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Attaching and detaching                                                                        */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexAttachInlineHook(void* address, const void* callback)
{
    // TODO: Use platform independent APIs

    // Make 
    DWORD old_protect;
    if (!VirtualProtect((LPVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP, PAGE_EXECUTE_READWRITE, 
        &old_protect))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    ZyrexWriteRelativeJump(address, (ZyanUPointer)callback);

    if (!VirtualProtect((LPVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP, old_protect, &old_protect))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), (LPCVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
