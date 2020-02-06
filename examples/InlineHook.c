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

/**
 * @file
 * @brief   Demonstrates the inline-hook.
 */

#include <Zycore/Defines.h>
#include <Zyrex/Zyrex.h>
#include <Zyrex/Internal/InlineHook.h>
#include <Zyrex/Internal/Trampoline.h>
#include <stdio.h>
#include "Zyrex/Transaction.h"
#include <stdint.h>

#include <Windows.h>

/* ============================================================================================== */
/* Entry point                                                                                    */
/* ============================================================================================== */

typedef DWORD (__stdcall functype)(void* param);

static functype* volatile original = NULL;

DWORD __stdcall xxx2(void* param)
{
    ZYAN_UNUSED(param);

    puts("hello from original\n");

    int v = 0;
    for (int i = 0; i < 3; ++i)
    {
        v++;
    }
    return 0x1337;
}

DWORD __stdcall callback(void* param)
{
    ZYAN_UNUSED(param);

    puts("hello from callback\n");

    return (*original)(NULL) + 1;
}

typedef BOOL (WINAPI FuncCopyFileW)(_In_ LPCWSTR lpExistingFileName, _In_ LPCWSTR lpNewFileName,
    _In_ BOOL bFailIfExists);

static FuncCopyFileW* volatile CopyFileWOriginal = ZYAN_NULL;

BOOL WINAPI CopyFileWCallback(_In_ LPCWSTR lpExistingFileName, _In_ LPCWSTR lpNewFileName,
    _In_ BOOL bFailIfExists)
{
    ZYAN_UNUSED(lpExistingFileName);
    ZYAN_UNUSED(lpNewFileName);
    ZYAN_UNUSED(bFailIfExists);

    puts("CopyFileW callback");

    const BOOL result = CopyFileWOriginal(lpExistingFileName, lpNewFileName, bFailIfExists);
    const DWORD error = GetLastError();
    ZYAN_UNUSED(error);

    return result;
}

int main()
{

    ZyrexInitialize();

    ZyrexTransactionBegin();
    ZyrexInstallInlineHook((void*)&xxx2, (const void*)&callback, (ZyanConstVoidPointer*)&original);
    /*ZyrexInstallInlineHook((void*)&CopyFileW, (const void*)&CopyFileWCallback, 
        (ZyanConstVoidPointer*)&CopyFileWOriginal);*/
    ZyrexTransactionCommit();
    printf("%x\n", (unsigned int)xxx2(0));

    ZyrexTransactionBegin();
    ZyrexRemoveInlineHook((ZyanConstVoidPointer*)&original);
    ZyrexTransactionCommit();
    printf("%x\n", (unsigned int)xxx2(0));


    //ZyanU8 buffer[] = 
    //{                                 // E1, E2
    //     /*0xEB, 0xFE, */0x75, 0x02, 0xeb, 0xfb, 0x67, 0xE3, 0xf8, 0x48, 0x8B, 0x05, 0xF5, 0xFF, 0xFF, 0xFF, 0x90, 0x90, 0x90, 0xC3
    //};
    //void* const buf = VirtualAlloc(NULL, sizeof(buffer), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    //memcpy(buf, buffer, sizeof(buffer));

    //functype* const xxx = (functype*)buf;


    //ZyrexTrampolineChunk* trampoline;
    //const ZyanStatus status =
    //    ZyrexTrampolineCreate((const void*)xxx, (const void*)&callback, 5, &trampoline);
    //if (ZYAN_SUCCESS(status))
    //{
    //    original = (functype*)&trampoline->code_buffer;

    //    printf("%.8X", ((functype*)original)(NULL));

    //    ZyrexTrampolineFree(trampoline);
    //}

    return 0;
}

/* ============================================================================================== */
