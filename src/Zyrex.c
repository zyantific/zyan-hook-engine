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
static ZyrexStatus  g_transactionError      = ZYREX_ERROR_SUCCESS;
static void*        g_pendingOperations     = NULL;
static HANDLE*      g_pendingThreads        = NULL;
static size_t       g_pendingThreadCount    = 0;

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

ZyrexStatus ZyrexTransactionBegin()
{
    if (g_transactionThreadId != 0) 
    {
        return ZYREX_ERROR_INVALID_OPERATION;
    }
    if (InterlockedCompareExchange(&g_transactionThreadId, (LONG)GetCurrentThreadId(), 0) != 0) 
    {
        return ZYREX_ERROR_INVALID_OPERATION;
    }
    g_transactionError   = ZYREX_ERROR_SUCCESS;
    g_pendingOperations  = NULL;
    g_pendingThreads     = NULL;
    g_pendingThreadCount = 0;
    return ZYREX_ERROR_SUCCESS;
}

ZyrexStatus ZyrexUpdateThread(HANDLE threadHandle)
{
    if (ZYREX_SUCCESS(g_transactionError)) 
    {
        return g_transactionError;
    }
    if (threadHandle == GetCurrentThread()) 
    {
        return ZYREX_ERROR_SUCCESS;
    }
    const void* memory = realloc(g_pendingThreads, sizeof(HANDLE) * (g_pendingThreadCount + 1));
    if (!memory)
    {
        if (g_pendingThreads)
        {
            free(g_pendingThreads);
        }
        return ZYREX_ERROR_NOT_ENOUGH_MEMORY;
    }
    g_pendingThreads = (HANDLE*)memory;
    g_pendingThreads[g_pendingThreadCount++] = threadHandle;
    if (SuspendThread(threadHandle) == (DWORD)-1)
    {
        return ZYREX_ERROR_SYSTEMCALL;
    }
    return ZYREX_ERROR_SUCCESS;
}

ZyrexStatus ZyrexTransactionCommit()
{
    return ZyrexTransactionCommitEx(NULL);
}

ZyrexStatus ZyrexTransactionCommitEx(const void** failedOperation)
{
    ZYREX_UNUSED_PARAMETER(failedOperation);
    return ZYREX_ERROR_SUCCESS;
}

ZyrexStatus ZyrexTransactionAbort()
{
    if (g_transactionThreadId != (LONG)GetCurrentThreadId()) 
    {
        return ZYREX_ERROR_INVALID_OPERATION;
    }
    if (g_pendingThreads)
    {
        free(g_pendingThreads);
    }
    // ..
    g_transactionThreadId = 0;
    return ZYREX_ERROR_SUCCESS;
}

/* ============================================================================================== */
