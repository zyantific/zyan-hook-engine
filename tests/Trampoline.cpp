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
 * @brief   End-to-end tests for the trampoline allocator (create / find / free).
 */

#include <cstring>
#include <cstdlib>
#include <vector>
#include <gtest/gtest.h>
#include <Zyrex/Internal/Trampoline.h>
#include <Zyrex/Internal/Utils.h>

// A simple, relocatable prologue followed by `ret`:
//   mov [rsp+8], rbx ; push rdi ; sub rsp, 0x20 ; ret
static const ZyanU8 g_function[] =
{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0xC3
};

TEST(TrampolineTest, CreateFindFreeRoundtrip)
{
    int callback_sink = 0; // any valid address works; the trampoline is not executed here

    ZyrexTrampolineChunk* chunk = nullptr;
    ASSERT_EQ(ZyrexTrampolineCreate(g_function, &callback_sink, ZYREX_SIZEOF_RELATIVE_JUMP, &chunk),
        ZYAN_STATUS_SUCCESS);
    ASSERT_NE(chunk, nullptr);

    // At least the branch-sized prologue was saved, verbatim.
    EXPECT_GE(chunk->original_code_size, static_cast<ZyanU8>(ZYREX_SIZEOF_RELATIVE_JUMP));
    EXPECT_EQ(std::memcmp(chunk->original_code, g_function, chunk->original_code_size), 0);
    EXPECT_EQ(chunk->backjump_address,
        reinterpret_cast<ZyanUPointer>(g_function) + chunk->original_code_size);

    // The trampoline must sit within +/-2 GiB of the source so a relative jump can reach it.
    const ZyanIPointer distance =
        reinterpret_cast<ZyanIPointer>(&chunk->code_buffer) -
        reinterpret_cast<ZyanIPointer>(g_function);
    EXPECT_LE(static_cast<long long>(distance < 0 ? -distance : distance),
        static_cast<long long>(0x7FFFFFFF));

    // The chunk is findable by its trampoline (code_buffer) pointer.
    ZyrexTrampolineChunk* found = nullptr;
    EXPECT_EQ(ZyrexTrampolineFind(&chunk->code_buffer, &found), ZYAN_STATUS_TRUE);
    EXPECT_EQ(found, chunk);

    EXPECT_EQ(ZyrexTrampolineFree(chunk), ZYAN_STATUS_SUCCESS);
}

TEST(TrampolineTest, ManyTrampolinesAllocateAndFree)
{
    // Creating many trampolines forces the region search to fill the first region and walk to
    // allocate more, exercising the +/-2 GiB range check and its termination. This is precisely
    // the path that hung before the range-check fix, so it must complete (under the test timeout)
    // and never fail to allocate.
    //
    // The `ZYAN_STATUS_OUT_OF_RANGE` path (both search cursors retired without finding a region)
    // is not exercised here: reserving a full +/-2 GiB window portably in a unit test is
    // impractical. Termination toward that path is now structurally guaranteed by the per-cursor
    // range/query/progress guards in `ZyrexTrampolineRegionAllocate`, independent of address
    // wraparound.
    int callback_sink = 0;

    std::vector<ZyrexTrampolineChunk*> chunks;
    for (int i = 0; i < 64; ++i)
    {
        ZyrexTrampolineChunk* chunk = nullptr;
        ASSERT_EQ(
            ZyrexTrampolineCreate(g_function, &callback_sink, ZYREX_SIZEOF_RELATIVE_JUMP, &chunk),
            ZYAN_STATUS_SUCCESS) << "failed at iteration " << i;
        ASSERT_NE(chunk, nullptr);
        chunks.push_back(chunk);
    }

    for (ZyrexTrampolineChunk* chunk : chunks)
    {
        EXPECT_EQ(ZyrexTrampolineFree(chunk), ZYAN_STATUS_SUCCESS);
    }
}
