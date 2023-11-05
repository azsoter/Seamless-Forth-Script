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

// -----------------------------------------------------------------------------------------------------
//                                              Search order stuff
// -----------------------------------------------------------------------------------------------------
// FORTH ( -- )
void forth_forth(forth_runtime_context_t *ctx)
{
	if ((0 == ctx->dictionary) || (0 == ctx->wordlists) || (0 == ctx->wordlist_slots) || (0 == ctx->wordlist_cnt))
	{
		return;	// Silently ignore problems, in case FORTH is executed in an interpret-only script.
	}

	ctx->wordlists[ctx->wordlist_slots - ctx->wordlist_cnt] = (forth_cell_t)&(ctx->dictionary->forth_wl);
}

#if !defined(FORTH_WITHOUT_COMPILATION)
// Initialize search order to something sane.
//
int forth_InitSearchOrder(forth_runtime_context_t *ctx, forth_cell_t *wordlists, forth_cell_t slots)
{
	if ((0 == wordlists) || (slots < 8))
	{
		return -1;
	}

	if (0 == ctx->dictionary)
	{
		return -1;
	}

	ctx->wordlists = wordlists;
	ctx->wordlist_slots = slots;
	ctx->wordlist_cnt = 2;
	ctx->wordlists[slots - 1] = (forth_cell_t)&forth_root_wordlist;
	ctx->wordlists[slots - 2] = (forth_cell_t)&(ctx->dictionary->forth_wl);
	ctx->current =	(forth_cell_t)&(ctx->dictionary->forth_wl);

	return 0;
}

// FORTH-WORDLIST ( -- wid )
void forth_forth_wordlist(forth_runtime_context_t *ctx)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // Unsupported operation.
	}
	forth_PUSH(ctx, (forth_cell_t)&(ctx->dictionary->forth_wl));
}

// GET-CURRENT ( -- wid )
void forth_get_current(forth_runtime_context_t *ctx)
{
	forth_PUSH(ctx, ctx->current);
}

// SET-CURRENT ( wid -- )
void forth_set_current(forth_runtime_context_t *ctx)
{
	forth_cell_t wid = forth_POP(ctx);

	// We do NOT allow adding to the 'Root' worlist, it is only a place holder.
	if (((forth_cell_t)&forth_root_wordlist) == wid)
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	ctx->current = wid;
}

// DEFINITIONS ( -- )
void forth_definitions(forth_runtime_context_t *ctx)
{
	if ((0 == ctx->wordlists) || (0 == ctx->wordlist_slots) || (0 == ctx->wordlist_cnt))
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	forth_PUSH(ctx, ctx->wordlists[ctx->wordlist_slots - ctx->wordlist_cnt]);
	forth_set_current(ctx);
}

// ORDER ( -- )
void forth_order(forth_runtime_context_t *ctx)
{
	forth_cell_t cnt = ctx->wordlist_cnt;
	forth_cell_t i;
	const char *name;
	forth_wordlist_t *wid;

	if ((0 == ctx->dictionary) || (0 == cnt))
	{
		return;
	}

	for (i = 1; i <= cnt; i++)
	{
		wid = (forth_wordlist_t *)(ctx->wordlists[ctx->wordlist_slots - i]);
		name = (const char *)(wid->name);

		if ((0 == name) || (0 == *name)) // Noname wordlist
		{
			forth_TYPE0(ctx, "WID:0X");
			forth_HDOT(ctx, (forth_cell_t)wid);
		}
		else
		{
			forth_TYPE0(ctx, name);
			forth_space(ctx);
		}
	}
	forth_cr(ctx);
}

// GET-ORDER ( -- WIDn ... WID2 WID1 n)
void forth_get_order(forth_runtime_context_t *ctx)
{
	forth_cell_t cnt = ctx->wordlist_cnt;
	forth_cell_t i;

	if ((0 == ctx->dictionary) || (0 == cnt) || (0 == ctx->wordlists))
	{
		forth_PUSH(ctx, 0);
		return;
	}

	for (i = 1; i <= cnt; i++)
	{
		forth_PUSH(ctx, ctx->wordlists[ctx->wordlist_slots - i]);
	}

	forth_PUSH(ctx, cnt);
}

// ALSO ( -- )
void forth_also(forth_runtime_context_t *ctx)
{
	forth_cell_t cnt = ctx->wordlist_cnt;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	if (((cnt + 1) > ctx->wordlist_slots)  || (0 == ctx->wordlists))
	{
		forth_THROW(ctx, -49); // search-order overflow
	}

	cnt += 1;
	ctx->wordlists[ctx->wordlist_slots - cnt] = ctx->wordlists[ctx->wordlist_slots - ctx->wordlist_cnt ];
	ctx->wordlist_cnt = cnt;
}

