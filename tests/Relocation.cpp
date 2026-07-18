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

#include <Zyrex/Internal/Utils.h>

// Resolves the absolute target of the relative instruction decoded at `offset` in `buffer`,
// treating `buffer` as if it were loaded at its own address.
static ZyanU64 ResolveTarget(const ZyanU8* buffer, ZyanUSize length, ZyanUSize offset)
{
    ZydisDecodedInstruction instr;
    EXPECT_TRUE(DecodeAt(buffer, length, offset, &instr));
    ZyanU64 target = 0;
    EXPECT_EQ(ZyrexCalcAbsoluteAddress(&instr, (ZyanU64)(buffer + offset), &target),
        ZYAN_STATUS_SUCCESS);
    return target;
}

TEST(RelocationTest, InternalBackwardBranchStillTargetsSameInstruction)
{
    // nop ; nop ; nop ; jmp short -5  (jumps to the first nop, an internal target)
    // Offsets: 0:90 1:90 2:90 3:EB 4:FB  -> jmp at 3, len 2, target = 3+2-5 = 0
    const ZyanU8 source[] = { 0x90, 0x90, 0x90, 0xEB, 0xFB };

    ZyrexTrampolineChunk chunk;
    InitChunk(&chunk);
    ZyanUSize read = 0, written = 0;
    ASSERT_EQ(ZyrexRelocateCode(source, sizeof(source), &chunk, 5, &read, &written),
        ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(read, static_cast<ZyanUSize>(5));

    // In the source, the jmp targets absolute (source+0). After relocation the copied jmp must
    // resolve to the copied first instruction, i.e. code_buffer+0.
    // Find the relocated jmp: it is the last translation-map entry whose source offset is 3.
    ZyanU8 jmp_dest_offset = 0xFF;
    for (ZyanU8 i = 0; i < chunk.translation_map.count; ++i)
    {
        if (chunk.translation_map.items[i].offset_source == 3)
        {
            jmp_dest_offset = chunk.translation_map.items[i].offset_destination;
        }
    }
    ASSERT_NE(jmp_dest_offset, 0xFF);

    const ZyanU64 resolved = ResolveTarget(chunk.code_buffer, sizeof(chunk.code_buffer),
        jmp_dest_offset);
    EXPECT_EQ(resolved, reinterpret_cast<ZyanU64>(&chunk.code_buffer[0]));
}

#include <Zyrex/Status.h>

TEST(RelocationTest, RelativeCallInPrologueRelocatedKeepsTarget)
{
    // call rel32 to an external target (E8 10 00 00 00 -> target = source + 5 + 0x10). 5 bytes.
    const ZyanU8 source[] = { 0xE8, 0x10, 0x00, 0x00, 0x00 };

    ZyrexTrampolineChunk chunk;
    InitChunk(&chunk);
    ZyanUSize read = 0, written = 0;
    ASSERT_EQ(ZyrexRelocateCode(source, sizeof(source), &chunk, 5, &read, &written),
        ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(read, static_cast<ZyanUSize>(5));

    // Still an E8 near-call, and its recomputed rel32 must resolve to the same absolute target as
    // the source call.
    EXPECT_EQ(chunk.code_buffer[0], static_cast<ZyanU8>(0xE8));
    const ZyanU64 source_target = ResolveTarget(source, sizeof(source), 0);
    const ZyanU64 reloc_target  = ResolveTarget(chunk.code_buffer, sizeof(chunk.code_buffer), 0);
    EXPECT_EQ(reloc_target, source_target);
}

TEST(RelocationTest, RipRelativeLeaKeepsResolvedTarget)
{
    // lea rax, [rip + 0x10]   (48 8D 05 10 00 00 00)  -- RIP-relative memory operand, 7 bytes.
    const ZyanU8 source[] = { 0x48, 0x8D, 0x05, 0x10, 0x00, 0x00, 0x00 };

    ZyrexTrampolineChunk chunk;
    InitChunk(&chunk);
    ZyanUSize read = 0, written = 0;
    ASSERT_EQ(ZyrexRelocateCode(source, sizeof(source), &chunk, 5, &read, &written),
        ZYAN_STATUS_SUCCESS);
    ASSERT_EQ(read, static_cast<ZyanUSize>(7)); // whole instruction copied to keep it intact

    // The RIP-relative target, computed from the source, must be preserved after relocation:
    // the relocated lea (at code_buffer+0) must resolve to the same absolute address.
    const ZyanU64 source_target = ResolveTarget(source, sizeof(source), 0);
    const ZyanU64 reloc_target  = ResolveTarget(chunk.code_buffer, sizeof(chunk.code_buffer), 0);
    EXPECT_EQ(reloc_target, source_target);
}
