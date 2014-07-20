/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <utils/base.h>

int gettid()
{
	return syscall (__NR_gettid);
}

extern void __ev_init ();
extern void __rex_init ();
extern void __poll_init ();
extern void __socket_init ();
extern void __sp_init ();

void __attribute__ ( (constructor) ) __modules_init (void)
{
	__ev_init ();
	__rex_init ();
	__poll_init ();
	__socket_init ();
	__sp_init ();
}


extern void __sp_exit ();
extern void __socket_exit ();
extern void __poll_exit ();
extern void __rex_exit ();
extern void __ev_exit ();

void __attribute__ ( (destructor) ) __modules_exit (void)
{
	__sp_exit ();
	__socket_exit ();
	__poll_exit ();
	__rex_exit ();
	__ev_exit ();
}
