#ifndef FORTH_CONFIG_H
#define FORTH_CONFIG_H
/*
* forth_config.h
*
*  Created on: Sep 16, 2023
*      Author: Andras Zsoter
*
* Copyright (c) 2023 Andras Zsoter
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*/

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_LP64) || defined(__LP64__)
#define FORTH_IS_64BIT 1
#elif  defined(_LP32) || defined(__LP32__)
#define FORTH_IS_32BIT 1
#else
#endif

#define FORTH_TIB_SIZE 256
#define FORTH_ALLOW_0X_HEX 1

// #define FORTH_EXCLUDE_DESCRIPTIONS 1
// #define FORTH_NO_DOUBLES 1
// #define FORTH_WITHOUT_COMPILATION 1

#define FORTH_INCLUDE_BLOCKS 1
#if defined(FORTH_INCLUDE_BLOCKS)
#   define FORTH_INCLUDE_BLOCK_EDITOR
#   define FORTH_BLOCK_BUFFERS_COUNT 3
#   define FORTH_MAX_BLOCKS 256
#endif

#if !defined(FORTH_WITHOUT_COMPILATION)
#define FORTH_INCLUDE_LOCALS 1
#endif

#include <forth_config_default.h>

// Get a human readable message for for non-standard (system defined) exceptions.
// #define FORTH_RETRIEVE_SYSTEM_DEFINED_ERROR_MESSAGE(CODE) forth_retrieve_system_defined_error_message(CODE)

#if defined(FORTH_RETRIEVE_SYSTEM_DEFINED_ERROR_MESSAGE)
extern const char *forth_retrieve_system_defined_error_message(int code);
#endif

#ifdef __cplusplus
}
#endif

#endif
