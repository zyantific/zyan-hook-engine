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
/* Enums and types                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZyrexBarrierHandle` data-type.
 */
typedef ZyanUPointer ZyrexBarrierHandle;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Macros                                                                                         */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Helper macros                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Tries to enter the barrier for the given hook and automatically returns from the callback
 * function after invoking the given `trampoline` in case of failure.
 *
 * @param   handle      The barrier hook handle
 * @param   trampoline  The trampoline.
 */
#define ZYREX_BARRIER_ENTER_FUNC(handle, trampoline, ...) \
    if (ZyrexBarrierTryEnter(handle) != ZYAN_STATUS_TRUE) \
    { \
        return trampoline(__VA_ARGS__); \
    }

/**
 * Tries to enter the barrier for the given hook and automatically returns from the callback
 * function after invoking the given `trampoline` in case of failure.
 *
 * @param   handle      The barrier hook handle
 * @param   trampoline  The trampoline.
 */
#define ZYREX_BARRIER_ENTER_PROC(handle, trampoline, ...) \
    if (ZyrexBarrierTryEnter(handle) != ZYAN_STATUS_TRUE) \
    { \
        trampoline(__VA_ARGS__); \
        return; \
    }

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Initialization and finalization                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Initializes the barrier system.
 *
 * @return  A zyan status code.
 *
 * This function must be called, before using any of the other barrier API functions.
 *
 * Calling this function multiple times might lead to unexpected behavior.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierSystemInitialize(void);

/**
 * Finalizes the barrier system.
 *
 * @return  A zyan status code.
 *
 * This function should be called before the current process exits.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierSystemShutdown(void);

/* ---------------------------------------------------------------------------------------------- */
/* Barrier                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Returns the barrier handle for the hook that is identified by the given `trampoline`.
 *
 * @param   trampoline  The `trampoline` that identifies the hook for which the barrier handle
 *                      should be obtained.
 *
 * @return  The barrier handle for the hook that is identified by the given `trampoline`.
 *
 * As the `trampoline` pointer might get modified from another thread (e.g. during hook removal),
 * it is necessary to obtain a constant barrier handle before invoking a barrier API function
 * inside a hook callback.
 *
 * The returned handle should be saved and then used for all subsequent calls to the barrier API
 * inside the current hook callback.
 */
ZYREX_EXPORT ZyrexBarrierHandle ZyrexBarrierGetHandle(const void* trampoline);

/**
 * Tries to enter the barrier for the given hook.
 *
 * @param   handle  The barrier hook handle.
 *
 * @return  `ZYAN_STATUS_TRUE` if the barrier has been passed, `ZYAN_STATUS_FALSE` if not, or a
 *          generic zyan status code if an error occured.
 *
 * This function passes the barrier, if the `current_recursion_depth` is `0`.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierTryEnter(ZyrexBarrierHandle handle);

/**
 * Tries to enter the barrier for the given hook.
 *
 * @param   handle              The barrier hook handle.
 * @param   max_recursion_depth The maximum recursion depth to pass the barrier.
 *
 * @return  `ZYAN_STATUS_TRUE` if the barrier has been passed, `ZYAN_STATUS_FALSE` if not, or a
 *          generic zyan status code if an error occured.
 *
 * This function passes the barrier, if the `current_recursion_depth` is less than or equal to
 * the given `max_recursion_depth`.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierTryEnterEx(ZyrexBarrierHandle handle,
    ZyanU32 max_recursion_depth);

/**
 * Leaves the barrier for the given hook.
 *
 * @param   handle  The barrier hook handle.
 *
 * @return  A zyan status code.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierLeave(ZyrexBarrierHandle handle);

/* ---------------------------------------------------------------------------------------------- */
/* Utils                                                                                          */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Returns the current recursion depth for the given hook.
 *
 * @param   handle          The barrier hook handle.
 * @param   current_depth   Receives the current recursion depth for the given hook or `0`, if no
 *                          barrier context was found.
 *
 * @return  `ZYAN_STATUS_TRUE` if a barrier context was found for the given hook,
 *          `ZYAN_STATUS_FALSE` if not, or a generic zyan status code if an error occurred.
 */
ZYREX_EXPORT ZyanStatus ZyrexBarrierGetRecursionDepth(ZyrexBarrierHandle handle,
    ZyanU32* current_depth);

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_BARRIER_H */
