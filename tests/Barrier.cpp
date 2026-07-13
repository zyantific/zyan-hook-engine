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
 * @brief   Tests that ZyrexInitialize wires up the barrier subsystem.
 */

#include <gtest/gtest.h>
#include <Zyrex/Zyrex.h>
#include <Zyrex/Barrier.h>

TEST(BarrierTest, InitializeEnablesBarrierApi)
{
    ASSERT_EQ(ZyrexInitialize(), ZYAN_STATUS_SUCCESS);

    // After ZyrexInitialize (which must have run ZyrexBarrierSystemInitialize), the barrier API
    // works without a manual system-init call: first entry for a handle passes, nested entry is
    // blocked at the default recursion depth of 0.
    const ZyrexBarrierHandle handle = ZyrexBarrierGetHandle((const void*)0x1234);
    EXPECT_EQ(ZyrexBarrierTryEnter(handle), ZYAN_STATUS_TRUE);
    EXPECT_EQ(ZyrexBarrierTryEnter(handle), ZYAN_STATUS_FALSE);
    EXPECT_EQ(ZyrexBarrierLeave(handle), ZYAN_STATUS_TRUE);

    ASSERT_EQ(ZyrexShutdown(), ZYAN_STATUS_SUCCESS);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
