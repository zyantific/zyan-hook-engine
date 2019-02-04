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

/**
 * @file
 * @brief   Status code definitions and check macros.
 */

#ifndef ZYREX_STATUS_H
#define ZYREX_STATUS_H

#include <Zycore/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Status codes                                                                                   */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Module IDs                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   The zydis module id.
 */
#define ZYAN_MODULE_ZYREX   0x200

/* ---------------------------------------------------------------------------------------------- */
/* Status codes                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Could not allocate a suitable trampoline memory region.
 */
#define ZYREX_STATUS_COULD_NOT_ALLOCATE_TRAMPOLINE \
    ZYAN_MAKE_STATUS(1, ZYAN_MODULE_ZYDIS, 0x00)

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_STATUS_H */
