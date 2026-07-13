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
 * @brief   Multithreaded thread-migration stress test.
 *
 * Removing a hook while other threads are actively calling it can free a trampoline that a
 * caller still references (e.g. a cached return address on its stack), regardless of how well
 * migration itself is implemented. A safe reclamation policy for that case - deferred retirement,
 * or never freeing trampolines - is an open design decision. This test therefore only exercises
 * concurrent hook *installation* (which migrates active callers via `ZyrexUpdateAllThreads`) and
 * quiesces all callers before removing the hook, so trampoline reclamation never races a caller.
 */

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <gtest/gtest.h>
#include <Zycore/Defines.h>
#include <Zyrex/Zyrex.h>
#include <Zyrex/Transaction.h>

typedef ZyanU32 (FnType)(ZyanU32 param);

static ZyanU32 ZYAN_NOINLINE MigrationTarget(ZyanU32 param)
{
    return param + 1;
}
static FnType* volatile g_orig = &MigrationTarget;
static ZyanU32 ZYAN_NOINLINE MigrationCallback(ZyanU32 param)
{
    return (*g_orig)(param) + 0x100;
}

TEST(MigrationTest, ConcurrentInstallMigratesCallersWithoutCorruption)
{
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);

    for (int iter = 0; iter < 8; ++iter)
    {
        std::atomic<bool> stop{false};
        std::atomic<unsigned long> mismatches{0};
        std::atomic<unsigned long> calls{0};

        auto worker = [&]()
        {
            while (!stop.load(std::memory_order_relaxed))
            {
                const ZyanU32 r = MigrationTarget(0x1000);
                if ((r != 0x1001) && (r != 0x1101))
                {
                    mismatches.fetch_add(1, std::memory_order_relaxed);
                }
                calls.fetch_add(1, std::memory_order_relaxed);
            }
        };

        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) workers.emplace_back(worker);
        while (calls.load() < 5000) { std::this_thread::yield(); }

        // Concurrent install: suspends + migrates the spinning workers, then installs the jump.
        ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
        ASSERT_EQ(ZyrexInstallInlineHook(reinterpret_cast<void*>(&MigrationTarget),
            reinterpret_cast<const void*>(&MigrationCallback),
            (ZyanConstVoidPointer*)(&g_orig)), ZYAN_STATUS_SUCCESS);
        ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
        ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

        // Let the workers execute the hooked path for a while.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Quiesce the callers BEFORE removing the hook. Removing while callers are active would
        // free a trampoline a caller may still reference (a cached pointer / return address) - the
        // trampoline-reclamation hazard that is a deferred design decision, not exercised here.
        stop.store(true);
        for (auto& t : workers) t.join();

        ASSERT_EQ(ZyrexTransactionBegin(), ZYAN_STATUS_SUCCESS);
        ASSERT_EQ(ZyrexRemoveInlineHook((ZyanConstVoidPointer*)(&g_orig)), ZYAN_STATUS_SUCCESS);
        ASSERT_EQ(ZyrexUpdateAllThreads(), ZYAN_STATUS_SUCCESS);
        ASSERT_EQ(ZyrexTransactionCommit(), ZYAN_STATUS_SUCCESS);

        EXPECT_EQ(mismatches.load(), 0u) << "iteration " << iter;
    }

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
}
