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

#include <stdlib.h>
#include <string.h>
#include "str_array.h"

void str_array_add(struct str_array* arr, const char* str) {
    char** at = 0;

    if (arr->size == arr->cap) {
        if (!(at = mem_zalloc(sizeof(char*) * arr->cap * 2))) {
            BUG_ON(1);
        }

        memcpy(at, arr->at, sizeof(char*) * arr->size);
        mem_free(arr->at, sizeof(char*) * arr->cap);
        arr->at = at;
        arr->cap = arr->cap * 2;
    }

    arr->at[arr->size] = mem_zalloc(strlen(str) + 1);
    strcpy(arr->at[arr->size], str);
    arr->size++;
}


void str_split(const char* haystack, struct str_array* arr, const char* needle) {
    char* hs = strdup(haystack);
    char* cur = hs, *pos;
    int needle_leng = strlen(needle);

    while ((pos = strstr(cur, needle))) {
        pos[0] = 0;
        str_array_add(arr, cur);
        cur = pos + needle_leng;
    }

    str_array_add(arr, cur);
    free(hs);
}
