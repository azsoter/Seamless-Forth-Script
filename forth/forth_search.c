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

// Compare a name (stored as a zero terminad string) to an input token (given as a c-addr, length pair)
// if they match (compare the same in a case insensitive comparison).
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

// Find a name in a list (array) of names.
// Return the address of the entry that matches the name or zero (NULL) if the name is not found.
const forth_vocabulary_entry_t *forth_SEARCH_LIST(const forth_vocabulary_entry_t *list, const char *name, int name_length)
{
    const forth_vocabulary_entry_t *p;

    if ((0 == list) || (0 == name) || (0 == name_length))
    {
        return 0;
    }

    for (p = list; 0 != p->name; p++)
    {
        if (forth_compare_names((char *)(p->name), name, name_length))
        {
            return p;
        }
    }

    return 0;
}

// Find a name in the live (user defined) forth dictionary (which is a linked list, not an array).
// Return the address of the entry that matches the name or zero (NULL) if the name is not found.
forth_vocabulary_entry_t *forth_SEARCH_DICTIONARY(forth_dictionary_t *dictionary, const char *name, int name_length)
{
    forth_vocabulary_entry_t *p;

    if (0 == dictionary)
    {
        return 0;
    }
   
	for (p = (forth_vocabulary_entry_t *)(dictionary->latest); 0 != p;  p = (forth_vocabulary_entry_t *)(p->link))
	{
        if (0 != p->name)
        {
            if (forth_compare_names((char *)(p->name), name, name_length))
            {
                return p;
            }  
        }  
    }

    return p;
}

// The master list of all arrays that contain (compiled in) entires of Forth words (usually implemented in C).
const forth_vocabulary_entry_t *forth_master_list_of_lists[] = {
    forth_wl_forth,
    0
};

// Search all available lists for the name (passed as a c-addr, len pain or the data stack).
// If the name is found return the corresponding execution token.
// If the name is not found return 0.
// https://forth-standard.org/proposals/find-name
//
// FIND-NAME ( c-addr len -- xt|0 )
//
void forth_find_name(struct forth_runtime_context *ctx)
{
    const forth_vocabulary_entry_t **wl;
    const forth_vocabulary_entry_t *ep = 0;
    forth_cell_t len = forth_POP(ctx);
    forth_cell_t addr = forth_POP(ctx);

    ep = forth_SEARCH_DICTIONARY(ctx->dictionary, (const char *)addr, len);

    if (0 == ep)
    {
        for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
        {
            ep = forth_SEARCH_LIST(*wl, (char *)addr, (int)len);
            if (0 != ep)
            {
                break;
            }
        }
    }

    // Pushing the address like this overrides (discards) the const qualifier from the poiter.
    // This is the lesser of two evils.
    // The alternative would be to make the entire serach mechanism consume and return non const decorated pointers.
    // But then the entire array where the list of words is stored woudl have to be non const which may place it to a writable
    // memory section.
    // On systems where e.g. flash is cheap and RAM is expensive that would not be good.

    forth_PUSH(ctx, (forth_cell_t)ep);
}
