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

#ifndef _H_PROXYIO_STR_ARRAY_
#define _H_PROXYIO_STR_ARRAY_

#include <utils/base.h>
#include <utils/alloc.h>

#define STR_ARRAY_DEFAULT_CAP 10

struct str_array {
    int size;
    int cap;
    char** at;
};

static inline void str_array_init(struct str_array* arr) {
    arr->size = 0;
    arr->cap = STR_ARRAY_DEFAULT_CAP;

    if (!(arr->at = mem_zalloc(sizeof(char*) * arr->cap))) {
        BUG_ON(1);
    }
}

static inline void str_array_destroy(struct str_array* arr) {
    int i;

    for (i = 0; i < arr->size; i++) {
        mem_free(arr->at[i], strlen(arr->at[i]));
    }

    mem_free(arr->at, sizeof(char*) * arr->cap);
    ZERO(arr);
}

void str_array_add(struct str_array* arr, const char* str);
void str_split(const char* haystack, struct str_array* arr, const char* needle);



#endif
