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

#ifndef ZYREX_INTERNAL_RELOCATION_H
#define ZYREX_INTERNAL_RELOCATION_H

#include <Zycore/Types.h>
#include <Zyrex/Internal/Trampoline.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/*                                                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Copies all instructions from the `source` address to the given `trampoline` chunk
 *          until at least `min_bytes_to_reloc` bytes has been moved.
 *
 * @param   source              A pointer to the source buffer.
 * @param   source_length       The maximum amount of bytes that can be safely read from the 
 *                              source buffer.
 * @param   trampoline          A pointer to the destination trampoline chunk.
 * @param   min_bytes_to_reloc  Specifies the minimum amount of bytes that should be relocated.
 *                              This function might copy more bytes on demand to keep individual
 *                              instructions intact.
 * @param   bytes_read          Returns the number of bytes read from the source buffer.
 * @param   bytes_written       Returns the number of bytes written to the destination buffer.
 *
 * @return  A zyan status code.
 */
ZyanStatus ZyrexRelocateCode(const void* source, ZyanUSize source_length, 
    ZyrexTrampolineChunk* trampoline, ZyanUSize min_bytes_to_reloc, ZyanUSize* bytes_read, 
    ZyanUSize* bytes_written);

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYREX_INTERNAL_RELOCATION_H */
