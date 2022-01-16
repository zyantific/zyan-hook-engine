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

#include <Zycore/API/Thread.h>
#include <Zycore/LibC.h>
#include <Zycore/Vector.h>
#include <Zyrex/Barrier.h>

/* ============================================================================================== */
/* Globals                                                                                        */
/* ============================================================================================== */

/**
 * The TLS slot used by the barrier system.
 */
static ZyanThreadTlsIndex g_barrier_tls_index = 0;

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Barrier context                                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZyrexBarrierContext` struct.
 */
typedef struct ZyrexBarrierContext_
{
    /**
     * The barrier context id.
     */
    ZyrexBarrierHandle id;
    /**
     * The current recursion depth.
     */
    ZyanU32 recursion_depth;
} ZyrexBarrierContext;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* TLS cleanup                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

/**
 * This function is invoked every time a thread exists.
 *
 * @param   data    The data currently stored in the TLS slot.
 */
ZYAN_THREAD_DECLARE_TLS_CALLBACK(ZyrexBarrierTlsCleanup, ZyanVector, data)
{
    if (!data)
    {
        return;
    }

    ZyanVectorDestroy(data);

    // TODO: Replace with ZyanMemoryFree in the future
    ZYAN_FREE(data);
}

/* ---------------------------------------------------------------------------------------------- */
/* Barrier context                                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines a comparison function for the `ZyrexBarrierContext` struct that uses the `id` field to
 * create an absolute order.
 */
ZYAN_DECLARE_COMPARISON_FOR_FIELD(ZyrexBarrierCompareContext, ZyrexBarrierContext, id)

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Initialization and finalization                                                                */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexBarrierSystemInitialize()
{
    return ZyanThreadTlsAlloc(&g_barrier_tls_index, (ZyanThreadTlsCallback)&ZyrexBarrierTlsCleanup);
}

ZyanStatus ZyrexBarrierSystemShutdown()
{
    return ZyanThreadTlsFree(g_barrier_tls_index);
}

/* ---------------------------------------------------------------------------------------------- */
/* Barrier                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

ZyrexBarrierHandle ZyrexBarrierGetHandle(const void* trampoline)
{
    return (ZyrexBarrierHandle)trampoline;
}

ZyanStatus ZyrexBarrierTryEnter(ZyrexBarrierHandle handle)
{
    return ZyrexBarrierTryEnterEx(handle, 0);
}

ZyanStatus ZyrexBarrierTryEnterEx(ZyrexBarrierHandle handle, ZyanU32 max_recursion_depth)
{
    ZyanVector* vector;
    ZYAN_CHECK(ZyanThreadTlsGetValue(g_barrier_tls_index, (void*)&vector));

    if (vector == ZYAN_NULL)
    {
        // TODO: Replace with ZyanMemoryAlloc in the future
        vector = ZYAN_MALLOC(sizeof(ZyanVector));
        ZYAN_CHECK(ZyanVectorInit(vector, sizeof(ZyrexBarrierContext), 32, ZYAN_NULL));
        ZYAN_CHECK(ZyanThreadTlsSetValue(g_barrier_tls_index, vector));
    }

    ZyrexBarrierContext context_element;
    context_element.id = handle;

    ZyanUSize found_index;
    const ZyanStatus status =
        ZyanVectorBinarySearch(vector, &context_element, &found_index,
            (ZyanComparison)&ZyrexBarrierCompareContext);
    ZYAN_CHECK(status);

    if (status == ZYAN_STATUS_TRUE)
    {
        ZyrexBarrierContext* const context =
            (ZyrexBarrierContext*)ZyanVectorGetMutable(vector, found_index);
        if (context->recursion_depth > max_recursion_depth)
        {
            return ZYAN_STATUS_FALSE;
        }

        ++context->recursion_depth;
        return ZYAN_STATUS_TRUE;
    }

    context_element.recursion_depth = 1;
    ZYAN_CHECK(ZyanVectorInsert(vector, found_index, &context_element));

    return ZYAN_STATUS_TRUE;
}

ZyanStatus ZyrexBarrierLeave(ZyrexBarrierHandle handle)
{
    ZyanVector* vector;
    ZYAN_CHECK(ZyanThreadTlsGetValue(g_barrier_tls_index, (void*)&vector));

    if (vector == ZYAN_NULL)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZyrexBarrierContext context_element;
    context_element.id = handle;

    ZyanUSize found_index;
    const ZyanStatus status =
        ZyanVectorBinarySearch(vector, &context_element, &found_index,
            (ZyanComparison)&ZyrexBarrierCompareContext);
    ZYAN_CHECK(status);

    if (status != ZYAN_STATUS_TRUE)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    ZyrexBarrierContext* const context =
        (ZyrexBarrierContext*)ZyanVectorGetMutable(vector, found_index);
    if (context->recursion_depth == 0)
    {
        return ZYAN_STATUS_INVALID_OPERATION;
    }

    if (--context->recursion_depth == 0)
    {
        ZYAN_CHECK(ZyanVectorDelete(vector, found_index));
    }

    return ZYAN_STATUS_TRUE;
}

/* ---------------------------------------------------------------------------------------------- */
/* Utils                                                                                          */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyrexBarrierGetRecursionDepth(ZyrexBarrierHandle handle, ZyanU32* current_depth)
{
    ZyanVector* vector;
    ZYAN_CHECK(ZyanThreadTlsGetValue(g_barrier_tls_index, (void*)&vector));

    if (vector == ZYAN_NULL)
    {
        *current_depth = 0;
        return ZYAN_STATUS_FALSE;
    }

    ZyrexBarrierContext context_element;
    context_element.id = handle;

    ZyanUSize found_index;
    const ZyanStatus status =
        ZyanVectorBinarySearch(vector, &context_element, &found_index,
            (ZyanComparison)&ZyrexBarrierCompareContext);
    ZYAN_CHECK(status);

    if (status == ZYAN_STATUS_TRUE)
    {
        const ZyrexBarrierContext* const context =
            (ZyrexBarrierContext*)ZyanVectorGetMutable(vector, found_index);

        *current_depth = context->recursion_depth;
        return ZYAN_STATUS_TRUE;
    }

    return ZYAN_STATUS_FALSE;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
