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
int forth_COMPARE_NAMES(const char *name, const char *input_word, int input_word_length)
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
        if (forth_COMPARE_NAMES((char *)(p->name), name, name_length))
        {
            return p;
        }
    }

    return 0;
}

// Search the contents of a word list.
forth_vocabulary_entry_t *forth_SEARCH_WORDLIST(forth_wordlist_t *wid, const char *name, int name_length)
{
    forth_vocabulary_entry_t *p = (forth_vocabulary_entry_t *)(wid->latest);

    while (0 != p)
	{
        if (0 != p->name)
        {
            if (forth_COMPARE_NAMES((char *)(p->name), name, name_length))
            {
                return p;
            }  
        }
        p = (forth_vocabulary_entry_t *)(p->link); 
    }
    return 0;
}

// Search all available lists for the name (passed as a c-addr, len pain or the data stack).
// If the name is found return the corresponding execution token.
// If the name is not found return 0.
// https://forth-standard.org/proposals/find-name
//
// FIND-NAME ( c-addr len -- nt|0 )
//
const forth_vocabulary_entry_t *forth_FIND_NAME(struct forth_runtime_context *ctx, const char *name, forth_cell_t len)
{
    const forth_vocabulary_entry_t **wl;
    const forth_vocabulary_entry_t *ep = 0;
    forth_wordlist_t *wid;
    forth_cell_t i;
    int root_searched = 0;

    if ((0 != ctx->dictionary) && (0 != ctx->wordlists) && (0 != ctx->wordlist_cnt) && (0 != ctx->wordlist_slots))
    {
        // forth_order(ctx);
        for (i = 1; i <= ctx->wordlist_cnt; i++)
        {
            wid = (forth_wordlist_t *)(ctx->wordlists[ctx->wordlist_slots - i]);
            ep = forth_SEARCH_WORDLIST(wid, name, len);

            if (0 != ep)
            {
                return ep;
            }

            if (wid == (forth_wordlist_t *)&(ctx->dictionary->forth_wl))
            {
                for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
                {
                    ep = forth_SEARCH_LIST(*wl, name, (int)len);

                    if (0 != ep)
                    {
                        return ep;
                    }
                }

                // Root is sort of part of Forth, so search it here.
                if (0 == root_searched)
                {
                    ep = forth_SEARCH_LIST(forth_wl_root, name, (int)len);
                    root_searched = 1;
                    if (0 != ep)
                    {
                        return ep;
                    }
                }

            }

            if ((wid == (forth_wordlist_t *)&forth_root_wordlist) && (0 == root_searched))
            {
                // Root was specifically in the search order.
                ep = forth_SEARCH_LIST(forth_wl_root, name, (int)len);
                root_searched = 1;

                if (0 != ep)
                {
                    return ep;
                }
            }
        }
    }
 
    if (0 == root_searched)
    {
        // As a last resort search Root.
        ep = forth_SEARCH_LIST(forth_wl_root, name, (int)len);
    }

    return ep;
}

// Search all available lists for the name (passed as a c-addr, len pain or the data stack).
// If the name is found return the corresponding execution token.
// If the name is not found return 0.
// https://forth-standard.org/proposals/find-name
//
// FIND-NAME ( c-addr len -- nt|0 )
//
void forth_find_name(struct forth_runtime_context *ctx)
{
    forth_cell_t len = forth_POP(ctx);
    const char *addr = (const char *)forth_POP(ctx);
    const forth_vocabulary_entry_t *ep = forth_FIND_NAME(ctx, addr, len);

    // Pushing the address like this overrides (discards) the const qualifier from the poiter.
    // This is the lesser of two evils.
    // The alternative would be to make the entire serach mechanism consume and return non const decorated pointers.
    // But then the entire array where the list of words is stored woudl have to be non const which may place it to a writable
    // memory section.
    // On systems where e.g. flash is cheap and RAM is expensive that would not be good.

    forth_PUSH(ctx, (forth_cell_t)ep);
}
// -----------------------------------------------------------------------------------------------
//                                    Vocabulary Listings
// -----------------------------------------------------------------------------------------------
void forth_PRINT_HELP_LIST(forth_runtime_context_t *ctx, const forth_vocabulary_entry_t *ep)
{
    static const char *action = "PCVDT..";
	size_t len;
	char a;

    for (; 0 != ep->name; ep++)
    {

		forth_EMIT(ctx, (FORTH_XT_FLAGS_IMMEDIATE & ep->flags) ? 'I' : FORTH_CHAR_SPACE);
		a = action[FORTH_XT_FLAGS_ACTION_MASK & ep->flags];
		forth_EMIT(ctx, a);
		forth_space(ctx);
        forth_TYPE0(ctx, (char *)(ep->name));
#if !defined(FORTH_EXCLUDE_DESCRIPTIONS)
        if (0 != ep->link)
        {
			len = strlen((char *)(ep->name));
			forth_PUSH(ctx, (len < 20) ? (20 - len) : 1);
			forth_spaces(ctx);
            forth_TYPE0(ctx, (char *)(ep->link));
        }
#endif
        forth_cr(ctx);
    }  
}
// HELP ( -- )
void forth_help(forth_runtime_context_t *ctx)
{
	const forth_vocabulary_entry_t **wl;// = forth_master_list_of_lists;
 
   	for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
    {
        forth_PRINT_HELP_LIST(ctx, *wl);
	}

    forth_PRINT_HELP_LIST(ctx, forth_wl_root);
}

void forth_PRINT_LIST(forth_runtime_context_t *ctx, const forth_vocabulary_entry_t *ep, int linked)
{
    size_t len;

    while((0 != ep) && (0 != ep->name))
    {
      	len = strlen((char *)(ep->name));

        if ((ctx->terminal_width - ctx->terminal_col) <= len)
		{
            forth_cr(ctx);
        }

        forth_TYPE0(ctx, (char *)(ep->name));
        forth_space(ctx);

        if (linked)
        {
            ep = (const forth_vocabulary_entry_t *)(ep->link);
        }
        else
        {
            ep++;
        }
    }
}

// A factor of WORDS
void forth_words_master(forth_runtime_context_t *ctx)
{
    const forth_vocabulary_entry_t **wl = forth_master_list_of_lists;

    for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
    {
       forth_PRINT_LIST(ctx, *wl, 0);
	}
}

// WORDS ( -- )
void forth_words(forth_runtime_context_t *ctx)
{

    forth_wordlist_t *wid;
    forth_cell_t i;
    int root_listed = 0;

	if (0 != ctx->dictionary)
	{
        if ((0 != ctx->wordlists) && (0 != ctx->wordlist_slots))
        {
            for (i = ctx->wordlist_cnt; i > 0; i--)
            {
                wid = (forth_wordlist_t *)(ctx->wordlists[ctx->wordlist_slots - i]);
                forth_PRINT_LIST(ctx, (forth_vocabulary_entry_t *)(wid->latest), 1);

                if (wid == (forth_wordlist_t *)&(ctx->dictionary->forth_wl))
                {
                    forth_words_master(ctx);
                    if (0 == root_listed)
                    {
                        // List root as part of Forth.
                        forth_PRINT_LIST(ctx, forth_wl_root, 0);
                        root_listed = 1;
                    }
                }

                if (wid == (forth_wordlist_t *)&forth_root_wordlist)
                {
                    if (0 == root_listed)
                    {
                        forth_PRINT_LIST(ctx, forth_wl_root, 0);
                        root_listed = 1;
                    }
                }
            }
        }
	}

    forth_cr(ctx);
}
