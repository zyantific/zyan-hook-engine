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

#include <Zycore/LibC.h>
#include <Zycore/Status.h>
#include <Zycore/Types.h>
#include <Zycore/Vector.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Internal/Relocation.h>

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Analyzed instruction                                                                           */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZyrexAnalyzedInstruction` struct.
 */
typedef struct ZyrexAnalyzedInstruction_
{
    /**
     * @brief   The runtime/memory address of the instruction.
     */
    ZyanUPointer address;
    /**
     * @brief   The `ZydisDecodedInstruction` struct of the analyzed instruction.
     */
    ZydisDecodedInstruction instruction;
    /**
     * @brief   Signals, if the instruction refers to a target address using a relative offset.
     */
    ZyanBool has_relative_target;
    /**
     * @brief   Signals, if the target address referred by the relative offset is not inside the
     *          analyzed code chunk.
     */
    ZyanBool has_external_target;
    /**
     * @brief   Signals, if this instruction is targeted by at least one instruction from inside
     *          the analyzed code chunk.
     */
    ZyanBool is_internal_target;
    /**
     * @brief   The absolute target address of the instruction calculated from the relative offset,
     *          if applicable.
     */
    ZyanU64 absolute_target_address;
    /**
     * @brief   Contains the indices of all instructions inside the analyzed code chunk that are
     *          targeting this instruction using a relative offset.
     */
    ZyanVector/*<ZyanU8>*/ targeted_from;
    /**
     * @brief   The index of an instruction inside the analyzed code chunk which is targeted by
     *          this instruction using a relative offset, or `-1` if not applicable.
     */
    ZyanU8 target_instruction_id;
} ZyrexAnalyzedInstruction;

/* ---------------------------------------------------------------------------------------------- */
/* Relocation context                                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZyrexRelocationContext` struct.
 */
typedef struct ZyrexRelocationContext_
{
    /**
     * @brief   The exact amount of bytes that should be relocated.
     */
    ZyanUSize bytes_to_reloc;
    /**
     * @brief   Contains a `ZyrexAnalyzedInstruction` struct for each instruction in the source
     *          buffer.
     */
    ZyanVector/*<ZyrexAnalyzedInstruction>*/ instructions;
    /**
     * @brief   A pointer to the source buffer.
     */
    const void* source;
    /**
     * @brief   The maximum amount of bytes that can be safely read from the source buffer.
     */
    ZyanUSize source_length;
    /**
     * @brief   A pointer to the destination buffer.
     */
    void* destination;
    /**
     * @brief   The maximum amount of bytes that can be safely written to the destination buffer.
     */
    ZyanUSize destination_length;
    /**
     * @brief   The instruction translation map.
     */
    ZyrexInstructionTranslationMap* translation_map;
    /**
     * @brief   The number of instructions read from the source buffer.
     */
    ZyanU8 instructions_read;
    /**
     * @brief   The number of instructions written to the destination buffer.
     */
    ZyanU8 instructions_written;
    /**
     * @brief   The number of bytes read from the source buffer.
     */
    ZyanUSize bytes_read;
    /**
     * @brief   The number of bytes written to the destination buffer.
     */
    ZyanUSize bytes_written;
} ZyrexRelocationContext;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* ZyrexAnalyzedInstruction                                                                       */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Finalizes the given `ZyrexAnalyzedInstruction` struct.
 *
 * @param   item    A pointer to the `ZyrexAnalyzedInstruction` struct.
 */
static void ZyrexAnalyzedInstructionDestroy(ZyrexAnalyzedInstruction* item)
{
    ZYAN_ASSERT(item);

    if (item->is_internal_target)
    {
        ZyanVectorDestroy(&item->targeted_from);   
    }
}

