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
 * @brief   Unit tests for the instruction-relocation engine.
 */

#include <cstring>
#include <gtest/gtest.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Internal/Relocation.h>
#include <Zyrex/Internal/Trampoline.h>

namespace {

// Decodes the instruction at `offset` in `buffer`. Returns true on success.
bool DecodeAt(const ZyanU8* buffer, ZyanUSize length, ZyanUSize offset,
    ZydisDecodedInstruction* out)
{
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    // ZYAN_NULL is `(void*)0`; C++ needs `nullptr` here since the parameter is
    // strongly typed as `ZydisDecoderContext*`, unlike in C.
    return ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&decoder, nullptr,
        buffer + offset, length - offset, out));
}

// Zero-initialises a chunk so relocation writes into a clean buffer.
void InitChunk(ZyrexTrampolineChunk* chunk)
{
    std::memset(chunk, 0, sizeof(*chunk));
}

} // namespace

TEST(RelocationTest, NonRelativePrologueCopiedVerbatim)
{
    // mov [rsp+8], rbx ; push rdi ; sub rsp, 0x20   (all non-relative)
    const ZyanU8 source[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20 };

    ZyrexTrampolineChunk chunk;
    InitChunk(&chunk);

    ZyanUSize read = 0, written = 0;
    ASSERT_EQ(ZyrexRelocateCode(source, sizeof(source), &chunk, 5, &read, &written),
        ZYAN_STATUS_SUCCESS);

    // The first instruction (mov [rsp+8], rbx) is exactly 5 bytes, satisfying min_bytes_to_reloc.
    EXPECT_EQ(read, static_cast<ZyanUSize>(5));
    EXPECT_EQ(written, static_cast<ZyanUSize>(5));
    EXPECT_EQ(std::memcmp(chunk.code_buffer, source, 5), 0);
    EXPECT_EQ(chunk.translation_map.count, 1);
    EXPECT_EQ(chunk.translation_map.items[0].offset_source, 0);
    EXPECT_EQ(chunk.translation_map.items[0].offset_destination, 0);
}
