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

#include "mix.h"

static int get_noblock (struct sockbase *sb, void *optval, int *optlen)
{
	mutex_lock (&sb->lock);
	* (int *) optval = sb->flagset.non_block ? true : false;
	mutex_unlock (&sb->lock);
	return 0;
}

static int get_socktype (struct sockbase *sb, void *optval, int *optlen)
{
	mutex_lock (&sb->lock);
	* (int *) optval = sb->vfptr->type;
	mutex_unlock (&sb->lock);
	return 0;
}

static int get_sockpf (struct sockbase *sb, void *optval, int *optlen)
{
	mutex_lock (&sb->lock);
	* (int *) optval = sb->vfptr->pf;
	mutex_unlock (&sb->lock);
	return 0;
}

static int get_verbose (struct sockbase *sb, void *optval, int *optlen)
{
	mutex_lock (&sb->lock);
	* (int *) optval = sb->flagset.verbose;
	mutex_unlock (&sb->lock);
	return 0;
}


static const sock_geter getopt_vfptr[] = {
	0,
	0,
	0,
	get_noblock,
	0,
	0,
	0,
	get_socktype,
	get_sockpf,
	get_verbose,
};

int mix_getopt (struct sockbase *sb, int opt, void *optval, int *optlen)
{
	int rc;
	if (opt >= NELEM (getopt_vfptr, sock_geter) || !getopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = getopt_vfptr[opt] (sb, optval, optlen);
	return rc;
}



static int set_noblock (struct sockbase *sb, void *optval, int optlen)
{
	mutex_lock (&sb->lock);
	sb->flagset.non_block = * (int *) optval ? true : false;
	mutex_unlock (&sb->lock);
	return 0;
}

static int set_verbose (struct sockbase *sb, void *optval, int optlen)
{
	mutex_lock (&sb->lock);
	sb->flagset.verbose =  * (int *) optval & 0xf;
	mutex_unlock (&sb->lock);
}
	
static const sock_seter setopt_vfptr[] = {
	0,
	0,
	0,
	set_noblock,
	0,
	0,
	0,
	0,
	0,
	set_verbose,
};

int mix_setopt (struct sockbase *sb, int opt, void *optval, int optlen)
{
	int rc;
	if (opt >= NELEM (setopt_vfptr, sock_seter) || !setopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = setopt_vfptr[opt] (sb, optval, optlen);
	return rc;
}
