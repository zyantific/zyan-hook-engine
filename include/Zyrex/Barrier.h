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

#ifndef ZYREX_BARRIER_H
#define ZYREX_BARRIER_H

#include <ZyrexExportConfig.h>
#include <Zycore/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Initialization and finalization                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Initializes the barrier system.
 *
 * @return  A zyan status code.
 *
 * This function must be called, before using any of the other barrier API functions.
 *
 * Calling this function multiple times might lead to unexpected behavior.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierSystemInitialize();

/**
 * @brief   Finalizes the barrier system.
 *
 * @return  A zyan status code.
 *
 * This function should never be called
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierSystemShutdown();

/* ---------------------------------------------------------------------------------------------- */
/* Barrier                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Tries to enter the barrier for the given hook.
 *
 * @brief   hook    TODO
 *
 * @return  `ZYAN_STATUS_TRUE` if the barrier could be entered, `ZYAN_STATUS_FALSE` if not, or a
 *          generic zyan status code if an error occured.
 *
 * This function passes the barrier, if the `current_recursion_depth` is `0`.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierTryEnter(const void* hook);

/**
 * @brief   Tries to enter the barrier for the given hook.
 *
 * @brief   hook                TODO
 * @biref   max_recursion_depth The maximum recursion depth to pass the barrier.
 *
 * @return  `ZYAN_STATUS_TRUE` if the barrier could be entered, `ZYAN_STATUS_FALSE` if not, or a
 *          generic zyan status code if an error occured.
 *
 * This function passes the barrier, if the `current_recursion_depth` is less than or equal to
 * the given `max_recursion_depth`.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierTryEnterEx(const void* hook, ZyanU32 max_recursion_depth);

/**
 * @brief   Leaves the barrier for the given hook.
 *
 * @param   hook    TODO
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierLeave(const void* hook);

/* ---------------------------------------------------------------------------------------------- */
/* Utils                                                                                          */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Returns the current recursion depth for the given hook.
 *
 * @param   hook            TODO
 * @param   current_depth   Receives the current recursion depth for the given hook or `0`, if no
 *                          barrier context was found.
 *
 * @return  `ZYAN_STATUS_TRUE` if a barrier context was found for the given hook,
 *          `ZYAN_STATUS_FALSE` if not, or a generic zyan status code if an error occured.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierGetRecursionDepth(const void* hook, ZyanU32* current_depth);

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_BARRIER_H */
