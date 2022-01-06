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
     * @brief   This value points to the memory that is passed by the user to store the trampoline
     *          pointer.
     */
    //ZyanConstVoidPointer* trampoline_accessor;
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

#ifdef ZYAN_WINDOWS

    /**
     * @brief   A list with all threads to update.
     */
    ZyanVector/*<HANDLE>*/ threads_to_update;

#endif
} g_transaction_data =
{
    0, ZYAN_VECTOR_INITIALIZER,
#ifdef ZYAN_WINDOWS
    ZYAN_VECTOR_INITIALIZER
#endif
};

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* ZyanVector<HANDLE>                                                                             */
/* ---------------------------------------------------------------------------------------------- */

#ifdef ZYAN_WINDOWS

/**
 * @brief   Finalizes the given `HANDLE` item.
 *
 * @param   item    A pointer to the `HANDLE` item.
 */
static void ZyrexWindowsHandleDestroy(HANDLE* item)
{
    ZYAN_ASSERT(item);

    CloseHandle(*item);
}

#endif

/* ---------------------------------------------------------------------------------------------- */
/* Code Patching                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

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

    ZYAN_CHECK(ZyanMemoryVirtualProtect(address, ZYREX_SIZEOF_RELATIVE_JUMP,
        ZYAN_PAGE_EXECUTE_READWRITE));

#if defined(ZYAN_X64)

    ZyrexWriteRelativeJump(address, (ZyanUPointer)&trampoline->callback_jump);

#elif defined(ZYAN_X86)

    ZyrexWriteRelativeJump(address, (ZyanUPointer)trampoline->callback_address);

#else
#   error "Unsupported platform"
#endif

    // TODO: Restore actual protection
    ZYAN_CHECK(ZyanMemoryVirtualProtect(address, ZYREX_SIZEOF_RELATIVE_JUMP,
        ZYAN_PAGE_EXECUTE_READ));

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
    ZYAN_CHECK(ZyanMemoryVirtualProtect(address, ZYREX_SIZEOF_RELATIVE_JUMP,
        ZYAN_PAGE_EXECUTE_READWRITE));

    ZYAN_MEMCPY(address, &trampoline->original_code, trampoline->original_code_size);

    // TODO: Restore actual protection
    ZYAN_CHECK(ZyanMemoryVirtualProtect(address, ZYREX_SIZEOF_RELATIVE_JUMP,
        ZYAN_PAGE_EXECUTE_READ));

    return ZyanProcessFlushInstructionCache(address, ZYREX_SIZEOF_RELATIVE_JUMP);
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

#ifdef ZYAN_WINDOWS

    const ZyanStatus status = ZyanVectorInit(&g_transaction_data.threads_to_update,
        sizeof(HANDLE), 16, (ZyanMemberProcedure)&ZyrexWindowsHandleDestroy);
    if (!ZYAN_SUCCESS(status))
    {
        ZyanVectorDestroy(&g_transaction_data.pending_operations);
        return status;
    }

#endif

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

#ifdef ZYAN_WINDOWS

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    if (thread_id == GetCurrentThreadId())
    {
        return ZYAN_STATUS_SUCCESS;
    }

    const DWORD desired_access = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT;
    const HANDLE handle = OpenThread(desired_access, ZYAN_FALSE, thread_id);
    if (handle == ZYAN_NULL)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (SuspendThread(handle) == (DWORD)(-1))
    {
        CloseHandle(handle);
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZyanVectorPushBack(&g_transaction_data.threads_to_update, &handle);

#else

    ZYAN_UNUSED(thread_id);
    return ZYAN_STATUS_SUCCESS;

#endif
}

ZyanStatus ZyrexUpdateAllThreads(void)
{
    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

#ifdef ZYAN_WINDOWS

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    const DWORD pid = GetCurrentProcessId();

    const HANDLE h_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
    if (h_snapshot == INVALID_HANDLE_VALUE)
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    THREADENTRY32 thread;
    ZYAN_MEMSET(&thread, 0, sizeof(thread));
    thread.dwSize = sizeof(thread);

    if (Thread32First(h_snapshot, &thread))
    {
        do
        {
            if ((thread.th32OwnerProcessID == pid) && (thread.th32ThreadID != tid))
            {
                const HANDLE h_thread =
                    OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                        ZYAN_FALSE, thread.th32ThreadID);
                if (h_thread != ZYAN_NULL)
                {
                    if (SuspendThread(h_thread) == (DWORD)(-1))
                    {
                        CloseHandle(h_thread);
                    }
                    else
                    {
                        ZyanVectorPushBack(&g_transaction_data.threads_to_update, &h_thread);
                    }
                }
            }
        } while (Thread32Next(h_snapshot, &thread));
    }

    if (!CloseHandle(h_snapshot))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

#endif

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionCommit(void)
{
    return ZyrexTransactionCommitEx(NULL);
}

ZyanStatus ZyrexTransactionCommitEx(const void** failed_operation)
{
    ZYAN_UNUSED(failed_operation);

    ZyanThreadId tid;
    ZYAN_CHECK(ZyanThreadGetCurrentThreadId(&tid));

    if (g_transaction_data.transaction_thread_id != tid)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
#ifdef ZYAN_WINDOWS
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);
#endif

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
#ifdef ZYAN_WINDOWS

                for (ZyanISize j = 0; j < (ZyanISize)g_transaction_data.threads_to_update.size;
                    ++j)
                {
                    const HANDLE* const thread_handle =
                        (const HANDLE*)ZyanVectorGet(&g_transaction_data.threads_to_update, j);
                    ZYAN_ASSERT(thread_handle);

                    // TODO: Handle status code
                    ZyrexMigrateThread(*thread_handle, item->address,
                        item->trampoline->original_code_size, &item->trampoline->code_buffer,
                        item->trampoline->code_buffer_size, &item->trampoline->translation_map,
                        ZYREX_THREAD_MIGRATION_DIRECTION_SRC_DST);
                }

#endif

                // TODO: Check if code has changed between this call and the Attach*
                status = ZyrexWriteHookJump(item->address, item->trampoline);
                break;
            }
            case ZYREX_OPERATION_ACTION_REMOVE:
            {
#ifdef ZYAN_WINDOWS

                for (ZyanISize j = 0; j < (ZyanISize)g_transaction_data.threads_to_update.size;
                    ++j)
                {
                    const HANDLE* const thread_handle =
                        (const HANDLE*)ZyanVectorGet(&g_transaction_data.threads_to_update, j);
                    ZYAN_ASSERT(thread_handle);

                    // TODO: Handle status code
                    ZyrexMigrateThread(*thread_handle, &item->trampoline->code_buffer,
                        item->trampoline->code_buffer_size, item->address,
                        item->trampoline->original_code_size, &item->trampoline->translation_map,
                        ZYREX_THREAD_MIGRATION_DIRECTION_DST_SRC);
                }

#endif

                status = ZyrexRestoreInstructions(item->address, item->trampoline);
                if (ZYAN_SUCCESS(status))
                {
                    status = ZyrexTrampolineFree(item->trampoline);
                    if (status == ZYAN_STATUS_FALSE)
                    {
                        status = ZYAN_STATUS_NOT_FOUND;
                    }
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
            revert_index = i - 1;
            break;
        }
    }

    if (revert_index >= 0)
    {
        // TODO: Revert changes
    }

#ifdef ZYAN_WINDOWS

    ZyanVectorDestroy(&g_transaction_data.threads_to_update);

#endif

    ZyanVectorDestroy(&g_transaction_data.pending_operations);
    g_transaction_data.transaction_thread_id = 0;

    return ZYAN_STATUS_SUCCESS;
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
#ifdef ZYAN_WINDOWS
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);
#endif

    ZYAN_VECTOR_FOREACH_MUTABLE(const ZyrexOperation, &g_transaction_data.pending_operations, 
        operation,
    {
        ZyrexTrampolineFree(operation->trampoline);
    });

#ifdef ZYAN_WINDOWS

    ZYAN_VECTOR_FOREACH(HANDLE, &g_transaction_data.threads_to_update, handle,
    {
        ResumeThread(handle);
    });

    ZyanVectorDestroy(&g_transaction_data.threads_to_update);

#endif

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
#ifdef ZYAN_WINDOWS
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);
#endif

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

    *trampoline = &operation.trampoline->code_buffer;

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
#ifdef ZYAN_WINDOWS
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);
#endif

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

    *original = target;

    return ZyanVectorPushBack(&g_transaction_data.pending_operations, &operation);
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
