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

#ifndef ZYREX_H
#define ZYREX_H

#include <Windows.h>
#include <Zycore/Defines.h>
#include <Zycore/Status.h>
#include <ZyrexExportConfig.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Starts a new transaction.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionBegin();

/**
 * @brief   Adds a thread to the update list.
 *
 * @param   threadHandle    The handle of the thread.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexUpdateThread(HANDLE threadHandle);

/**
 * @brief   Attaches an inline hook.
 *
 * @param   address     Pointer to the code address. Receives the trampoline address, if the
 *                      transaction succeeded.
 * @param   callback    The callback address.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexAttachInlineHook(const void** address, const void* callback);

/**
 * @brief   Attaches an exception hook.
 *
 * @param   address     Pointer to the code address. Receives the trampoline address, if the
 *                      transaction succeeded.
 * @param   callback    The callback address.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexAttachExceptionHook(const void** address, const void* callback);

/**
 * @brief   Attaches a context hook.
 *
 * @param   address     Pointer to the code address. Receives the trampoline address, if the
 *                      transaction succeeded.
 * @param   callback    The callback address.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexAttachContextHook(const void** address, const void* callback);

// TODO: IAT/EAT, VTable, ..

/**
 * @brief   Commits the current transaction.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionCommit();

/**
 * @brief   Commits the current transaction.
 *
 * @param   failedOperation Receives a pointer to the operation that failed the transaction.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionCommitEx(const void** failedOperation);

/**
 * @brief   Cancels the current transaction.
 *
 * @return  @c ZYREX_ERROR_SUCCESS if the function succeeded, an other zyrex status code, if not.
 */
ZYREX_EXPORT ZyanStatus ZyrexTransactionAbort();

#ifdef __cplusplus
}
#endif
#endif /* ZYREX_H */
