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

#include <Windows.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Internal/Trampoline.h>
#include <Zyrex/Internal/Utils.h>

/* ============================================================================================== */
/* Defines                                                                                        */
/* ============================================================================================== */

/**
 * @brief   Defines the maximum amount of instruction bytes that can be saved to a trampoline.
 */
#define ZYREX_TRAMPOLINE_CODE_SIZE          27

/**
 * @brief   Defines the maximum amount of instruction bytes that can be restored from a trampoline.
 */
#define ZYREX_TRAMPOLINE_RESTORE_SIZE       19

/**
 * @brief   Defines the maximum amount of items in the trampoline instruction translation map.
 *          Each saved instruction requires one slot in the map.
 */
#define ZYREX_TRAMPOLINE_INSTRUCTION_COUNT  5

/**
 * @brief   Defines the trampoline region signature.
 */
#define ZYREX_TRAMPOLINE_REGION_SIGNATURE   'zrex'

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */



/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */



/* ============================================================================================== */
/* Public functions                                                                               */
/* ============================================================================================== */

ZyanStatus ZyrexTrampolineCreate(const void* address, ZyanUSize size, const void* callback,
    const void** trampoline)
{
     ZYAN_UNUSED(address);
     ZYAN_UNUSED(size);
     ZYAN_UNUSED(callback);
     ZYAN_UNUSED(trampoline);

#ifdef ZYAN_MSVC
    __try
#endif
    {

    }
#ifdef ZYAN_MSVC
    __except(EXCEPTION_EXECUTE_HANDLER)
#endif
    {

    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyrexTrampolineFree(const void* trampoline)
{
    ZYAN_UNUSED(trampoline);

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
