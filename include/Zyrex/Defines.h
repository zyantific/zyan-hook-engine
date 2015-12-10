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

#ifndef _ZYREX_DEFINES_H_
#define _ZYREX_DEFINES_H_

/* ============================================================================================== */
/* Compiler detection                                                                             */
/* ============================================================================================== */

#if defined(__clang__)
#   define ZYREX_CLANG
#   define ZYREX_GNUC
#elif defined(__ICC) || defined(__INTEL_COMPILER)
#   define ZYREX_ICC
#elif defined(__GNUC__) || defined(__GNUG__)
#   define ZYREX_GCC
#   define ZYREX_GNUC
#elif defined(_MSC_VER)
#   define ZYREX_MSVC
#elif defined(__BORLANDC__)
#   define ZYREX_BORLAND
#else
#   define ZYREX_UNKNOWN_COMPILER
#endif

/* ============================================================================================== */
/* Platform detection                                                                             */
/* ============================================================================================== */

#if defined(_WIN32)
#   define ZYREX_WINDOWS
#elif defined(__APPLE__)
#   define ZYREX_APPLE
#   define ZYREX_POSIX
#elif defined(__linux)
#   define ZYREX_LINUX
#   define ZYREX_POSIX
#elif defined(__unix)
#   define ZYREX_UNIX
#   define ZYREX_POSIX
#elif defined(__posix)
#   define ZYREX_POSIX
#else
#   error "Unsupported platform detected"
#endif

/* ============================================================================================== */
/* Architecture detection                                                                         */
/* ============================================================================================== */

#if defined (_M_AMD64) || defined (__x86_64__)
#   define ZYREX_X64
#elif defined (_M_IX86) || defined (__i386__)
#   define ZYREX_X86
#else
#   error "Unsupported architecture detected"
#endif

/* ============================================================================================== */
/* Debug/Release detection                                                                        */
/* ============================================================================================== */  

#if defined(ZYREX_MSVC)
#   ifdef _DEBUG
#       define ZYREX_DEBUG
#   else
#       define ZYREX_RELEASE
#   endif
#elif defined(ZYREX_GNUC) || defined(ZYREX_ICC)
#   ifdef NDEBUG
#       define ZYREX_RELEASE
#   else
#       define ZYREX_DEBUG
#   endif
#else
#   error "Unsupported compiler detected"
#endif

/* ============================================================================================== */
/* Function definition macros                                                                     */
/* ============================================================================================== */

#if defined(ZYREX_MSVC) || defined(ZYREX_BORLAND)
#   define ZYREX_INLINE __inline 
#else
#   define ZYREX_INLINE inline
#endif

/* ---------------------------------------------------------------------------------------------- */

#if defined(ZYREX_MSVC) && defined(ZYREX_DLL)
#   ifdef ZYREX_EXPORTS
#       define ZYREX_DLLEXTERN __declspec(dllexport)
#   else 
#       define ZYREX_DLLEXTERN __declspec(dllimport)
#   endif
#else
#   define ZYREX_DLLEXTERN 
#endif

/* ============================================================================================== */

#endif // _ZYREX_DEFINES_H_