/* ---------------------------------------------------------------------------------------------- */
/* Instruction analysis                                                                           */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Analyzes the code in the source buffer and updates the relocation-context.
 *
 * @param   context             A pointer to the `ZyrexRelocationContext` struct.
 * @param   min_bytes_to_reloc  The minimum amount of bytes that need to be relocated to the
 *                              trampoline.
 *
 * Initializes the `bytes_to_reloc` and `instruction` members of the `ZyrexRelocationContext`.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexAnalyzeCode(ZyrexRelocationContext* context, ZyanUSize min_bytes_to_reloc)
{
    ZYAN_ASSERT(context);
    ZYAN_ASSERT(min_bytes_to_reloc);

    ZydisDecoder decoder;
#if defined(ZYAN_X86)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_ADDRESS_WIDTH_32);
#elif defined(ZYAN_X64)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
#else
#   error "Unsupported architecture detected"
#endif

    ZYAN_CHECK(ZyanVectorInit(&context->instructions, sizeof(ZyrexAnalyzedInstruction), 
        ZYREX_TRAMPOLINE_MAX_INSTRUCTION_COUNT, 
        (ZyanMemberProcedure)&ZyrexAnalyzedInstructionDestroy));

    // First pass:
    //   - Determine exact amount of instructions and instruction bytes
    //   - Decode all instructions and calculate relative target address for instructions with
    //     relative offsets
    //
    ZyanUSize bytes_read = 0;
    while (bytes_read < min_bytes_to_reloc)
    {
        ZyrexAnalyzedInstruction item;

        ZYAN_CHECK(ZydisDecoderDecodeBuffer(&decoder, (const ZyanU8*)context->source + bytes_read,
            context->source_length - bytes_read, &item.instruction));
        
        item.address = (ZyanUPointer)(const ZyanU8*)context->source + bytes_read;
        item.has_relative_target = (item.instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE)
            ? ZYAN_TRUE
            : ZYAN_FALSE;
        item.has_external_target = item.has_relative_target;
        item.absolute_target_address = 0;
        if (item.has_relative_target)
        {
            ZYAN_CHECK(ZyrexCalcAbsoluteAddress(&item.instruction, 
                (ZyanU64)context->source + bytes_read, &item.absolute_target_address));    
        }
        item.is_internal_target = ZYAN_FALSE;
        item.target_instruction_id = (ZyanU8)(-1);
        ZYAN_CHECK(ZyanVectorPushBack(&context->instructions, &item));

        bytes_read += item.instruction.length;
    }

    ZYAN_ASSERT(bytes_read >= min_bytes_to_reloc);
    context->bytes_to_reloc = bytes_read;

    // Second pass:
    //   - Find internal outgoing target for instructions with relative offsets
    //   - Find internal incoming targets from instructions with relative offsets
    //
    for (ZyanU8 i = 0; i < context->instructions.size; ++i)
    {
        ZyrexAnalyzedInstruction* const current = ZyanVectorGetMutable(&context->instructions, i);
        ZYAN_ASSERT(current);

        for (ZyanU8 j = 0; j < context->instructions.size; ++j)
        {
            ZyrexAnalyzedInstruction* const item = ZyanVectorGetMutable(&context->instructions, j);
            ZYAN_ASSERT(item);

            if (item->has_relative_target && (item->absolute_target_address == current->address))
            {
                // The `item` instruction targets the `current` instruction
                item->has_external_target = ZYAN_FALSE;
                item->target_instruction_id = (ZyanU8)i;

                // The `current` instruction is an internal target of the `item` instruction
                if (!current->is_internal_target)
                {
                    current->is_internal_target = ZYAN_TRUE;
                    ZYAN_CHECK(ZyanVectorInit(&current->targeted_from, sizeof(ZyanU8), 2, 
                        ZYAN_NULL));    
                }
                ZYAN_CHECK(ZyanVectorPushBack(&current->targeted_from, &j));
            }
        }
    }
    
    return ZYAN_STATUS_SUCCESS;
}

