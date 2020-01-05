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
#include <Windows.h>
#include <Zycore/LibC.h>
#include <Zycore/Vector.h>
#include <Zycore/Zycore.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Transaction.h>
#include <Zyrex/Internal/InlineHook.h>
#include <Zyrex/Internal/Trampoline.h>

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * @brief   Defines the `ZyrexHookType` enum.
 */
typedef enum ZyrexHookType_
{
    /**
     * @brief   Inline hook.
     */
    ZYREX_HOOK_TYPE_INLINE,
    /**
     * @brief   Exception hook.
     */
    ZYREX_HOOK_TYPE_EXCEPTION,
    /**
     * @brief   Context/HWBP hook.
     */
    ZYREX_HOOK_TYPE_CONTEXT
} ZyrexHookType;

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
    ZyanConstVoidPointer* trampoline_accessor;
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
    ZyanVector/*<HANDLE>*/ threads_to_update;
} g_transaction_data =
{
    0, ZYAN_VECTOR_INITIALIZER, ZYAN_VECTOR_INITIALIZER
};

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* ZyanVector<HANDLE>                                                                             */
/* ---------------------------------------------------------------------------------------------- */

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
    // TODO: Use platform independent APIs

    DWORD old_protect;
    if (!VirtualProtect((LPVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP, PAGE_EXECUTE_READWRITE, 
        &old_protect))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

#if defined(ZYAN_X64)

    ZyrexWriteRelativeJump(address, (ZyanUPointer)&trampoline->callback_jump);

#elif defined(ZYAN_X86)

    ZyrexWriteRelativeJump(address, (ZyanUPointer)trampoline->callback_address);

#else
#   error "Unsupported platform"
#endif

    if (!VirtualProtect((LPVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP, old_protect, &old_protect))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), (LPCVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;
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
    // TODO: Use platform independent APIs

    DWORD old_protect;
    if (!VirtualProtect((LPVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP, PAGE_EXECUTE_READWRITE, 
        &old_protect))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    ZYAN_MEMCPY(address, &trampoline->original_code, trampoline->original_code_size);

    if (!VirtualProtect((LPVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP, old_protect, &old_protect))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), (LPCVOID)address, ZYREX_SIZEOF_RELATIVE_JUMP))
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;    
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Transaction                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexTransactionBegin()
{
    if (g_transaction_data.transaction_thread_id != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    if (InterlockedCompareExchange((volatile LONG*)&g_transaction_data.transaction_thread_id,
        (LONG)GetCurrentThreadId(), 0) != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_CHECK(ZyanVectorInit(&g_transaction_data.pending_operations, sizeof(ZyrexOperation),
        16, ZYAN_NULL));

    const ZyanStatus status = ZyanVectorInit(&g_transaction_data.threads_to_update, 
        sizeof(HANDLE), 16, (ZyanMemberProcedure)&ZyrexWindowsHandleDestroy);
    if (!ZYAN_SUCCESS(status))
    {
        ZyanVectorDestroy(&g_transaction_data.pending_operations);
        return status;
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexUpdateThread(DWORD thread_id)
{
    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    
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
}

ZyanStatus ZyrexUpdateAllThreads()
{
    // TODO:

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionCommit()
{
    return ZyrexTransactionCommitEx(NULL);
}

ZyanStatus ZyrexTransactionCommitEx(const void** failed_operation)
{
    ZYAN_UNUSED(failed_operation);

    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
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
                status = ZyrexWriteHookJump(item->address, item->trampoline);
                break;
            }
            case ZYREX_OPERATION_ACTION_REMOVE:
                status = ZyrexRestoreInstructions(item->address, item->trampoline);
                break;
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

    // TODO: Update threads

    ZyanVectorDestroy(&g_transaction_data.pending_operations);
    ZyanVectorDestroy(&g_transaction_data.threads_to_update);
    g_transaction_data.transaction_thread_id = 0;

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionAbort()
{
    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    
    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZYAN_VECTOR_FOREACH_MUTABLE(ZyrexOperation, &g_transaction_data.pending_operations, operation, 
    {
        ZyrexTrampolineFree(operation->trampoline);
    });

    ZYAN_VECTOR_FOREACH(HANDLE, &g_transaction_data.threads_to_update, handle, 
    {
        ResumeThread(handle);
    });

    ZyanVectorDestroy(&g_transaction_data.pending_operations);
    ZyanVectorDestroy(&g_transaction_data.threads_to_update);
    g_transaction_data.transaction_thread_id = 0;

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Hook installation                                                                              */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexInstallInlineHook(void* address, const void* callback, 
    ZyanConstVoidPointer* original)
{
    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
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
        /* trampoline          */ ZYAN_NULL,
        /* trampoline_accessor */ ZYAN_NULL
    };
    operation.address = address;
    operation.trampoline_accessor = original;
    ZYAN_CHECK(ZyrexTrampolineCreate(address, callback, ZYREX_SIZEOF_RELATIVE_JUMP, 
        &operation.trampoline));

    *original = &operation.trampoline->code_buffer;

    return ZyanVectorPushBack(&g_transaction_data.pending_operations, &operation);
}

/* ---------------------------------------------------------------------------------------------- */
/* Hook installation                                                                              */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexRemoveInlineHook(void* address, ZyanConstVoidPointer* original)
{
    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    
    ZYAN_ASSERT(g_transaction_data.pending_operations.data);
    ZYAN_ASSERT(g_transaction_data.threads_to_update.data);

    ZyrexOperation operation = 
    {
        /* type                */ ZYREX_HOOK_TYPE_INLINE,
        /* action              */ ZYREX_OPERATION_ACTION_REMOVE,
        /* address             */ ZYAN_NULL,
        /* trampoline          */ ZYAN_NULL,
        /* trampoline_accessor */ ZYAN_NULL
    };
    operation.address = address;
    operation.trampoline_accessor = original;

    // TODO:
    /*ZYAN_CHECK(ZyrexTrampolineCreate(address, callback, ZYREX_SIZEOF_RELATIVE_JUMP, 
        &operation.trampoline));*/

    *original = &operation.trampoline->code_buffer;

    return ZYAN_STATUS_SUCCESS;//ZyanVectorPushBack(&g_transaction_data.pending_operations, &operation);        
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */