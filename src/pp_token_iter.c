#include "pp_token_iter.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "allocator.h"
#include "file_storage.h"
#include "llist.h"
#include "pp_lexer.h"
#include "str.h"

void
ppti_include_file(pp_token_iter *it, string filename) {
    file *current_file = NULL;
    for (ppti_entry *e = it->it; e; e = e->next) {
        if (e->f) {
            current_file = e->f;
            break;
        }
    }

    file *f = fs_get_file(filename, current_file);
    if (!f) {
        NOT_IMPL;
    }

    ppti_entry *entry = aalloc(it->a, sizeof(ppti_entry));
    entry->f          = f;
    entry->lexer      = aalloc(it->a, sizeof(pp_lexer));
    pp_lexer_init(entry->lexer, f->contents.data, STRING_END(f->contents), it->tok_buf,
                  it->tok_buf_capacity);
    LLIST_ADD(it->it, entry);
}

void
ppti_insert_tok_list(pp_token_iter *it, pp_token *first, pp_token *last) {
    assert(first && last);

    ppti_entry *e = it->it;
    if (!e) {
        e = aalloc(it->a, sizeof(ppti_entry));
        LLIST_ADD(it->it, e);
    }

    last->next    = e->token_list;
    e->token_list = first;
}

pp_token *
ppti_peek_forward(pp_token_iter *it, uint32_t count) {
    pp_token *tok = NULL;
    ppti_entry *e = it->it;
    if (e) {
        uint32_t idx    = 0;
        pp_token **tokp = &e->token_list;
        for (;;) {
            // First, skip tokens that are already in tokens list
            while (*tokp && (*tokp)->next && idx != count) {
                tokp = &(*tokp)->next;
                ++idx;
            }

            if (*tokp && count == idx) {
                break;
            }

            if (!e || !e->lexer) {
                break;
            }

            // Try to use lexer.
            pp_token local_tok = {0};
            bool not_eof       = pp_lexer_parse(e->lexer, &local_tok);
            // If lexer produces eof, meaning it has reached its end, we must
            // skip to the next stack entry.
            if (!not_eof) {
                e = e->next;
                if (e) {
                    tokp = &e->token_list;
                }
                continue;
            }

            pp_token *new_tok = aalloc(it->a, sizeof(pp_token));
            memcpy(new_tok, &local_tok, sizeof(pp_token));
            new_tok->loc.filename = e->f->name;
            if (new_tok->str.data) {
                new_tok->str = string_dup(it->a, new_tok->str);
            }
#if HOLOC_DEBUG
            {
                char buffer[4096];
                uint32_t len     = fmt_pp_tok_verbose(buffer, sizeof(buffer), new_tok);
                char *debug_info = aalloc(get_debug_allocator(), len + 1);
                memcpy(debug_info, buffer, len + 1);
                new_tok->_debug_info = debug_info;
            }
#endif

            if (*tokp) {
                assert(!(*tokp)->next);
                (*tokp)->next = new_tok;
                tokp          = &(*tokp)->next;
                ++idx;
            } else {
                *tokp = new_tok;
            }
        }

        if (idx == count) {
            tok = *tokp;
        }
    }

    if (!tok) {
        tok = it->eof_token;
    }
    return tok;
}

pp_token *
ppti_peek(pp_token_iter *it) {
    return ppti_peek_forward(it, 0);
}

void
ppti_eat_multiple(pp_token_iter *it, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        ppti_eat(it);
    }
}

void
ppti_eat(pp_token_iter *it) {
    // TODO: Do we want to return success status here?
    ppti_entry *e = it->it;
    if (e) {
        pp_token *tok = e->token_list;
        if (tok) {
            LLIST_POP(e->token_list);
            afree(it->a, tok, sizeof(pp_token));
        } else {
            if (e->lexer) {
                afree(it->a, e->lexer, sizeof(pp_token));
            }
            it->it = e->next;
            afree(it->a, e, sizeof(ppti_entry));
            ppti_eat(it);
        }
    }
}

pp_token *
ppti_eat_peek(pp_token_iter *it) {
    ppti_eat(it);
    return ppti_peek(it);
}
