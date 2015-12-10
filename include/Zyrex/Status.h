/***************************************************************************************************

  Zyan Hook Engine (Zyrex)
  Version 1.0

  Remarks         : Freeware, Copyright must be included

  Original Author : Florian Bernd
  Modifications   :

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

#ifndef ZYREX_STATUS_H
#define ZYREX_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Common Types                                                                                   */
/* ============================================================================================== */

/**
 * @brief   Defines an alias representing a zyrex status code.
 */
typedef uint32_t ZyrexStatus;

/* ============================================================================================== */
/* Status Codes                                                                                   */
/* ============================================================================================== */

/**
 * @brief   Values that represent zyrex status codes.
 */
typedef enum ZyrexStatusCode
{
    /**
     * @brief   No error occured.
     */
    ZYREX_ERROR_SUCCESS,
    /**
     * @brief   An invalid parameter was passed to a function.
     */
    ZYREX_ERROR_INVALID_PARAMETER,
    /**
     * @brief   A caller tried to perform an invalid operation.
     */
    ZYREX_ERROR_INVALID_OPERATION,
    /**
     * @brief   The application is out of memory.
     */
    ZYREX_ERROR_NOT_ENOUGH_MEMORY,
    /**
     * @brief   An error occured during a system function call.
     */
    ZYREX_ERROR_SYSTEMCALL,
    /**
     * @brief   A search operation could not find a specific element.
     */
    ZYREX_ERROR_NOT_FOUND,
    /**
     * @brief   Could not find a suitable trampoline memory region.
     */
    ZYREX_ERROR_TRAMPOLINE_ADDRESS,
    /**
     * @brief   An error occured while disassembling an instruction or code block.
     */
    ZYREX_ERROR_DISASSEMBLER,
    /**
     * @brief   A relative instruction could not get enlarged.
     */
    ZYREX_ERROR_NOT_ENLARGEABLE,
    /**
     * @brief   A relative instruction could not get relocated.
     */
    ZYREX_ERROR_NOT_RELOCATEABLE,
    /**
     * @brief   The target code is not hookable.
     */
    ZYREX_ERROR_NOT_HOOKABLE
} ZyrexStatusCode;

/* ============================================================================================== */
/* Macros                                                                                         */
/* ============================================================================================== */

/**
 * @brief   Checks a zyrex status code.
 *
 * @param   status  The zyrex status code.
 */
#define ZYREX_SUCCESS(status) (status == ZYREX_ERROR_SUCCESS)

/**
 * @brief   Checks a zyrex status code and returns it, if an error occured.
 *
 * @param   status  The zyrex status code.
 */
#define ZYREX_CHECK(status) \
    if (!ZYREX_SUCCESS(status)) \
    { \
        return status; \
    }

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif
#endif /*ZYREX_STATUS_H */
