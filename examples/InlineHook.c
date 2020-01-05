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

#include <Zyrex/Zyrex.h>
#include <Zyrex/Internal/InlineHook.h>
#include <Zyrex/Internal/Trampoline.h>
#include <stdio.h>
#include <Windows.h>
#include "Zyrex/Transaction.h"
#include <stdint.h>

/* ============================================================================================== */
/* Entry point                                                                                    */
/* ============================================================================================== */

typedef DWORD (__stdcall *functype)(void* lpThreadParameter);

const void* original = NULL;

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

    return ((functype)original)(NULL) + 1;
}

int main()
{
    ZyrexInitialize();

    ZyrexTransactionBegin();
    ZyrexInstallInlineHook((void*)&xxx2, (const void*)&callback, &original);
    int x = xxx2(0);
    printf("%x\n", x);
    ZyrexTransactionCommit();


    ZyrexTransactionBegin();
    ZyrexRemoveInlineHook((void*)&xxx2, &original);
    int z = xxx2(0);
    printf("%x\n", z);
    ZyrexTransactionCommit();
    

    //ZyrexTransactionAbort();

//    ZyanU8 buffer[] = 
//    {                                 // E1, E2
//         0xEB, 0xFE, 0x75, 0x02, 0xeb, 0xfb, 0x67, 0xE3, 0xf8, 0x48, 0x8B, 0x05, 0xF5, 0xFF, 0xFF, 0xFF, 0x90, 0x90, 0x90, 0xC3
//    };
//    void* const buf = VirtualAlloc(NULL, sizeof(buffer), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
//    memcpy(buf, buffer, sizeof(buffer));
//
//    const functype xxx = xxx2;// (functype)buf;
//
//
//    ZyrexTrampolineChunk* trampoline;
//    const ZyanStatus status =
//        ZyrexTrampolineCreate((const void*)xxx, (const void*)&callback, 5, &trampoline);
//    if (ZYAN_SUCCESS(status))
//    {
//        original = &trampoline->code_buffer;
//
//#ifdef ZYAN_X64
//        ZyrexAttachInlineHook((void*)xxx, &trampoline->callback_jump);
//#else
//        ZyrexAttachInlineHook(xxx, &callback);
//#endif
//
//        printf("%.8X", ((functype)xxx)(NULL));
//
//        ZyrexTrampolineFree(trampoline);
//    }

    return 0;
}

/* ============================================================================================== */