// PREVIOUS ( -- )
void forth_previous(forth_runtime_context_t *ctx)
{
	forth_cell_t cnt = ctx->wordlist_cnt;

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	if (1 > cnt)
	{
		forth_THROW(ctx, -50); // search-order underflow
	}

	ctx->wordlist_cnt = cnt - 1;
}

// ONLY ( -- )
void forth_only(forth_runtime_context_t *ctx)
{
	if ((1 > ctx->wordlist_slots)  || (0 == ctx->wordlists))
	{
		forth_THROW(ctx, -49); // search-order overflow
	}

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	ctx->wordlist_cnt = 1;
	ctx->wordlists[ctx->wordlist_slots - 1] = (forth_cell_t)&forth_root_wordlist;
}

// SET-ORDER ( WIDn ... WID2 WID1 n -- )
void forth_set_order(forth_runtime_context_t *ctx)
{
	forth_cell_t cnt = forth_POP(ctx);
	forth_cell_t i;

	if (-1 == (forth_scell_t)cnt)
	{
		forth_only(ctx);
		return;
	}

	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	if ((cnt > ctx->wordlist_slots)  || (0 == ctx->wordlists))
	{
		forth_THROW(ctx, -49); // search-order overflow
	}

	ctx->wordlist_cnt = cnt;

	for (i = cnt; i >= 1; i--)
	{
		ctx->wordlists[ctx->wordlist_slots - i] = forth_POP(ctx);
	}
}

// WORDLIST ( -- wid )
void forth_wordlist(forth_runtime_context_t *ctx)
{
	if (0 == ctx->dictionary)
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

	forth_align(ctx);
	forth_here(ctx);
	forth_COMMA(ctx, 0);									// latest
    forth_COMMA(ctx, ctx->dictionary->last_wordlist);		// link
	forth_COMMA(ctx, ctx->current);                         // parent
	forth_COMMA(ctx, 0);									// name
    ctx->dictionary->last_wordlist = ctx->sp[0];
}

// Compiled by VOCABULARY
// (do-voc) ( addr -- )
void forth_do_voc(forth_runtime_context_t *ctx)
{
    if ((0 == ctx->dictionary) || (0 == ctx->wordlists) || (0 == ctx->wordlist_slots) || (0 == ctx->wordlist_cnt))
	{
		forth_THROW(ctx, -21); // unsupported operation
	}

    // We don't need the fetch.
    // The original idea was to store WID at the body of create, but the wordlist is created so that
    // the WID is the same as the addres of the CREATEd word.
    // forth_fetch(ctx);
    ctx->wordlists[ctx->wordlist_slots - ctx->wordlist_cnt] = forth_POP(ctx);
}

// Define a vocabulary.
// A vocabulary includes a wordlist and when executed it behaves like the word "FORTH", i.e.
// replaces the top of the search order with this wordlist.
// VOCABULARY ( "name" -- )
void forth_vocabulary(forth_runtime_context_t *ctx)
{
    forth_vocabulary_entry_t *entry;
    forth_wordlist_t *wid;

    forth_create(ctx);
    entry = forth_GET_LATEST(ctx);
    forth_wordlist(ctx);
    wid = (forth_wordlist_t *)forth_POP(ctx);
    wid->name = entry->name;
    entry->meaning = (forth_cell_t)forth_DO_VOC_xt;
}

// .WORDLISTS ( -- ) List all wordlists
void forth_dot_wordlists(forth_runtime_context_t *ctx)
{
    forth_wordlist_t *wid;

    if (0 == ctx->dictionary)
    {
        return;
    }

    for (wid = (forth_wordlist_t *)(ctx->dictionary->last_wordlist); 0 != wid; wid = (forth_wordlist_t *)wid->link)
    {
        if ((0 == wid->name) || (0 == *(const char *)(wid->name)))
        {
            forth_TYPE0(ctx,"WID:0X");
            forth_PUSH(ctx, (forth_cell_t)wid);
            forth_hdot(ctx);
        }
        else
        {
            forth_TYPE0(ctx, (const char *)(wid->name));
            forth_space(ctx);
        }
    }
}
#endif
// ----------------------------------------------------------------------------------------------------
//                                      Searching definition names
// ----------------------------------------------------------------------------------------------------

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
const forth_vocabulary_entry_t *forth_SEARCH_COMPILED_IN_LIST(const forth_vocabulary_entry_t *list, const char *name, int name_length)
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

