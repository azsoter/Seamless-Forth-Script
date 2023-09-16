/*
* forth_search.c
*
*  Created on: Feb 01, 2023
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
#include <string.h>

#include <forth.h>
#include <forth_internal.h>

// Returns -1 (true) if the names match.
// Returns 0 (false) if the names do not match.
int forth_compare_names(const char *name, const char *input_word, int input_word_length)
{
    int l = strlen(name);

    if (l != input_word_length)
    {
        return 0; // Length does not match, they are not the same name.
    }

    return strncasecmp(name, input_word, input_word_length) ? 0 : -1;
}

const forth_vocabulary_entry_t *forth_SEARCH_LIST(const forth_vocabulary_entry_t *list, const char *name, int name_length)
{
    const forth_vocabulary_entry_t *p = list;

    if ((0 == p) || (0 == name))
    {
        return 0;
    }

    while (0 != p->name)
    {
        if (forth_compare_names(p->name, name, name_length))
        {
            return p;
        }
        p++;
    }

    return 0;
}

const forth_vocabulary_entry_t *forth_master_list_of_lists[] = {
    forth_wl_forth,
    0
};

// https://forth-standard.org/proposals/find-name
// ( c-addr len -- xt|0 )
void forth_find_name(struct forth_runtime_context *ctx)
{
    const forth_vocabulary_entry_t **wl = forth_master_list_of_lists;
    const forth_vocabulary_entry_t *ep = 0;
    forth_cell_t len = forth_POP(ctx);
    forth_cell_t addr = forth_POP(ctx);

    //ep = forth_SEARCH_LIST(forth_wl_forth, (char *)addr, (int)len);
    while (0 != *wl)
    {
       ep = forth_SEARCH_LIST(*wl, (char *)addr, (int)len);
       if (0 != ep)
       {
        break;
       }
       wl++;
    }

    // Pushing the address like this overrides (discards) the const qualifier from the poiter.
    // This is the lesser of two evils.
    // The alternative would be to make the entire serach mechanism consume and return non const decorated pointers.
    // But then the entire array where the list of words is stored woudl have to be non const which may place it to a writable
    // memory section.
    // On systems where e.g. flash is cheap and RAM is expensive that would not be good.

    forth_PUSH(ctx, (forth_cell_t)ep);
}
