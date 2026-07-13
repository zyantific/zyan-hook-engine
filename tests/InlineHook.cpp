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
 * @brief   End-to-end test for inline hook install/remove.
 */

#include <gtest/gtest.h>
#include <Zycore/Defines.h>
#include <Zyrex/Zyrex.h>
#include <Zyrex/Transaction.h>

typedef ZyanU32 (FnHookType)(ZyanU32 param);

static ZyanU32 ZYAN_NOINLINE HookTarget(ZyanU32 param)
{
    return param;
}

static FnHookType* volatile g_original = &HookTarget;

static ZyanU32 ZYAN_NOINLINE HookCallback(ZyanU32 param)
{
    return (*g_original)(param) + 1;
}

TEST(InlineHookTest, InstallCallRemove)
{
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);

    // Unhooked baseline.
    EXPECT_EQ(HookTarget(0x1337), static_cast<ZyanU32>(0x1337));

    // Install the hook.
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&HookTarget),
        reinterpret_cast<const void*>(&HookCallback),
        (ZyanConstVoidPointer*)(&g_original)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

    // Hooked: the callback adds one.
    EXPECT_EQ(HookTarget(0x1337), static_cast<ZyanU32>(0x1338));

    // Remove the hook.
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexRemoveInlineHook((ZyanConstVoidPointer*)(&g_original)),
        ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

    // Unhooked again.
    EXPECT_EQ(HookTarget(0x1337), static_cast<ZyanU32>(0x1337));

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
}