// Search the compiled in master table.
const forth_vocabulary_entry_t *forth_SEARCH_MASTER_TABLE(const char *name, int name_length)
{
    const forth_vocabulary_entry_t **wl;
    const forth_vocabulary_entry_t *ep = 0;

    for (wl = forth_master_list_of_lists; 0 != *wl; wl++)
    {
        ep = forth_SEARCH_COMPILED_IN_LIST(*wl, name, (int)name_length);

        if (0 != ep)
        {
            return ep;
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



// SEARCH-WORDLIST ( c-addr u wid -- 0 | xt 1 | xt -1 )
void forth_search_wordlist(struct forth_runtime_context *ctx)
{
    forth_wordlist_t *wid = (forth_wordlist_t *)forth_POP(ctx);
    forth_cell_t len = forth_POP(ctx);
    const char *name = (const char *)forth_POP(ctx);
    const forth_vocabulary_entry_t *ep;

    if (0 == wid)
    {
        forth_THROW(ctx, -9); // invalid memory address
    }

    ep = forth_SEARCH_WORDLIST(wid, name, len);

    if ((0 == ep) && (0 != ctx->dictionary) && ((forth_wordlist_t *)&(ctx->dictionary->forth_wl) == wid))
    {
        ep = forth_SEARCH_MASTER_TABLE(name, len);

        if (0 == ep)
        {
            ep = forth_SEARCH_COMPILED_IN_LIST(forth_wl_root, name, (int)len);
        }
    }

    if ((&forth_root_wordlist == wid) && (0 == ep))
    {
       ep = forth_SEARCH_COMPILED_IN_LIST(forth_wl_root, name, (int)len); 
    }

    if (0 == ep)
    {
        forth_PUSH(ctx, 0);
    }
    else
    {
        forth_PUSH(ctx, (forth_cell_t)ep);

        if ( 0 != (FORTH_XT_FLAGS_IMMEDIATE & ep->flags))
        {
            // IMMEDIATE word.
            forth_PUSH(ctx, 1);
        }
        else
        {
            // Non-IMMEDIATE word.
            forth_PUSH(ctx, -1);
        }
    }
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
    // const forth_vocabulary_entry_t **wl;
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
                ep = forth_SEARCH_MASTER_TABLE(name, len);

                if (0 != ep)
                {
                    return ep;
                }

                // Root is sort of part of Forth, so search it here.
                if (0 == root_searched)
                {
                    ep = forth_SEARCH_COMPILED_IN_LIST(forth_wl_root, name, (int)len);
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
                ep = forth_SEARCH_COMPILED_IN_LIST(forth_wl_root, name, (int)len);
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
        ep = forth_SEARCH_COMPILED_IN_LIST(forth_wl_root, name, (int)len);
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

// -----------------------------------------------------------------------------------------------
//						            Minimal (ROOT) List
// -----------------------------------------------------------------------------------------------
const forth_vocabulary_entry_t forth_wl_root[] =
{
DEF_FORTH_WORD("words",      		0, forth_words,         				"( -- )"),
#if !defined(FORTH_WITHOUT_COMPILATION)
DEF_FORTH_WORD( "definitions",		0, forth_definitions,      	 			"( -- )"),
DEF_FORTH_WORD("forth-wordlist",	0, forth_forth_wordlist,     			"( -- wid )" ),
DEF_FORTH_WORD("wordlist",			0, forth_wordlist,     					"( -- wid )" ),
DEF_FORTH_WORD("order",  			0, forth_order,                       	"( -- )" ),
DEF_FORTH_WORD("only",   			0, forth_only,                        	"( -- )" ),
DEF_FORTH_WORD("also",   			0, forth_also,                        	"( -- )" ),
DEF_FORTH_WORD("previous",   		0, forth_previous,                		"( -- )" ),
DEF_FORTH_WORD("get-current",		0, forth_get_current,           		"( -- wid )" ),
DEF_FORTH_WORD("set-current",		0, forth_set_current,           		"( wid -- )" ),
DEF_FORTH_WORD("set-order",			0, forth_set_order,               		"( WIDn ... WID2 WID1 n -- )" ),
DEF_FORTH_WORD("get-order",			0, forth_get_order,               		"( -- WIDn ... WID2 WID1 n )" ),
DEF_FORTH_WORD(".wordlists",  		0, forth_dot_wordlists,                 "( -- )" ),
DEF_FORTH_WORD("vocabulary",  		0, forth_vocabulary,                    "( \"name\" -- )" ),
#else
DEF_FORTH_WORD("only",   			0, forth_noop,                        	"( -- )" ),
#endif
DEF_FORTH_WORD( "bye",				0, forth_bye,           				"( -- )"),
DEF_FORTH_WORD("search-wordlist",   0, forth_search_wordlist,               "( c-addr u wid -- 0 | xt 1 | xt -1 )"),
DEF_FORTH_WORD( "forth",  	 		0, forth_forth,      	 				"( -- )"),
DEF_FORTH_WORD(0, 0, 0, 0)
};

forth_wordlist_t forth_root_wordlist =
{
	0, 0, 0 , (forth_cell_t) "Root"
};
