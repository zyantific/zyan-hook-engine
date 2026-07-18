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

// A function whose first byte is an opcode invalid in 64-bit long mode (0x06, `PUSH ES`), which
// the relocation engine's decode step rejects, so a hook install on it fails - used to force the
// transaction down its rollback path.
extern "C" ZyanU32 UnhookableTarget(ZyanU32 param);

#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
__asm__(
    ".text\n"
    ".globl UnhookableTarget\n"
    "UnhookableTarget:\n"
    "    .byte 0x06\n"  // PUSH ES: invalid in 64-bit long mode -> relocation rejects the prologue
    "    mov %edi, %eax\n"
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

static ZyanU32 ZYAN_NOINLINE ReArmTarget(ZyanU32 param)
{
    return param;
}
// `ZyrexRemoveInlineHook` eagerly rewrites the pointer handed to it to the raw target address at
// queue time, before the transaction commits. If the callback dereferenced that same pointer, a
// reverted (re-armed) remove would make it call the hooked function itself, recursing forever.
// The callback instead uses this separate handle, captured once right after install and never
// touched by any later remove call, so it always reaches the trampoline.
static FnHookType* volatile g_rearm_trampoline = nullptr;
static ZyanU32 ZYAN_NOINLINE ReArmCallback(ZyanU32 param)
{
    return (*g_rearm_trampoline)(param) + 1;
}

TEST(InlineHookTest, RevertReArmsRemovedHookOnCommitFailure)
{
#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ReArmTarget(0x1337), static_cast<ZyanU32>(0x1337));

    // Install a hook on A and commit; A is now hooked.
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&ReArmTarget),
        reinterpret_cast<const void*>(&ReArmCallback),
        (ZyanConstVoidPointer*)(&g_rearm_trampoline)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ReArmTarget(0x1337), static_cast<ZyanU32>(0x1338));

    // New transaction: remove A's hook (applies first at commit), and install a hook on an
    // mmap'd target whose page is unmapped before commit, so the commit fails on the second
    // operation, after the remove of A has already been applied.
    const ZyanUSize page_size = static_cast<ZyanUSize>(sysconf(_SC_PAGESIZE));
    void* const page = mmap(ZYAN_NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);

    static const unsigned char prologue[] =
    {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0xC3
    };
    std::memcpy(page, prologue, sizeof(prologue));
    ASSERT_EQ(mprotect(page, page_size, PROT_READ | PROT_EXEC), 0);

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);

    // Use a scratch copy for the remove call - `ZyrexRemoveInlineHook` overwrites it with the
    // raw target address, and `g_rearm_trampoline` must keep pointing at the trampoline.
    ZyanConstVoidPointer remove_handle = reinterpret_cast<ZyanConstVoidPointer>(g_rearm_trampoline);
    ASSERT_EQ(ZyrexRemoveInlineHook(&remove_handle), ZYAN_STATUS_SUCCESS);

    ZyanConstVoidPointer page_hook_original = nullptr;
    ASSERT_EQ(ZyrexInstallInlineHook(page, reinterpret_cast<const void*>(&ReArmCallback),
        &page_hook_original), ZYAN_STATUS_SUCCESS);

    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);

    // Yank the page out from under the second operation right before commit, so the commit fails
    // only after the remove of A has already been applied.
    ASSERT_EQ(munmap(page, page_size), 0);

    const ZyanStatus status = ZyrexTransactionCommit();
    EXPECT_FALSE(ZYAN_SUCCESS(status));

    // The revert loop must have re-armed A's hook - callable, still returning the hooked value -
    // proving the trampoline survived the revert instead of being freed out from under it.
    EXPECT_EQ(ReArmTarget(0x1337), static_cast<ZyanU32>(0x1338));

    // Clean up: remove A's hook in a final successful transaction so the process ends clean.
    ZyanConstVoidPointer final_handle = reinterpret_cast<ZyanConstVoidPointer>(g_rearm_trampoline);
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexRemoveInlineHook(&final_handle), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ReArmTarget(0x1337), static_cast<ZyanU32>(0x1337));

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
#else
    GTEST_SKIP() << "Requires mmap/mprotect and the GNU x64 prologue bytes (ZYAN_GNUC && ZYAN_X64).";
#endif
}

// A function whose first 5 bytes are a relative CALL (E8 rel32) to a nearby helper that loads a
// bias into eax; the function then returns bias + param. Hooking it forces the relocation engine
// to relocate the CALL into the trampoline, and calling the original through the trampoline
// executes that relocated CALL - exercising a return address pushed into the trampoline.
extern "C" ZyanU32 CallPrologueTarget(ZyanU32 param);

