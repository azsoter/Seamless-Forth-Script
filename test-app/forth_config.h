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

#if defined(_LP64) || defined(__LP64__)
#define FORTH_IS_64BIT 1
#elif  defined(_LP32) || defined(__LP32__)
#define FORTH_IS_32BIT 1
#else
#endif

#include <forth_config_default.h>

#define FORTH_TIB_SIZE 256
#define FORTH_ALLOW_0X_HEX 1

// #define FORTH_EXCLUDE_DESCRIPTIONS 1
// #define FORTH_NO_DOUBLES 1
#define FORTH_WITHOUT_COMPILATION 1

#endif
