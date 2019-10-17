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
#include <Zyrex/Internal/Trampoline.h>
#include <stdio.h>
#include <Windows.h>
#include "Zyrex/Transaction.h"
#include <stdint.h>

/* ============================================================================================== */
/* Entry point                                                                                    */
/* ============================================================================================== */

typedef int (*functype)();

const void* original = NULL;

int xxx2()
{
    puts("hello from original\n");

    int v = 0;
    for (int i = 0; i < 3; ++i)
    {
        v++;
    }
    return 1337;
}

int callback()
{
    puts("hello from callback\n");

    return ((functype)original)() + 1;
}

int main()
{
    ZyrexTransactionBegin();
    //ZyrexTransactionAbort();

    ZyanU8 buffer[15] = 
    {
        0x75, 0x02, 0xeb, 0xfb, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xC3
    };
    void* const buf = VirtualAlloc(NULL, sizeof(buffer), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(buf, buffer, sizeof(buffer));

    const functype xxx = (functype)buf;

    ZyrexTrampoline trampoline;
    const ZyanStatus status =
        ZyrexTrampolineCreate((const void*)xxx, (const void*)&callback, 5, &trampoline);
    if (ZYAN_SUCCESS(status))
    {
        original = trampoline.address_of_trampoline_code;

        DWORD old;
        VirtualProtect((LPVOID)xxx, 5, PAGE_EXECUTE_READWRITE, &old);
        uint8_t* t = (uint8_t*)xxx;
        *t++ = 0xE9;

#ifdef ZYAN_X64
        uintptr_t x = (const uint8_t*)trampoline.address_of_callback_jump - (const uint8_t*)xxx - 5;
#else
        uintptr_t x = (const uint8_t*)trampoline.address_of_callback - (const uint8_t*)xxx - 5;
#endif

        *(uint32_t*)t = (uint32_t)x;

        printf("%d", ((functype)xxx)());
    }

    return 0;
}

/* ============================================================================================== */
