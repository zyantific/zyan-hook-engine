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

#include <stdlib.h>
#include <stdint.h>
#include <Zycore/LibC.h>
#include <Zycore/Vector.h>
#include <Zycore/Zycore.h>
#include <Zydis/Zydis.h>
#include <Zycore/API/Memory.h>
#include <Zycore/API/Process.h>
#include <Zycore/API/Thread.h>
#include <Zyrex/Transaction.h>
#include <Zyrex/Internal/InlineHook.h>
#include <Zyrex/Internal/Trampoline.h>

#ifdef ZYAN_WINDOWS
#   include <Windows.h>
#   include <TlHelp32.h>
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * @brief   Defines the `ZyrexOperationAction` enum.
 */
typedef enum ZyrexOperationAction_
{
    /**
     * @brief   Attach action.
     */
    ZYREX_OPERATION_ACTION_ATTACH,
    /**
     * @brief   Removal action.
     */
     ZYREX_OPERATION_ACTION_REMOVE
} ZyrexOperationAction;

/**
 * @brief   Defines the `ZyrexOperation` struct.
 */
typedef struct ZyrexOperation_
{
    /**
     * @brief   The hook type.
     */
    ZyrexHookType type;
    /**
     * @brief   The operation action.
     */
    ZyrexOperationAction action;
    /**
     * @brief   The code address.
     */
    void* address;
    /**
     * @brief   The trampoline chunk.
     */
    ZyrexTrampolineChunk* trampoline;
    /**
     * @brief   The user-provided pointer that stores the callable "original" - the trampoline while
     *          the hook is installed, the raw target once removed. It is updated during commit or
     *          revert while all other threads are suspended, so a concurrent caller never observes
     *          a state where the patched jump and this pointer disagree.
     */
    ZyanConstVoidPointer* accessor;
} ZyrexOperation;

/* ============================================================================================== */
/* Globals                                                                                        */
/* ============================================================================================== */

/**
 * @brief   Contains global transaction data.
 */
static struct
{
    /**
     * @brief   The id of the thread that started the current transaction.
     */
    volatile ZyanThreadId transaction_thread_id;
    /**
     * @brief   A list with all pending operations.
     */
    ZyanVector/*<ZyrexOperation>*/ pending_operations;
    /**
     * @brief   A list with all threads to update.
     */
    ZyanVector/*<ZyanThreadId>*/ threads_to_update;
} g_transaction_data =
{
    0, ZYAN_VECTOR_INITIALIZER, ZYAN_VECTOR_INITIALIZER
};

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Code Patching                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Changes the protection of the page(s) covering `[address, address + size)` and returns
 *          the previous protection of the first page in `old_protection` (if not `ZYAN_NULL`).
 *
 * `mprotect` requires a page-aligned base, so the range is widened to page boundaries; this also
 * covers a patch that straddles a page boundary.
 */
static ZyanStatus ZyrexProtectCode(void* address, ZyanUSize size,
    ZyanMemoryPageProtection protection, ZyanMemoryPageProtection* old_protection)
{
    const ZyanUPointer page_size = (ZyanUPointer)ZyanMemoryGetSystemPageSize();
    const ZyanUPointer start = ZYAN_ALIGN_DOWN((ZyanUPointer)address, page_size);
    const ZyanUPointer end   = ZYAN_ALIGN_UP((ZyanUPointer)address + size, page_size);

    if (old_protection)
    {
        ZyanMemoryRegionInfo info;
        ZYAN_CHECK(ZyanMemoryVirtualQuery((const void*)start, &info));
        *old_protection = info.protection;
    }
    return ZyanMemoryVirtualProtect((void*)start, end - start, protection);
}

/**
 * @brief   Writes the hook jump which redirects the code-flow from the given `address` to the
 *          `trampoline`.
 *
 * @param   address     The target address.
 * @param   trampoline  A pointer to the `ZyrexTrampolineChunk` struct.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexWriteHookJump(void* address, const ZyrexTrampolineChunk* trampoline)
{
    ZYAN_ASSERT(address);
    ZYAN_ASSERT(trampoline);

    ZyanMemoryPageProtection old_protection;
    ZYAN_CHECK(ZyrexProtectCode(address, ZYREX_SIZEOF_RELATIVE_JUMP, ZYAN_PAGE_EXECUTE_READWRITE,
        &old_protection));

#if defined(ZYAN_X64)

    ZyrexWriteRelativeJump(address, (ZyanUPointer)&trampoline->callback_jump);

#elif defined(ZYAN_X86)

    ZyrexWriteRelativeJump(address, (ZyanUPointer)trampoline->callback_address);

#else
#   error "Unsupported platform"
#endif

    ZYAN_CHECK(ZyrexProtectCode(address, ZYREX_SIZEOF_RELATIVE_JUMP, old_protection, ZYAN_NULL));
    return ZyanProcessFlushInstructionCache(address, ZYREX_SIZEOF_RELATIVE_JUMP);
}

/**
 * @brief   Reads the original code instructions from the `trampoline` and restores them to the
 *          given `address`.
 *
 * @param   address     The target address.
 * @param   trampoline  A pointer to the `ZyrexTrampolineChunk` struct.
 *
 * @return  A zyan status code.
 */