/**
 * @brief   Checks if the given instruction is a relative branch instruction.
 *
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      check.
 *
 * @return  `ZYAN_TRUE` if the instruction is a supported relative branch instruction or
 *          `ZYAN_FALSE`, if not. 
 */
static ZyanBool ZyrexIsRelativeBranchInstruction(const ZydisDecodedInstruction* instruction)
{
    ZYAN_ASSERT(instruction);

    switch (instruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_JMP:
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_JB:
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_JZ:
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_JBE:
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_JNLE:
        return ZYAN_TRUE;
    default:
        return ZYAN_FALSE;
    }
}

/**
 * @brief   Checks if the given instruction is an instruction with a relative memory operand.
 *
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      check.
 *
 * @return  `ZYAN_TRUE` if the instruction is an instruction with a relative memory operand or
 *          `ZYAN_FALSE`, if not.
 */
static ZyanBool ZyrexIsRelativeMemoryInstruction(const ZydisDecodedInstruction* instruction)
{
    ZYAN_ASSERT(instruction);

    return ((instruction->attributes & ZYDIS_ATTRIB_HAS_MODRM) &&
        (instruction->raw.modrm.mod == 0) && (instruction->raw.modrm.rm == 5))
        ? ZYAN_TRUE
        : ZYAN_FALSE;
}

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */



/**
 * @brief   Checks if the given instruction needs to be enlarged to 32-bit.
 *
 * @param   context     A pointer to the `ZyrexRelocationContext` struct.
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      relocate.
 *
 * @return  A zyan status code.
 *
 * This function returns `ZYAN_STATUS_TRUE` if the instruction is a valid branch instruction that
 * needs to be enlarged to 32-bit, `ZYAN_STATUS_FALSE` if the instruction is not a valid branch
 * instruction or does already have a 32-bit offset, or another zyan status code if an error
 * occured.
 *
 * A branch instruction needs to be enlarged exactly
 */
static ZyanStatus ZyrexShouldEnlargeBranchInstruction(ZyrexRelocationContext* context,
    const ZydisDecodedInstruction* instruction)
{
    ZYAN_ASSERT(context);
    ZYAN_ASSERT(instruction);

    if (instruction->raw.imm[0].size == 32)
    {
        return ZYAN_STATUS_FALSE;
    }

    ZyanU64 address;
    ZYAN_CHECK(ZyrexCalcAbsoluteAddress(instruction,
        (ZyanU64)context->source + context->bytes_read, &address));

    if ((address < (ZyanU64)context->source) ||
        (address > (ZyanU64)context->source + context->bytes_to_reloc))
    {
        return ZYAN_STATUS_TRUE;
    }

    return ZYAN_STATUS_FALSE;
}

