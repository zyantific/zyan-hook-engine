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
 * @brief   Demonstrates the 'Hook Barrier' functionality.
 */

#include <stdio.h>
#include <string.h>

#include <Zyrex/Barrier.h>

/* ============================================================================================== */
/* Example functions                                                                              */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Target functions                                                                               */
/* ---------------------------------------------------------------------------------------------- */

static int ZYAN_NOINLINE LogMessage(const char* message);

/**
 * Delays execution of the current thread for a certain amount of time by performing loop
 * iterations.
 *
 * @param   iterations  The number of iterations to perform.
 */
static void ZYAN_NOINLINE Delay(int iterations)
{
    for (int i = 0; i < iterations; ++i)
    {
    }

    LogMessage("Execution has been delayed\n");
}

/**
 * Logs a message to `stdout`.
 *
 * @param   message The message to log.
 *
 * @return  The number of characters written to `stdout`.
 */
static int ZYAN_NOINLINE LogMessage(const char* message)
{
    return printf("%s", message);
}

/* ---------------------------------------------------------------------------------------------- */
/* Hook callback                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

typedef int (SignatureLogMessage)(const char* message);

static SignatureLogMessage* volatile OriginalLogMessage = &LogMessage;

/**
 * Logs a message to `stdout` and prepends the `Intercepted: ` string.
 *
 * @param   message The message to log.
 *
 * @return  The number of characters written to `stdout`.
 */
static int CallbackLogMessage(const char* message)
{
    // Obtain the barrier handle identified by the given trampoline:
    // It's important to only call `ZyrexBarrierGetHandle` once as the value of `trampoline`
    // will change if the hook got removed by a different thread between the first- and the
    // second- call.
    const ZyrexBarrierHandle barrier_handle = ZyrexBarrierGetHandle((const void*)OriginalLogMessage);

    // Try to enter the Hook Barrier
    if (ZyrexBarrierTryEnter(barrier_handle) != ZYAN_STATUS_TRUE)
    {
        // Failed to pass the barrier. Invoke the trampoline function and immediately leave the
        // callback
        return OriginalLogMessage(message);
    }

    // We successfully passed the barrier ..
    const int result = OriginalLogMessage("Intercepted: ") + OriginalLogMessage(message);

    // We can now execute a function that internally calls `LogMessage` without risking infinite
    // recursion
    Delay(1000);

    // We can even execute the original function without involving the trampoline
    LogMessage("NOT intercepted\n");

    // Leave the barrier:
    // This is very important! If we do not leave the barrier, all subsequent callback executions
    // won't be able to enter it again.
    ZyrexBarrierLeave(barrier_handle);

    return result;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Entry point                                                                                    */
/* ============================================================================================== */

int main()
{
    // Must be called once at program start!
    ZyrexBarrierSystemInitialize();

    CallbackLogMessage("Example log message 1\n");
    CallbackLogMessage("Example log message 2\n");

    ZyrexBarrierSystemShutdown();

    return 0;
}

/* ============================================================================================== */