#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
__asm__(
    ".text\n"
    ".globl CallPrologueTarget\n"
    "CallPrologueTarget:\n"
    "    call CallPrologueBias\n"  // E8 rel32 (5 bytes) - the relocated instruction
    "    addl %edi, %eax\n"        // System V: param in edi, result in eax -> bias + param
    "    ret\n"
    "CallPrologueBias:\n"
    "    movl $0x100, %eax\n"
    "    ret\n"
);

static FnHookType* volatile g_call_original = &CallPrologueTarget;
static ZyanU32 ZYAN_NOINLINE CallPrologueCallback(ZyanU32 param)
{
    return (*g_call_original)(param) + 0x11;
}
#endif

TEST(InlineHookTest, RelativeCallInPrologueHookedAndRemoved)
{
#if defined(ZYAN_GNUC) && defined(ZYAN_X64)
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);

    // Unhooked: bias (0x100) + param.
    EXPECT_EQ(CallPrologueTarget(0x1000), static_cast<ZyanU32>(0x1100));

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&CallPrologueTarget),
        reinterpret_cast<const void*>(&CallPrologueCallback),
        (ZyanConstVoidPointer*)(&g_call_original)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

    // Hooked: the callback runs the original via the trampoline (executing the relocated CALL) and
    // adds 0x11, i.e. (0x100 + param) + 0x11.
    EXPECT_EQ(CallPrologueTarget(0x1000), static_cast<ZyanU32>(0x1111));

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexRemoveInlineHook((ZyanConstVoidPointer*)(&g_call_original)),
        ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

    // Unhooked again.
    EXPECT_EQ(CallPrologueTarget(0x1000), static_cast<ZyanU32>(0x1100));

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
#else
    GTEST_SKIP() << "Requires the GNU x64 CALL-prologue stub (ZYAN_GNUC && ZYAN_X64).";
#endif
}

static ZyanU32 ZYAN_NOINLINE ReleaseTarget(ZyanU32 param)
{
    return param;
}
static FnHookType* volatile g_release_original = &ReleaseTarget;
static ZyanU32 ZYAN_NOINLINE ReleaseCallback(ZyanU32 param)
{
    return (*g_release_original)(param) + 1;
}

TEST(InlineHookTest, RemoveWithReleaseFlagCommits)
{
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ReleaseTarget(0x1337), static_cast<ZyanU32>(0x1337));

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&ReleaseTarget),
        reinterpret_cast<const void*>(&ReleaseCallback),
        (ZyanConstVoidPointer*)(&g_release_original)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ReleaseTarget(0x1337), static_cast<ZyanU32>(0x1338));

    // Remove with the release flag: the caller asserts no thread still references the trampoline
    // (single-threaded here), so its memory is unmapped rather than quarantined.
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexRemoveInlineHookEx((ZyanConstVoidPointer*)(&g_release_original),
        ZYREX_REMOVE_HOOK_FLAG_RELEASE_TRAMPOLINE), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ReleaseTarget(0x1337), static_cast<ZyanU32>(0x1337));

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
}

static ZyanU32 ZYAN_NOINLINE ShutdownReleaseTarget(ZyanU32 param)
{
    return param;
}
static FnHookType* volatile g_shutdown_original = &ShutdownReleaseTarget;
static ZyanU32 ZYAN_NOINLINE ShutdownReleaseCallback(ZyanU32 param)
{
    return (*g_shutdown_original)(param) + 1;
}

TEST(InlineHookTest, ShutdownExReleasesQuarantinedTrampolines)
{
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);

    // Install then remove with the default (quarantine) policy, leaving a quarantined trampoline.
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&ShutdownReleaseTarget),
        reinterpret_cast<const void*>(&ShutdownReleaseCallback),
        (ZyanConstVoidPointer*)(&g_shutdown_original)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexRemoveInlineHook((ZyanConstVoidPointer*)(&g_shutdown_original)),
        ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ShutdownReleaseTarget(0x1337), static_cast<ZyanU32>(0x1337));

    // Finalize with the release flag: the quarantined trampoline memory is reclaimed. The
    // subsystem must tear down and re-initialize cleanly, proving the release path is sound.
    ASSERT_EQ(ZyrexShutdownEx(ZYREX_SHUTDOWN_FLAG_RELEASE_TRAMPOLINES), ZYAN_STATUS_SUCCESS);

    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&ShutdownReleaseTarget),
        reinterpret_cast<const void*>(&ShutdownReleaseCallback),
        (ZyanConstVoidPointer*)(&g_shutdown_original)), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);
    EXPECT_EQ(ShutdownReleaseTarget(0x1337), static_cast<ZyanU32>(0x1338));

    ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexRemoveInlineHookEx((ZyanConstVoidPointer*)(&g_shutdown_original),
        ZYREX_REMOVE_HOOK_FLAG_RELEASE_TRAMPOLINE), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

    ASSERT_EQ(ZyrexShutdownEx(ZYREX_SHUTDOWN_FLAG_RELEASE_TRAMPOLINES), ZYAN_STATUS_SUCCESS);
}
