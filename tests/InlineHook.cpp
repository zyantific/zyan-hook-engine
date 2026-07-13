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

// A function whose first bytes are a relative CALL, which the relocation engine rejects, so a
// hook install on it fails - used to force a mid-transaction commit failure.
extern "C" ZyanU32 UnhookableTarget(ZyanU32 param);

#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
__asm__(
    ".text\n"
    ".globl UnhookableTarget\n"
    "UnhookableTarget:\n"
    "    call 1f\n"     // relative CALL in the first bytes -> relocation rejects the prologue
    "1:  mov %edi, %eax\n"
    "    ret\n"
);
#else
extern "C" ZyanU32 UnhookableTarget(ZyanU32 param) { return param; }
#endif

static ZyanU32 ZYAN_NOINLINE ValidTarget(ZyanU32 param)
{
    return param;
}
static FnHookType* volatile g_valid_original = &ValidTarget;
static ZyanU32 ZYAN_NOINLINE ValidCallback(ZyanU32 param)
{
    return (*g_valid_original)(param) + 1;
}

#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
#include <sys/mman.h>
#include <cstring>
#include <unistd.h>
#endif

static ZyanU32 ZYAN_NOINLINE RevertTestTarget(ZyanU32 param)
{
    return param;
}
static FnHookType* volatile g_revert_test_original = &RevertTestTarget;
static ZyanU32 ZYAN_NOINLINE RevertTestCallback(ZyanU32 param)
{
    return (*g_revert_test_original)(param) + 1;
}

TEST(InlineHookTest, RevertOnCommitTimeFailure)
{
#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(RevertTestTarget(0x10), static_cast<ZyanU32>(0x10));

    const ZyanUSize page_size = static_cast<ZyanUSize>(sysconf(_SC_PAGESIZE));
    void* const page = mmap(ZYAN_NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);

    // A relocatable prologue (mov [rsp+8],rbx; push rdi; sub rsp,0x20; ret) with no RIP-relative
    // or branch instructions, so queue-time relocation analysis accepts it as a hookable target.
    static const unsigned char prologue[] =
    {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0xC3
    };
    std::memcpy(page, prologue, sizeof(prologue));
    ASSERT_EQ(mprotect(page, page_size, PROT_READ | PROT_EXEC), 0);

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);

    // First: a valid install that will be applied during commit.
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&RevertTestTarget),
        reinterpret_cast<const void*>(&RevertTestCallback),
        (ZyanConstVoidPointer*)(&g_revert_test_original)), ZYAN_STATUS_SUCCESS);

    // Second: a target with a valid relocatable prologue, so it queues successfully; its page
    // is unmapped below so the commit fails only once it tries to patch this target.
    ZyanConstVoidPointer page_hook_original = nullptr;
    ASSERT_EQ(ZyrexInstallInlineHook(page, reinterpret_cast<const void*>(&RevertTestCallback),
        &page_hook_original), ZYAN_STATUS_SUCCESS);

    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);

    // Yank the page out from under the second operation right before commit, so
    // `ZyrexWriteHookJump`'s `mprotect` fails once the commit loop reaches it.
    ASSERT_EQ(munmap(page, page_size), 0);

    const ZyanStatus status = ZyrexTransactionCommit();
    EXPECT_FALSE(ZYAN_SUCCESS(status));

    // The revert loop must have restored the first, already-applied target.
    EXPECT_EQ(RevertTestTarget(0x10), static_cast<ZyanU32>(0x10));

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
#else
    GTEST_SKIP() << "Requires mmap/mprotect and the GNU x64 prologue bytes (ZYAN_GNUC && ZYAN_X64).";
#endif
}

TEST(InlineHookTest, FailedCommitRollsBackAppliedOperations)
{
#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ValidTarget(0x10), static_cast<ZyanU32>(0x10));

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    // First: a valid install that will be applied.
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&ValidTarget),
        reinterpret_cast<const void*>(&ValidCallback),
        (ZyanConstVoidPointer*)(&g_valid_original)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);

    // Second: install on a target that relocation rejects, so this operation is queued but the
    // commit will fail when it reaches it. (Install may fail here at queue time, which also leaves
    // the transaction to be aborted - either way the valid target must not remain hooked.)
    ZyanConstVoidPointer unhookable_original = nullptr;
    const ZyanStatus install2 = ZyrexInstallInlineHook(reinterpret_cast<void*>(&UnhookableTarget),
        reinterpret_cast<const void*>(&ValidCallback), &unhookable_original);

    ZyanStatus commit_status = ZYAN_STATUS_SUCCESS;
    if (ZYAN_SUCCESS(install2))
    {
        commit_status = ZyrexTransactionCommit();
        EXPECT_FALSE(ZYAN_SUCCESS(commit_status)); // the second operation fails the commit
    }
    else
    {
        // The bad install was rejected at queue time; abort unwinds the first, valid install.
        EXPECT_EQ(ZyrexTransactionAbort(), ZYAN_STATUS_SUCCESS);
    }

    // Either path must leave ValidTarget NOT hooked (rolled back / never committed).
    EXPECT_EQ(ValidTarget(0x10), static_cast<ZyanU32>(0x10));

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
#else
    GTEST_SKIP() << "Requires the GNU inline-asm un-hookable stub (ZYAN_GNUC && ZYAN_X64).";
#endif
}