/**
 * @brief   Calculates a new relative offset for rebased relative instructions.
 *
 * @param   original_instruction    The `ZydisDecodedInstruction` struct of the original instruction.
 * @param   original_address        The runtime address of the original instruction.
 * @param   rebased_address         The runtime address of the rebased instruction.
 * @param   rebased_length          The length of the rebased instruction.
 * @param   rebased_offset          Receives the new offset value adjusted for the rebased
 *                                  instruction.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexRebaseRelativeOffset(const ZydisDecodedInstruction* original_instruction, 
    ZyanUPointer original_address, ZyanUPointer rebased_address, ZyanU8 rebased_length, 
    ZyanI32* rebased_offset)
{
    ZYAN_ASSERT(original_instruction);
    ZYAN_ASSERT(rebased_offset);

    // Either a RIP-relative displacement or a relative immediate
    const ZyanI64 offset = original_instruction->raw.disp.offset
        ? original_instruction->raw.disp.value
        : original_instruction->raw.imm[0].value.s;

    const ZyanIPointer target_address = 
        (ZyanIPointer)original_address + original_instruction->length + offset;
    const ZyanIPointer target_ip = 
        (ZyanIPointer)rebased_address + rebased_length;

#define _ZYREX_HALF_MAX_SIGNED(type) ((type)1 << (sizeof(type) * 8 - 2))
#define _ZYREX_MAX_SIGNED(type) (_ZYREX_HALF_MAX_SIGNED(type) - 1 + _ZYREX_HALF_MAX_SIGNED(type))
#define _ZYREX_MIN_SIGNED(type) (-1 - _ZYREX_MAX_SIGNED(type))
#define _ZYREX_DOES_OVERFLOW(type, a, b) \
    (((a > 0) && (b > _ZYREX_MAX_SIGNED(type) - a)) || \
    ((a < 0) && (b < _ZYREX_MIN_SIGNED(type) - a)))

    if (_ZYREX_DOES_OVERFLOW(ZyanIPointer, target_address, -target_ip))
    {
        // TODO:
        return ZYAN_STATUS_FAILED;
    }

#undef _ZYREX_DOES_OVERFLOW
#undef _ZYREX_MIN_SIGNED
#undef _ZYREX_MAX_SIGNED
#undef _ZYREX_HALF_MAX_SIGNED

    *rebased_offset = (ZyanI32)(target_address - target_ip);
    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Relocation                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Relocates a single common instruction (without a relative offset) and updates the
 *          relocation-context.
 *
 * @param   context     A pointer to the `ZyrexRelocationContext` struct.
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      relocate.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexRelocateCommonInstruction(ZyrexRelocationContext* context,
    const ZydisDecodedInstruction* instruction)
{
    ZYAN_ASSERT(context);
    ZYAN_ASSERT(instruction);

    ZYAN_MEMCPY((ZyanU8*)context->destination + context->bytes_written,
        (const ZyanU8*)context->source + context->bytes_read, instruction->length);

    context->translation_map->items[context->instructions_read].offset_source =
        (ZyanU8)context->bytes_read;
    context->translation_map->items[context->instructions_read].offset_destination =
        (ZyanU8)context->bytes_written;
    ++context->translation_map->count;

    context->bytes_written += instruction->length;
    ++context->instructions_written;

    return ZYAN_STATUS_SUCCESS;
}

/**
 * @brief   Relocates the given relative branch instruction and updates the relocation-context.
 *
 * @param   context     A pointer to the `ZyrexRelocationContext` struct.
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      relocate.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexRelocateRelativeBranchInstruction(ZyrexRelocationContext* context,
    const ZydisDecodedInstruction* instruction)
{
    ZYAN_ASSERT(context);
    ZYAN_ASSERT(instruction);

    const ZyanStatus status = ZyrexShouldEnlargeBranchInstruction(context, instruction);
    ZYAN_CHECK(status);

    if (status == ZYAN_STATUS_TRUE)
    {
        // Enlarge branch instruction

        ZyanU8 opcode;
        ZyanU8 length = 6;
        switch (instruction->mnemonic)
        {
        case ZYDIS_MNEMONIC_JMP:
        {
            opcode = 0xE9;
            length = 5;
            break;
        }
        case ZYDIS_MNEMONIC_JO:
        {
            opcode = 0x80;
            break;
        }
        case ZYDIS_MNEMONIC_JNO:
        {
            opcode = 0x81;
            break;
        }
        case ZYDIS_MNEMONIC_JB:
        {
            opcode = 0x82;
            break;
        }
        case ZYDIS_MNEMONIC_JNB:
        {
            opcode = 0x83;
            break;
        }
        case ZYDIS_MNEMONIC_JZ:
        {
            opcode = 0x84;
            break;
        }
        case ZYDIS_MNEMONIC_JNZ:
        {
            opcode = 0x85;
            break;
        }
        case ZYDIS_MNEMONIC_JBE:
        {
            opcode = 0x86;
            break;
        }
        case ZYDIS_MNEMONIC_JNBE:
        {
            opcode = 0x87;
            break;
        }
        case ZYDIS_MNEMONIC_JS:
        {
            opcode = 0x88;
            break;
        }
        case ZYDIS_MNEMONIC_JNS:
        {
            opcode = 0x89;
            break;
        }
        case ZYDIS_MNEMONIC_JP:
        {
            opcode = 0x8A;
            break;
        }
        case ZYDIS_MNEMONIC_JNP:
        {
            opcode = 0x8B;
            break;
        }
        case ZYDIS_MNEMONIC_JL:
        {
            opcode = 0x8C;
            break;
        }
        case ZYDIS_MNEMONIC_JNL:
        {
            opcode = 0x8D;
            break;
        }
        case ZYDIS_MNEMONIC_JLE:
        {
            opcode = 0x8E;
            break;
        }
        case ZYDIS_MNEMONIC_JNLE:
        {
            opcode = 0x8F;
            break;
        }
        default:
            ZYAN_UNREACHABLE;
        }

        context->translation_map->items[context->instructions_read].offset_source =
            (ZyanU8)context->bytes_read;
        context->translation_map->items[context->instructions_read].offset_destination =
            (ZyanU8)context->bytes_written;
        ++context->translation_map->count;
        context->instructions_written += 1;

        ZyanI32 offset;
        ZYAN_CHECK(ZyrexRebaseRelativeOffset(instruction, 
            (ZyanUPointer)context->source + context->bytes_read,
            (ZyanUPointer)context->destination + context->bytes_written, length, &offset));

        ZyanU8* address = (ZyanU8*)context->destination + context->bytes_written;
        if (opcode == 0xE9)
        {
            *address++ = 0xE9;
            *(ZyanI32*)(address) = offset;
        } else
        {
            *address++ = 0x0F;
            *address++ = opcode;
            *(ZyanI32*)(address) = offset;
        }
        context->bytes_written += length;

        return ZYAN_STATUS_SUCCESS;
    }

    return ZyrexRelocateCommonInstruction(context, instruction);
}

/**
 * @brief   Relocates the given instruction with relative memory operand and updates the
 *          relocation-context.
 *
 * @param   context     A pointer to the `ZyrexRelocationContext` struct.
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      relocate.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexRelocateRelativeMemoryInstruction(ZyrexRelocationContext* context,
    const ZydisDecodedInstruction* instruction)
{
    ZYAN_ASSERT(context);
    ZYAN_ASSERT(instruction);



    return ZYAN_STATUS_FALSE;
}

/**
 * @brief   Relocates a single relative instruction and updates the relocation-context.
 *
 * This function takes care of code rewriting and/or enlarging the instruction to 32-bit if needed.
 *
 * @param   context     A pointer to the `ZyrexRelocationContext` struct.
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct of the instruction to
 *                      relocate.
 * @param   flags       Additional flags to control how the function handles some special
 *                      instructions.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexRelocateRelativeInstruction(ZyrexRelocationContext* context,
    const ZydisDecodedInstruction* instruction, ZyrexCodeRelocationFlags flags)
{
    ZYAN_ASSERT(context);
    ZYAN_ASSERT(instruction);

    switch (instruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_CALL:
    {
        if (!(flags & ZYREX_CODE_RELOC_FLAG_REWRITE_CALL))
        {
            return ZYAN_STATUS_FAILED; // TODO:
        }

        if (instruction->attributes & ZYDIS_ATTRIB_HAS_MODRM)
        {
            // Indirect absolute `CALL` instruction with `EIP/RIP`-relative address

        }
        else
        {
            // Relative `CALL` instruction

        }

        // TODO: Rewrite CALL

        break;
    }
    case ZYDIS_MNEMONIC_JCXZ:
    case ZYDIS_MNEMONIC_JECXZ:
    case ZYDIS_MNEMONIC_JRCXZ:
    {
        if (!(flags & ZYREX_CODE_RELOC_FLAG_REWRITE_JCXZ))
        {
            return ZYAN_STATUS_FAILED; // TODO:
        }

        // TODO: Rewrite JCXZ

        break;
    }
    case ZYDIS_MNEMONIC_LOOP:
    case ZYDIS_MNEMONIC_LOOPE:
    case ZYDIS_MNEMONIC_LOOPNE:
    {
        if (!(flags & ZYREX_CODE_RELOC_FLAG_REWRITE_LOOP))
        {
            return ZYAN_STATUS_FAILED; // TODO:
        }

        // TODO: Rewrite LOOP

        break;
    }
    default:
        break;
    }

    // Relocate relative branch instruction
    if (ZyrexIsRelativeBranchInstruction(instruction))
    {
        return ZyrexRelocateRelativeBranchInstruction(context, instruction);
    }

    // Relocate instruction with relative memory operand
    if (ZyrexIsRelativeMemoryInstruction(instruction))
    {
        return ZyrexRelocateRelativeMemoryInstruction(context, instruction);
    }

    // We should not be able to reach this code, if we correctly handled all existing relative
    // instructions   
    ZYAN_UNREACHABLE;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

ZyanStatus ZyrexRelocateCode(const void* source, ZyanUSize source_length, void* destination, 
    ZyanUSize destination_length, ZyanUSize min_bytes_to_reloc, ZyrexCodeRelocationFlags flags, 
    ZyrexInstructionTranslationMap* translation_map, ZyanUSize* bytes_read, ZyanUSize* bytes_written)
{
    ZYAN_ASSERT(source);
    ZYAN_ASSERT(source_length);
    ZYAN_ASSERT(destination);
    ZYAN_ASSERT(destination_length);
    ZYAN_ASSERT(min_bytes_to_reloc);
    ZYAN_ASSERT(translation_map);
    ZYAN_ASSERT(bytes_read);
    ZYAN_ASSERT(bytes_written);

    ZyrexRelocationContext context;
    context.bytes_to_reloc       = 0;
    context.source               = source;
    context.source_length        = source_length;
    context.destination          = destination;
    context.destination_length   = destination_length;
    context.translation_map      = translation_map;
    context.instructions_read    = 0;
    context.instructions_written = 0;
    context.bytes_read           = 0;
    context.bytes_written        = 0;

    ZydisDecoder decoder;
#if defined(ZYAN_X86)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_ADDRESS_WIDTH_32);
#elif defined(ZYAN_X64)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
#else
#   error "Unsupported architecture detected"
#endif

    ZYAN_CHECK(ZyrexAnalyzeCode(&context, min_bytes_to_reloc));

    ZydisDecodedInstruction instruction;
    ZyanUSize offset = 0;

    // Relocate instructions
    while (offset < min_bytes_to_reloc)
    {
        // The code buffer is full
        ZYAN_ASSERT(context.bytes_written < context.destination_length);
        // The translation map is full
        ZYAN_ASSERT(context.instructions_read < ZYAN_ARRAY_LENGTH(context.translation_map->items));

        const ZyanStatus status =
            ZydisDecoderDecodeBuffer(&decoder, (const ZyanU8*)source + offset, 
                source_length - offset, &instruction);
        ZYAN_ASSERT(status == ZYAN_STATUS_SUCCESS);

        if (instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE)
        {
            ZYAN_CHECK(ZyrexRelocateRelativeInstruction(&context, &instruction, flags));
        } else
        {
            ZYAN_CHECK(ZyrexRelocateCommonInstruction(&context, &instruction));
        }

        context.bytes_read += instruction.length;
        ++context.instructions_read;

        offset += instruction.length;
    }

    if (context.bytes_written > context.bytes_read)
    {
        // Fix the offsets of all relative instructions with target addresses inside the code chunk
        // to compensate for the instruction shift caused by rewriting some of the code

        // TODO:
    }

    ZYAN_ASSERT(offset == context.bytes_to_reloc);

    *bytes_read = context.bytes_read;
    *bytes_written = context.bytes_written;

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
