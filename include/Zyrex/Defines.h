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
 * @brief   General helper and platform detection macros.
 */

#ifndef ZYREX_DEFINES_H
#define ZYREX_DEFINES_H

#include <ZyrexExportConfig.h>

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
#   define ZYREX_UNKNOWN_PLATFORM
#endif

/* ============================================================================================== */
/* Architecture detection                                                                         */
/* ============================================================================================== */

#if defined(_M_AMD64) || defined(__x86_64__)
#   define ZYREX_X64
#elif defined(_M_IX86) || defined(__i386__)
#   define ZYREX_X86
#elif defined(_M_ARM64) || defined(__aarch64__)
#   define ZYREX_AARCH64
#elif defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__) || defined(__thumb__)
#   define ZYREX_ARM
#else
#   error "Unsupported architecture detected"
#endif

/* ============================================================================================== */
/* Debug/Release detection                                                                        */
/* ============================================================================================== */  

#if defined(ZYREX_MSVC) || defined(ZYREX_BORLAND)
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
#   define ZYREX_RELEASE
#endif

/* ============================================================================================== */
/* Misc compatibility macros                                                                      */
/* ============================================================================================== */

#if defined(ZYREX_MSVC) || defined(ZYREX_BORLAND)
#   define ZYREX_INLINE __inline 
#else
#   define ZYREX_INLINE static inline
#endif

/* ============================================================================================== */
/* Debugging and optimization macros                                                              */
/* ============================================================================================== */

#if defined(ZYREX_NO_LIBC)
#   define ZYREX_ASSERT(condition)
#else
#   include <assert.h>
#   define ZYREX_ASSERT(condition) assert(condition)
#endif

#if defined(ZYREX_RELEASE)
#   if defined(ZYREX_CLANG) // GCC eagerly evals && RHS, we have to use nested ifs.
#       if __has_builtin(__builtin_unreachable)
#           define ZYREX_UNREACHABLE __builtin_unreachable()
#       else
#           define ZYREX_UNREACHABLE for(;;)
#       endif
#   elif defined(ZYREX_GCC) && ((__GNUC__ == 4 && __GNUC_MINOR__ > 4) || __GNUC__ > 4)
#       define ZYREX_UNREACHABLE __builtin_unreachable()
#   elif defined(ZYREX_ICC)
#       ifdef ZYREX_WINDOWS
#           include <stdlib.h> // "missing return statement" workaround
#           define ZYREX_UNREACHABLE __assume(0); (void)abort()
#       else
#           define ZYREX_UNREACHABLE __builtin_unreachable()
#       endif
#   elif defined(ZYREX_MSVC)
#       define ZYREX_UNREACHABLE __assume(0)
#   else
#       define ZYREX_UNREACHABLE for(;;)
#   endif
#elif defined(ZYREX_NO_LIBC)
#   define ZYREX_UNREACHABLE for(;;)
#else
#   include <stdlib.h>
#   define ZYREX_UNREACHABLE { assert(0); abort(); }
#endif

/* ============================================================================================== */
/* Utils                                                                                          */
/* ============================================================================================== */

/**
 * @brief   Compiler-time assertion.
 */
#if __STDC_VERSION__ >= 201112L
#   define ZYREX_STATIC_ASSERT(x) _Static_assert(x, #x)
#else
#   define ZYREX_STATIC_ASSERT(x) typedef int ZYREX_SASSERT_IMPL[(x) ? 1 : -1]
#endif

/**
 * @brief   Declares a bitfield.
 */
#define ZYREX_BITFIELD(x) : x

/**
 * @brief   Marks the specified parameter as unused.
 */
#define ZYREX_UNUSED_PARAMETER(x) (void)(x)

/**
 * @brief   Calculates the size of an array.
 */
#define ZYREX_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

/* ============================================================================================== */

#endif /* ZYREX_DEFINES_H */
