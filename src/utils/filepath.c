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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base.h"
#include "filepath.h"

/* TODO: remove platform specified code from here */
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

char *fp_join (const char *p1, const char *p2)
{
	int len = strlen (p1) + strlen (p2) + 2;
	char *p3 = NULL;

	if (! (p3 = mem_zalloc (len)))
		goto EXIT;
	snprintf (p3, len, "%s/%s", p1, p2);
EXIT:
	return p3;
}

char *fp_abs (const char *path)
{
	char __cwd[PATH_MAX] = {}, *abs = NULL;

	getcwd (__cwd, PATH_MAX);
	if (strlen (path) == 0)
		return strdup (__cwd);
	if (path[0] == '/')
		return strdup (path);
	abs = fp_join (__cwd, path);
	return abs;
}

char *fp_base (const char *path)
{
	char *p = strdup (path), *base = NULL;

	if (!p)
		return NULL;
	base = strdup (basename (p));
	free (p);
	return base;
}

char *fp_dir (const char *path)
{
	char *p = strdup (path), *dir = NULL;

	if (!p)
		return NULL;
	dir = strdup (dirname (p));
	free (p);
	return dir;
}

int fp_isabs (const char *path)
{
	if (strlen (path) == 0 || path[0] != '/')
		return false;
	return true;
}

int fp_hasprefix (const char *path, const char *prefix)
{
	return strlen (path) >= strlen (prefix) && memcmp (path, prefix, strlen (prefix)) == 0;
}

int fp_hassuffix (const char *path, const char *suffix)
{
	return strlen (path) >= strlen (suffix) &&
	       memcmp (path + strlen (path) - strlen (suffix), suffix, strlen (suffix)) == 0;
}


static int __walk (filepath_t *fp, const char *root, walkFn f, void *args)
{
	char *subpath;
	DIR *dp = NULL;
	struct stat finfo = {};
	struct dirent *dir = NULL;

	if ((dp = opendir (root)) == NULL)
		return -1;
	while ((dir = readdir (dp)) != NULL) {
		if (STREQ (dir->d_name, ".") || STREQ (dir->d_name, ".."))
			continue;
		subpath = fp_join (root, dir->d_name);
		if (stat (subpath, &finfo) < 0) {
			free (subpath);
			continue;
		}
		if (((fp->mask & W_FILE) && S_ISREG (finfo.st_mode))
		     || ((fp->mask & W_DIR) && S_ISDIR (finfo.st_mode))
		     || (fp->mask & (W_FILE|W_DIR)) == (W_FILE|W_DIR))
			f (subpath, args);
		if (S_ISDIR (finfo.st_mode)) {
			fp->cur_deep++;
			if (fp->max_deep == 0 || (fp->max_deep > 0 && fp->cur_deep < fp->max_deep))
				__walk (fp, subpath, f, args);
			fp->cur_deep--;
		}
		free (subpath);
	}
	closedir (dp);
	return 0;
}


int filepath_walk (filepath_t *fp, walkFn f, void *args, int mask, int deep)
{
	fp->max_deep = deep;
	fp->mask = mask;
	return __walk (fp, fp->root, f, args);
}
