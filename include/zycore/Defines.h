/***************************************************************************************************

  Zyan Core Library
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

#ifndef ZYCORE_DEFINES_H
#define ZYCORE_DEFINES_H

/* ============================================================================================== */
/* Compiler detection                                                                             */
/* ============================================================================================== */

#if defined(__clang__)
#   define ZYCORE_CLANG
#   define ZYCORE_GNUC
#elif defined(__ICC) || defined(__INTEL_COMPILER)
#   define ZYCORE_ICC
#elif defined(__GNUC__) || defined(__GNUG__)
#   define ZYCORE_GCC
#   define ZYCORE_GNUC
#elif defined(_MSC_VER)
#   define ZYCORE_MSVC
#elif defined(__BORLANDC__)
#   define ZYCORE_BORLAND
#else
#   define ZYCORE_UNKNOWN_COMPILER
#endif

/* ============================================================================================== */
/* Platform detection                                                                             */
/* ============================================================================================== */

#if defined(_WIN32)
#   define ZYCORE_WINDOWS
#elif defined(__APPLE__)
#   define ZYCORE_APPLE
#   define ZYCORE_POSIX
#elif defined(__linux)
#   define ZYCORE_LINUX
#   define ZYCORE_POSIX
#elif defined(__unix)
#   define ZYCORE_UNIX
#   define ZYCORE_POSIX
#elif defined(__posix)
#   define ZYCORE_POSIX
#else
#   error "Unsupported platform detected"
#endif

/* ============================================================================================== */
/* Architecture detection                                                                         */
/* ============================================================================================== */

#if defined (_M_AMD64) || defined (__x86_64__)
#   define ZYCORE_X64
#elif defined (_M_IX86) || defined (__i386__)
#   define ZYCORE_X86
#else
#   error "Unsupported architecture detected"
#endif

/* ============================================================================================== */
/* Debug/Release detection                                                                        */
/* ============================================================================================== */  

#if defined(ZYCORE_MSVC) || defined(ZYCORE_BORLAND)
#   ifdef _DEBUG
#       define ZYCORE_DEBUG
#   else
#       define ZYCORE_RELEASE
#   endif
#elif defined(ZYCORE_GNUC) || defined(ZYCORE_ICC)
#   ifdef NDEBUG
#       define ZYCORE_RELEASE
#   else
#       define ZYCORE_DEBUG
#   endif
#else
#   error "Unsupported compiler detected"
#endif

/* ============================================================================================== */
/* Misc compatibility macros                                                                      */
/* ============================================================================================== */

#if defined(ZYCORE_MSVC) || defined(ZYCORE_BORLAND)
#   define ZYCORE_INLINE __inline 
#else
#   define ZYCORE_INLINE inline
#endif

/* ============================================================================================== */

#endif /* ZYCORE_DEFINES_H */
