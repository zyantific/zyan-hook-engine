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

#ifndef ZYREX_TRANSACTION_H
#define ZYREX_TRANSACTION_H

#include <Zycore/Defines.h>
#include <Zycore/Status.h>
#include <Zycore/API/Thread.h>
#include <ZyrexExportConfig.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Hook operation                                                                                 */
/* ---------------------------------------------------------------------------------------------- */



/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Transaction                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Starts a new transaction.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionBegin();

/**
 * @brief   Adds a specific thread to the thread-update list.
 *
 * The given thread is immediately suspended and resumed after the transaction was either committed
 * or canceled.
 *
 * @param   thread_id   The id of the thread to add to the update list.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexUpdateThread(DWORD thread_id);

/**
 * @brief   Adds all threads (except the calling one) to the update list.
 *
 * All threads are immediately suspended and resumed after the transaction was either committed
 * or canceled.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexUpdateAllThreads();

/**
 * @brief   Commits the current transaction.
 *
 * This function performs all pending hook attach/remove operations and updates all threads in the
 * thread-update list.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionCommit();

/**
 * @brief   Commits the current transaction.
 *
 * @param   failed_operation    Receives a pointer to the operation that failed the transaction.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionCommitEx(const void** failed_operation);

/**
 * @brief   Cancels the current transaction.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionAbort();

/* ---------------------------------------------------------------------------------------------- */
/* Hook installation                                                                              */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Installs an inline hook at the given `address`.
 *
 * @param   address     The address to hook.
 * @param   callback    The callback address.
 * @param   original    Receives the address of the original function, if the operation succeeded.
 *
 * @return  A zyan status code.                                                                  
 */
ZYREX_EXPORT ZyanStatus ZyrexInstallInlineHook(void* address, const void* callback, 
    ZyanConstVoidPointer* original);

///**
// * @brief   Attaches an exception hook.
// *
// * @param   address     Pointer to the code address. Receives the trampoline address, if the
// *                      transaction succeeded.
// * @param   callback    The callback address.
// *
// * @return  A zyan status code.
// */
//ZYREX_EXPORT ZyanStatus ZyrexAttachExceptionHook(const void** address, const void* callback);
//
///**
// * @brief   Attaches a context hook.
// *
// * @param   address     Pointer to the code address. Receives the trampoline address, if the
// *                      transaction succeeded.
// * @param   callback    The callback address.
// *
// * @return  A zyan status code.
// */
//ZYREX_EXPORT ZyanStatus ZyrexAttachContextHook(const void** address, const void* callback);

// TODO: IAT/EAT, VTable, ..

/* ---------------------------------------------------------------------------------------------- */
/* Hook removal                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Removes an inline hook at the given `address`.
 *
 * @param   address     The address to unhook.
 * @param   original    Receives the address of the original function, if the operation succeeded.
 *
 * @return  A zyan status code.                                                                  
 */
ZYREX_EXPORT ZyanStatus ZyrexRemoveInlineHook(void* address, ZyanConstVoidPointer* original);

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_TRANSACTION_H */
