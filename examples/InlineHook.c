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

#include <stdio.h>
#include <Zycore/Defines.h>
#include <Zyrex/Zyrex.h>
#include <Zyrex/Transaction.h>

/* ============================================================================================== */
/* Entry point                                                                                    */
/* ============================================================================================== */

typedef ZyanU32 (FnHookType)(ZyanU32 param);

static FnHookType* volatile FnHookOriginal = NULL;

ZyanU32 FnHookTarget(ZyanU32 param)
{
    puts("hello from original\n");

    return param;
}

ZyanU32 FnHookCallback(ZyanU32 param)
{
    puts("hello from callback\n");

    return (*FnHookOriginal)(param) + 1;
}

int main()
{
    ZyrexInitialize();

    ZyrexTransactionBegin();
    ZyrexInstallInlineHook(
        (void*)((ZyanUPointer)&FnHookTarget), 
        (const void*)((ZyanUPointer)&FnHookCallback), 
        (ZyanConstVoidPointer*)&FnHookOriginal
    );
    ZyrexUpdateAllThreads();
    ZyrexTransactionCommit();

    printf("%x\n", FnHookTarget(0x1337));

    (void)getchar();

    ZyrexTransactionBegin();
    ZyrexRemoveInlineHook((ZyanConstVoidPointer*)&FnHookOriginal);
    ZyrexUpdateAllThreads();
    ZyrexTransactionCommit();

    printf("%x\n", FnHookTarget(0x1337));

    return 0;
}

/* ============================================================================================== */