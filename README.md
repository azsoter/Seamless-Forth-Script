# Seamless-Forth-Script
 A Forth based scripting language that integrates seamlessly with C.

You can check out my other forth project at https://github.com/azsoter/forth-devel
which is a Forth implementation that is perhaps pleasing to a Forth programmer's eyes,
but as it turns out it is somewhat clumsy to integrate with C code where lots of
functions need to be called from Forth.
This version is perhaps less elegant as a Forth system, but should integrate
more seamlessly with C code.

The idea here is to add live scripting to embedded programs written in C,
not to implement entire systems in Forth.

The test code included in this repository shows an application where I/O is using
standard input and output.
But the real use cases are where the Forth engine is compiled into some firmware
possibly running on an RTOS and Forth is accessed wia some other interface
(e.g. a serial port would do), the clearly defined terminal interface needs
to have the appropriate functions hooked up and it is good to go.

I myself have already run it in such an environment in an embedded project
that is not public.
Here I am giving here a simple example that anyone with a computer and a
C compiler can run (I have used GCC in all cases where I was working with it,
so other compilers need to be tested, but apart from some macros in the default
config header files that may not be defined by all compilers everything
should be portable C).

Copyright © András Zsótér, 2023

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.