static ZyanStatus ZyrexRestoreInstructions(void* address, const ZyrexTrampolineChunk* trampoline)
{
    ZyanMemoryPageProtection old_protection;
    ZYAN_CHECK(ZyrexProtectCode(address, trampoline->original_code_size,
        ZYAN_PAGE_EXECUTE_READWRITE, &old_protection));

    ZYAN_MEMCPY(address, &trampoline->original_code, trampoline->original_code_size);

    ZYAN_CHECK(ZyrexProtectCode(address, trampoline->original_code_size, old_protection,
        ZYAN_NULL));
    return ZyanProcessFlushInstructionCache(address, trampoline->original_code_size);
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Transaction                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexTransactionBegin(void)
{
    if (g_transaction_data.transaction_thread_id != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

#ifdef ZYAN_WINDOWS

    // TODO: Use platform independent APIs
    if (InterlockedCompareExchange((volatile LONG*)&g_transaction_data.transaction_thread_id,
        (LONG)GetCurrentThreadId(), 0) != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

#else

    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));
    g_transaction_data.transaction_thread_id = tid;

#endif

    ZYAN_CHECK(ZyanVectorInit(&g_transaction_data.pending_operations, sizeof(ZyrexOperation),
        16, ZYAN_NULL));

    const ZyanStatus status = ZyanVectorInit(&g_transaction_data.threads_to_update,
        sizeof(ZyanThreadId), 16, ZYAN_NULL);
    if (!ZYAN_SUCCESS(status))
    {
        ZyanVectorDestroy(&g_transaction_data.pending_operations);
        return status;
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexUpdateThread(ZyanThreadId thread_id)
{
    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    if (thread_id == tid)
    {
        return ZYAN_STATUS_SUCCESS; // never suspend the calling thread
    }
    ZYAN_CHECK(ZyanThreadSuspend(thread_id));
    return ZyanVectorPushBack(&g_transaction_data.threads_to_update, &thread_id);
}

ZyanStatus ZyrexUpdateAllThreads(void)
{
    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZyanVector ids;
    ZYAN_CHECK(ZyanVectorInit(&ids, sizeof(ZyanThreadId), 16, ZYAN_NULL));
    ZyanStatus status = ZyanThreadEnumerate(&ids, ZYAN_FALSE);
    if (ZYAN_SUCCESS(status))
    {
        for (ZyanUSize i = 0; i < ids.size; ++i)
        {
            const ZyanThreadId id = *(const ZyanThreadId*)ZyanVectorGet(&ids, i);
            if (!ZYAN_SUCCESS(ZyanThreadSuspend(id)))
            {
                continue; // skip threads we could not suspend (e.g. already exited)
            }
            if (!ZYAN_SUCCESS(ZyanVectorPushBack(&g_transaction_data.threads_to_update, &id)))
            {
                ZYAN_UNUSED(ZyanThreadResume(id));
            }
        }
    }
    ZYAN_UNUSED(ZyanVectorDestroy(&ids));
    return status;
}

ZyanStatus ZyrexTransactionCommit(void)
{
    return ZyrexTransactionCommitEx(NULL);
}

ZyanStatus ZyrexTransactionCommitEx(const void** failed_operation)
{
    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZyanISize revert_index = (ZyanISize)(-1);
    ZyanStatus status = ZYAN_STATUS_SUCCESS;
    for (ZyanISize i = 0; i < (ZyanISize)g_transaction_data.pending_operations.size; ++i)
    {
        const ZyrexOperation* item = ZyanVectorGet(&g_transaction_data.pending_operations, i);
        ZYAN_ASSERT(item);

        switch (item->type)
        {
        case ZYREX_HOOK_TYPE_INLINE:
            switch (item->action)
            {
            case ZYREX_OPERATION_ACTION_ATTACH:
            {
                for (ZyanISize j = 0; j < (ZyanISize)g_transaction_data.threads_to_update.size;
                    ++j)
                {
                    const ZyanThreadId thread_id = *(const ZyanThreadId*)ZyanVectorGet(
                        &g_transaction_data.threads_to_update, j);

                    // TODO: Handle status code
                    ZyrexMigrateThread(thread_id, item->address,
                        item->trampoline->original_code_size, &item->trampoline->code_buffer,
                        item->trampoline->code_buffer_size, &item->trampoline->translation_map,
                        ZYREX_THREAD_MIGRATION_DIRECTION_SRC_DST);
                }

                // TODO: Check if code has changed between this call and the Attach*
                status = ZyrexWriteHookJump(item->address, item->trampoline);
                if (ZYAN_SUCCESS(status) && item->accessor)
                {
                    *(item->accessor) = &item->trampoline->code_buffer;
                }
                break;
            }
            case ZYREX_OPERATION_ACTION_REMOVE:
            {
                for (ZyanISize j = 0; j < (ZyanISize)g_transaction_data.threads_to_update.size;
                    ++j)
                {
                    const ZyanThreadId thread_id = *(const ZyanThreadId*)ZyanVectorGet(
                        &g_transaction_data.threads_to_update, j);

                    // TODO: Handle status code
                    ZyrexMigrateThread(thread_id, &item->trampoline->code_buffer,
                        item->trampoline->code_buffer_size, item->address,
                        item->trampoline->original_code_size, &item->trampoline->translation_map,
                        ZYREX_THREAD_MIGRATION_DIRECTION_DST_SRC);
                }

                status = ZyrexRestoreInstructions(item->address, item->trampoline);
                if (ZYAN_SUCCESS(status) && item->accessor)
                {
                    *(item->accessor) = item->address;
                }
                break;
            }
            default:
                ZYAN_UNREACHABLE;
            }
            break;
        case ZYREX_HOOK_TYPE_EXCEPTION:
            break;
        case ZYREX_HOOK_TYPE_CONTEXT:
            break;
        default:
            ZYAN_UNREACHABLE;
        }

        if (!ZYAN_SUCCESS(status))
        {
            if (failed_operation)
            {
                // The target address is stable; the operation struct is freed with the vector.
                *failed_operation = item->address;
            }
            revert_index = i;
            break;
        }
    }

    if (ZYAN_SUCCESS(status))
    {
        // Every operation applied. The trampolines of removed hooks are no longer referenced;
        // attach trampolines remain live as the installed hooks.
        for (ZyanISize i = 0; i < (ZyanISize)g_transaction_data.pending_operations.size; ++i)
        {
            const ZyrexOperation* const op =
                ZyanVectorGet(&g_transaction_data.pending_operations, i);
            if ((op->type == ZYREX_HOOK_TYPE_INLINE) &&
                (op->action == ZYREX_OPERATION_ACTION_REMOVE))
            {
                ZYAN_UNUSED(ZyrexTrampolineFree(op->trampoline));
            }
        }
    }
    else
    {
        // A commit operation failed. Roll back the applied/attempted operations in reverse. Undos
        // are idempotent (restoring already-original bytes, or re-writing an already-present jump,
        // is harmless), so including the failing operation is safe.
        for (ZyanISize j = revert_index; j >= 0; --j)
        {
            const ZyrexOperation* const undo =
                ZyanVectorGet(&g_transaction_data.pending_operations, j);
            if (undo->type != ZYREX_HOOK_TYPE_INLINE)
            {
                continue;
            }
            if (undo->action == ZYREX_OPERATION_ACTION_ATTACH)
            {
                for (ZyanISize k = 0; k < (ZyanISize)g_transaction_data.threads_to_update.size; ++k)
                {
                    const ZyanThreadId thread_id = *(const ZyanThreadId*)ZyanVectorGet(
                        &g_transaction_data.threads_to_update, k);
                    ZyrexMigrateThread(thread_id, &undo->trampoline->code_buffer,
                        undo->trampoline->code_buffer_size, undo->address,
                        undo->trampoline->original_code_size, &undo->trampoline->translation_map,
                        ZYREX_THREAD_MIGRATION_DIRECTION_DST_SRC);
                }
                ZYAN_UNUSED(ZyrexRestoreInstructions(undo->address, undo->trampoline));
                if (undo->accessor)
                {
                    *(undo->accessor) = undo->address;
                }
            }
            else
            {
                // Re-arm the removed hook; its trampoline is still valid (not freed above).
                for (ZyanISize k = 0; k < (ZyanISize)g_transaction_data.threads_to_update.size; ++k)
                {
                    const ZyanThreadId thread_id = *(const ZyanThreadId*)ZyanVectorGet(
                        &g_transaction_data.threads_to_update, k);
                    ZyrexMigrateThread(thread_id, undo->address,
                        undo->trampoline->original_code_size, &undo->trampoline->code_buffer,
                        undo->trampoline->code_buffer_size, &undo->trampoline->translation_map,
                        ZYREX_THREAD_MIGRATION_DIRECTION_SRC_DST);
                }
                ZYAN_UNUSED(ZyrexWriteHookJump(undo->address, undo->trampoline));
                if (undo->accessor)
                {
                    *(undo->accessor) = &undo->trampoline->code_buffer;
                }
            }
        }
        // No attach survives a failed transaction; free every attach trampoline (applied-and-
        // reverted or never applied). Remove trampolines belong to still-installed hooks - keep them.
        for (ZyanISize i = 0; i < (ZyanISize)g_transaction_data.pending_operations.size; ++i)
        {
            const ZyrexOperation* const op =
                ZyanVectorGet(&g_transaction_data.pending_operations, i);
            if ((op->type == ZYREX_HOOK_TYPE_INLINE) &&
                (op->action == ZYREX_OPERATION_ACTION_ATTACH))
            {
                ZYAN_UNUSED(ZyrexTrampolineFree(op->trampoline));
            }
        }
    }

    ZYAN_VECTOR_FOREACH(ZyanThreadId, &g_transaction_data.threads_to_update, thread_id,
    {
        ZYAN_UNUSED(ZyanThreadResume(thread_id));
    });
    ZyanVectorDestroy(&g_transaction_data.threads_to_update);

    ZyanVectorDestroy(&g_transaction_data.pending_operations);
    g_transaction_data.transaction_thread_id = 0;

    return status;
}

ZyanStatus ZyrexTransactionAbort(void)
{
    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZYAN_VECTOR_FOREACH_MUTABLE(const ZyrexOperation, &g_transaction_data.pending_operations,
        operation,
    {
        if ((operation->type == ZYREX_HOOK_TYPE_INLINE) &&
            (operation->action == ZYREX_OPERATION_ACTION_ATTACH))
        {
            ZyrexTrampolineFree(operation->trampoline);
        }
    });

    ZYAN_VECTOR_FOREACH(ZyanThreadId, &g_transaction_data.threads_to_update, thread_id,
    {
        ZYAN_UNUSED(ZyanThreadResume(thread_id));
    });

    ZyanVectorDestroy(&g_transaction_data.threads_to_update);

    ZyanVectorDestroy(&g_transaction_data.pending_operations);
    g_transaction_data.transaction_thread_id = 0;

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Hook installation                                                                              */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexInstallInlineHook(void* address, const void* callback,
    ZyanConstVoidPointer* trampoline)
{
    if (!address || !callback || !trampoline)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZyrexOperation operation =
    {
        /* type                */ ZYREX_HOOK_TYPE_INLINE,
        /* action              */ ZYREX_OPERATION_ACTION_ATTACH,
        /* address             */ ZYAN_NULL,
        /* trampoline          */ ZYAN_NULL
    };
    operation.address = address;
    ZYAN_CHECK(ZyrexTrampolineCreate(address, callback, ZYREX_SIZEOF_RELATIVE_JUMP,
        &operation.trampoline));
    operation.accessor = trampoline;

    return ZyanVectorPushBack(&g_transaction_data.pending_operations, &operation);
}

/* ---------------------------------------------------------------------------------------------- */
/* Hook installation                                                                              */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexRemoveInlineHook(ZyanConstVoidPointer* original)
{
    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZyrexTrampolineChunk* trampoline;
    ZYAN_CHECK(ZyrexTrampolineFind(*original, &trampoline));

    ZyanVoidPointer const target =
        (ZyanVoidPointer)(trampoline->backjump_address - trampoline->original_code_size);

    ZyrexOperation operation =
    {
        /* type                */ ZYREX_HOOK_TYPE_INLINE,
        /* action              */ ZYREX_OPERATION_ACTION_REMOVE,
        /* address             */ ZYAN_NULL,
        /* trampoline          */ ZYAN_NULL
    };
    operation.address = target;
    operation.trampoline = trampoline;
    operation.accessor = original;

    return ZyanVectorPushBack(&g_transaction_data.pending_operations, &operation);
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
