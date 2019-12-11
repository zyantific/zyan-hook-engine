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
#include <Zycore/Vector.h>
#include <Zycore/Zycore.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Transaction.h>
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
typedef struct ZyrexOperation
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
     * @brief   The trampoline chunk.
     */
     ZyrexTrampolineChunk* trampoline;
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
     * @brief   Signals, if the transaction API is initialized.
     */
    ZyanBool is_initialized;
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
    ZYAN_FALSE, 0, ZYAN_VECTOR_INITIALIZER, ZYAN_VECTOR_INITIALIZER
};

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

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

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

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

    if (!g_transaction_data.is_initialized)
    {
        ZYAN_CHECK(ZyanVectorInit(&g_transaction_data.pending_operations, sizeof(ZyrexOperation),
            16, ZYAN_NULL));

        const ZyanStatus status = ZyanVectorInit(&g_transaction_data.threads_to_update, 
            sizeof(HANDLE), 16, (ZyanMemberProcedure)&ZyrexWindowsHandleDestroy);
        if (!ZYAN_SUCCESS(status))
        {
            ZyanVectorDestroy(&g_transaction_data.pending_operations);
            return status;
        }

        g_transaction_data.is_initialized = ZYAN_TRUE;
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexUpdateThread(DWORD thread_id)
{
    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    if (!g_transaction_data.is_initialized)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

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
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionCommit()
{
    return ZyrexTransactionCommitEx(NULL);
}

ZyanStatus ZyrexTransactionCommitEx(const void** failed_operation)
{
    ZYAN_UNUSED(failed_operation);
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionAbort()
{
    if (g_transaction_data.transaction_thread_id != GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    if (!g_transaction_data.is_initialized)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZYAN_VECTOR_FOREACH_MUTABLE(ZyrexOperation, &g_transaction_data.pending_operations, operation, 
    {
        ZyrexTrampolineFree(operation->trampoline);
    });

    ZYAN_VECTOR_FOREACH(HANDLE, &g_transaction_data.threads_to_update, handle, 
    {
        ResumeThread(handle);
    });

    g_transaction_data.transaction_thread_id  = 0;

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */