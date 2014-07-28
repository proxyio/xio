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

#include "base.h"
#include "mutex.h"

int mutex_init (mutex_t *mutex)
{
	int rc = 0;
	pthread_mutexattr_t mattr;
	pthread_mutex_t *lock = (pthread_mutex_t *) mutex;

	pthread_mutexattr_init (&mattr);
	rc = pthread_mutex_init (lock, &mattr);
	pthread_mutexattr_destroy (&mattr);	
	if (rc != 0) {
		ERRNO_RETURN (rc);
	}
	return 0;
}
/*
  for POSIX-API. If successful, the pthread_mutex_lock() and pthread_mutex_unlock()
  functions shall return zero; otherwise, an error number shall be returned to indicate
  the error. */
int mutex_lock (mutex_t *mutex)
{
	int rc = 0;
	pthread_mutex_t *lock = (pthread_mutex_t *) mutex;

	if ((rc = pthread_mutex_lock (lock)) != 0) {
		ERRNO_RETURN (rc);
	}
	return 0;
}

int mutex_trylock (mutex_t *mutex)
{
	int rc = 0;
	pthread_mutex_t *lock = (pthread_mutex_t *) mutex;

	if ((rc = pthread_mutex_trylock (lock)) != 0) {
		ERRNO_RETURN (rc);
	}
	return 0;
}


int mutex_unlock (mutex_t *mutex)
{
	int rc = 0;
	pthread_mutex_t *lock = (pthread_mutex_t *) mutex;

	if ((rc = pthread_mutex_unlock (lock)) != 0) {
		ERRNO_RETURN (rc);
	}
	return 0;
}


int mutex_destroy (mutex_t *mutex)
{
	int rc = 0;
	pthread_mutex_t *lock = (pthread_mutex_t *) mutex;

	if ((rc = pthread_mutex_destroy (lock)) != 0) {
		ERRNO_RETURN (rc);
	}
	return 0;
}
