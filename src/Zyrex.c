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
#include <Zyrex/Zyrex.h>

/* ============================================================================================== */
/* Structs                                                                                        */
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
/* Static Variables                                                                               */
/* ============================================================================================== */

static LONG         g_transactionThreadId   = 0;
static ZyanStatus   g_transactionError      = ZYAN_STATUS_SUCCESS;
static void*        g_pendingOperations     = NULL;
static HANDLE*      g_pendingThreads        = NULL;
static size_t       g_pendingThreadCount    = 0;

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

ZyanStatus ZyrexTransactionBegin()
{
    if (g_transactionThreadId != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    if (InterlockedCompareExchange(&g_transactionThreadId, (LONG)GetCurrentThreadId(), 0) != 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    g_transactionError   = ZYAN_STATUS_SUCCESS;
    g_pendingOperations  = NULL;
    g_pendingThreads     = NULL;
    g_pendingThreadCount = 0;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexUpdateThread(HANDLE threadHandle)
{
    if (ZYAN_SUCCESS(g_transactionError))
    {
        return g_transactionError;
    }
    if (threadHandle == GetCurrentThread())
    {
        return ZYAN_STATUS_SUCCESS;
    }
    const void* memory = realloc(g_pendingThreads, sizeof(HANDLE) * (g_pendingThreadCount + 1));
    if (!memory)
    {
        if (g_pendingThreads)
        {
            free(g_pendingThreads);
        }
        return ZYAN_STATUS_NOT_ENOUGH_MEMORY;
    }
    g_pendingThreads = (HANDLE*)memory;
    g_pendingThreads[g_pendingThreadCount++] = threadHandle;
    if (SuspendThread(threadHandle) == (DWORD)-1)
    {
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionCommit()
{
    return ZyrexTransactionCommitEx(NULL);
}

ZyanStatus ZyrexTransactionCommitEx(const void** failedOperation)
{
    ZYAN_UNUSED(failedOperation);
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTransactionAbort()
{
    if (g_transactionThreadId != (LONG)GetCurrentThreadId())
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }
    if (g_pendingThreads)
    {
        free(g_pendingThreads);
    }
    // ..
    g_transactionThreadId = 0;
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
