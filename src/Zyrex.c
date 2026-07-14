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

#include <Zycore/Zycore.h>
#include <Zydis/Zydis.h>
#include <Zyrex/Zyrex.h>
#include <Zyrex/Barrier.h>
#include <Zyrex/Internal/Trampoline.h>

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Initialization & Finalization                                                                  */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexInitialize(void)
{
    if (ZycoreGetVersion() != ZYCORE_VERSION)
    {
        return ZYAN_STATUS_MISSING_DEPENDENCY;    
    }
    if (ZydisGetVersion() != ZYDIS_VERSION)
    {
        return ZYAN_STATUS_MISSING_DEPENDENCY;     
    }
    if (!ZydisIsFeatureEnabled(ZYDIS_FEATURE_DECODER))
    {
        return ZYAN_STATUS_MISSING_DEPENDENCY;
    }

    return ZyrexBarrierSystemInitialize();
}

ZyanStatus ZyrexShutdown(void)
{
    return ZyrexShutdownEx(ZYREX_SHUTDOWN_FLAG_NONE);
}

ZyanStatus ZyrexShutdownEx(ZyanU32 flags)
{
    ZyanStatus status = ZYAN_STATUS_SUCCESS;
    if (flags & ZYREX_SHUTDOWN_FLAG_RELEASE_TRAMPOLINES)
    {
        status = ZyrexTrampolineReleaseAll();
    }

    // Always shut the barrier system down; surface the first failure.
    const ZyanStatus barrier_status = ZyrexBarrierSystemShutdown();
    return ZYAN_SUCCESS(status) ? barrier_status : status;
}

/* ---------------------------------------------------------------------------------------------- */
/* Information                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

ZyanU64 ZyrexGetVersion(void)
{
    return ZYREX_VERSION;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
