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
#include <Zyrex/Transaction.h>

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

typedef uint8_t ZyrexOperationType;

enum ZyrexOperationTypes
{
    ZYREX_OPERATION_TYPE_INLINE,
    ZYREX_OPERATION_TYPE_EXCEPTION,
    ZYREX_OPERATION_TYPE_CONTEXT
};

typedef uint8_t ZyrexOperationAction;

enum ZyrexOperationActions
{
    ZYREX_OPERATION_ACTION_ATTACH,
    ZYREX_OPERATION_ACTION_REMOVE
};

typedef struct ZyrexOperation
{
    ZyrexOperationType type;
    ZyrexOperationAction action;
    struct ZyrexOperation* next;
} ZyrexOperation;

typedef struct ZyrexInlineOperation_
{
    ZyrexOperationType type;
    ZyrexOperationAction action;
    struct ZyrexOperation* next;
    const void* trampoline;
} ZyrexInlineOperation;

/* ============================================================================================== */
/* Globals                                                                                        */
/* ============================================================================================== */

static LONG         g_transaction_thread_id = 0;
static ZyanStatus   g_transaction_error     = ZYAN_STATUS_SUCCESS;
static void*        g_pending_operations    = NULL;
static HANDLE*      g_pending_threads       = NULL;
static size_t       g_pending_thread_count  = 0;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZyrexTransactionBegin()
{
    if (g_transaction_thread_id != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    if (InterlockedCompareExchange(&g_transaction_thread_id, (LONG)GetCurrentThreadId(), 0) != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    g_transaction_error   = ZYAN_STATUS_SUCCESS;
    g_pending_operations  = NULL;
    g_pending_threads     = NULL;
    g_pending_thread_count = 0;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexUpdateThread(HANDLE thread_handle)
{
    if (ZYAN_SUCCESS(g_transaction_error))
    {
        return g_transaction_error;
    }
    if (thread_handle == GetCurrentThread())
    {
        return ZYAN_STATUS_SUCCESS;
    }
    const void* memory = realloc(g_pending_threads, sizeof(HANDLE) * (g_pending_thread_count + 1));
    if (!memory)
    {
        if (g_pending_threads)
        {
            free(g_pending_threads);
        }
        return ZYAN_STATUS_NOT_ENOUGH_MEMORY;
    }
    g_pending_threads = (HANDLE*)memory;
    g_pending_threads[g_pending_thread_count++] = thread_handle;
    if (SuspendThread(thread_handle) == (DWORD)-1)
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }
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
    if (g_transaction_thread_id != (LONG)GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    if (g_pending_threads)
    {
        free(g_pending_threads);
    }
    // ..
    g_transaction_thread_id = 0;
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
