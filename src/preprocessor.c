#include "preprocessor.h"

#include "str.h"
#include "pp_lexer.h"
#include "hashing.h"
#include "allocator.h"
#include "bump_allocator.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

// FIXME: All copies from lexer->string_buf act as if it always was an 1-byte aligned array, which in reality it may be not

static preprocessor_macro **
get_macro(preprocessor *pp, uint32_t hash) {
    preprocessor_macro **macrop = hash_table_sc_get_u32(
        pp->macro_hash, sizeof(pp->macro_hash) / sizeof(*pp->macro_hash),
        preprocessor_macro, next, name_hash, 
        hash);
    return macrop;
}

static preprocessor_macro *
get_new_macro(preprocessor *pp) {
    preprocessor_macro *macro = pp->macro_freelist;
    if (macro) {
        pp->macro_freelist = macro->next;
        memset(macro, 0, sizeof(*macro));
    } else {
        macro = bump_alloc(pp->a, sizeof(*macro));
    }
    return macro;
}

static void 
free_macro_data(preprocessor *pp, preprocessor_macro *macro) {
    while (macro->args) {
        preprocessor_macro_arg *arg = macro->args;
        macro->args = arg->next;
        arg->next = pp->macro_arg_freelist;
        pp->macro_arg_freelist = arg;
    }

    while (macro->definition) {
        preprocessor_token *tok = macro->definition;
        macro->definition = tok;
        tok->next = pp->tok_freelist;
        pp->tok_freelist = tok;
    }
}

static preprocessor_macro_arg *
get_new_macro_arg(preprocessor *pp) {
    preprocessor_macro_arg *arg = pp->macro_arg_freelist;
    if (arg) {
        pp->macro_arg_freelist = arg->next;
        memset(arg, 0, sizeof(*arg));
    } else {
        arg = bump_alloc(pp->a, sizeof(*arg));
    }
    return arg;
}

static preprocessor_token *
get_new_token(preprocessor *pp) {
    preprocessor_token *tok = pp->tok_freelist;
    if (tok) {
        pp->tok_freelist = tok->next;
        memset(tok, 0, sizeof(*tok));
    } else {
        tok = bump_alloc(pp->a, sizeof(*tok));
    }
    return tok;
}

static preprocessor_conditional_include *
get_new_cond_incl(preprocessor *pp) {
    preprocessor_conditional_include *incl = pp->incl_freelist;
    if (incl) {
        pp->incl_freelist = incl->next;
        memset(incl, 0, sizeof(*incl));
    } else {
        incl = bump_alloc(pp->a, sizeof(*incl));
    }
    return incl;
}

static preprocessor_macro_expansion_arg *
get_new_expansion_arg(preprocessor *pp) {
    preprocessor_macro_expansion_arg *arg = pp->macro_expansion_arg_freelist;
    if (arg) {
        pp->macro_expansion_arg_freelist = arg->next;
        memset(arg, 0, sizeof(*arg));
    } else {
        arg = bump_alloc(pp->a, sizeof(*arg));
    }
    return arg;
}

static void
init_pp_token_from_lexer(preprocessor *pp, preprocessor_token *tok) {
    tok->kind = pp->lexer->tok_kind;
    tok->str_kind = pp->lexer->tok_str_kind;
    tok->punct_kind = pp->lexer->tok_punct_kind;
    string data = string(pp->lexer->tok_buf, pp->lexer->tok_buf_len);
    allocator a = bump_get_allocator(pp->a);
    tok->str = string_dup(&a, data);
}

static void 
define_macro(preprocessor *pp) {
    pp_lexer *lexer = pp->lexer;
    if (lexer->tok_kind != PP_TOK_ID) {
        // TODO: Diagnostic
        assert(false);
    }

    string ident_name = string(lexer->tok_buf, lexer->tok_buf_len);
    uint32_t hash = hash_string(ident_name);
    preprocessor_macro **macrop = get_macro(pp, hash);
    if (*macrop) {
        ident_name = (*macrop)->name;
        // Macro is already present
        // TODO: Diagnostic
        assert(false);
        // Free all args and tokens macro is holding
        free_macro_data(pp, *macrop);
        // Delete macro data but keep its next pointer for hash table
        preprocessor_macro *next = (*macrop)->next;
        memset(*macrop, 0, sizeof(**macrop));
        (*macrop)->next = next;
    } else {
        allocator a = bump_get_allocator(pp->a);
        ident_name = string_dup(&a, ident_name);

        preprocessor_macro *new_macro = get_new_macro(pp);
        new_macro->next = *macrop;
        *macrop = new_macro;
    }

    preprocessor_macro *macro = *macrop;
    assert(macro);
    
    macro->name = ident_name;
    macro->name_hash = hash;

    pp_lexer_parse(lexer);
    if (lexer->tok_kind == PP_TOK_PUNCT && 
        lexer->tok_punct_kind == '(' &&
        !lexer->tok_has_whitespace) {
        preprocessor_macro_arg *args = 0;

        pp_lexer_parse(lexer);
        // TODO: This condition can be checked inside the loop body
        while (lexer->tok_kind != PP_TOK_PUNCT || lexer->tok_punct_kind != ')') {
            if (lexer->tok_kind == PP_TOK_PUNCT && 
                lexer->tok_punct_kind == PP_TOK_PUNCT_VARARGS) {
                string arg_name = WRAP_Z("__VA_ARGS__");
                preprocessor_macro_arg *arg = get_new_macro_arg(pp);
                arg->name = arg_name;
                arg->next = args;
                args = arg;

                pp_lexer_parse(lexer);
                if (lexer->tok_kind != ')') {
                    // TODO: Diagnostic
                    assert(false);
                    break;
                }
            } else if (lexer->tok_kind == PP_TOK_ID) {
                allocator a = bump_get_allocator(pp->a);
                string arg_name = string_memdup(&a, lexer->tok_buf);
                preprocessor_macro_arg *arg = get_new_macro_arg(pp);
                arg->name = arg_name;
                arg->next = args;
                args = arg;
                
                pp_lexer_parse(lexer);
                if (lexer->tok_kind == PP_TOK_PUNCT) {
                    if (lexer->tok_punct_kind == ',') {
                        pp_lexer_parse(lexer);
                    } else if (lexer->tok_punct_kind != ')') {
                        // TODO: Diagnostic
                        assert(false);
                        break;
                    }
                } 
            }
        }

        if (lexer->tok_kind == PP_TOK_PUNCT && lexer->tok_punct_kind == ')') {
            pp_lexer_parse(lexer);
        }

        macro->args = args;
        macro->is_function_like = true;
    }

    while (!lexer->tok_at_line_start && lexer->tok_kind != PP_TOK_EOF) {
        preprocessor_token *tok = get_new_token(pp);
        init_pp_token_from_lexer(pp, tok);
        tok->next = macro->definition;
        macro->definition = tok;
    }
}

static void 
undef_macro(preprocessor *pp) {
    pp_lexer *lexer = pp->lexer;
    if (lexer->tok_kind != PP_TOK_ID) {
        // TODO: Diagnostic
        assert(false);
    }

    string ident_name = string(lexer->tok_buf, lexer->tok_buf_len);
    uint32_t hash = hash_string(ident_name);
    preprocessor_macro **macrop = get_macro(pp, hash);
    if (*macrop) {
        preprocessor_macro *macro = *macrop;
        free_macro_data(pp, macro);
        *macrop = macro->next;
        macro->next = pp->macro_freelist;
        pp->macro_freelist = macro;
    } else {
        // TODO: Diagnostic
        assert(false);
    }
}

static void 
push_cond_incl(preprocessor *pp, bool is_included) {
    preprocessor_conditional_include *incl = get_new_cond_incl(pp);
    incl->is_included = is_included;

    incl->next = pp->cond_incl_stack;
    pp->cond_incl_stack = incl;
}

static void 
skip_cond_incl(preprocessor *pp) {
    uint32_t depth = 0;
    pp_lexer *lexer = pp->lexer;

    while (lexer->tok_kind != PP_TOK_EOF) {
        if (lexer->tok_kind == PP_TOK_PUNCT && 
            lexer->tok_punct_kind == '#' &&
            lexer->tok_at_line_start) {
            pp_lexer_parse(lexer);
            if (lexer->tok_kind == PP_TOK_ID) {
                string directive = string(lexer->tok_buf, lexer->tok_buf_len);
                if (string_eq(directive, WRAP_Z("if")) ||
                    string_eq(directive, WRAP_Z("ifdef")) ||
                    string_eq(directive, WRAP_Z("ifndef"))) {
                    ++depth;
                } else if (string_eq(directive, WRAP_Z("elif")) ||
                           string_eq(directive, WRAP_Z("else")) ||
                           string_eq(directive, WRAP_Z("endif"))) {
                    if (!depth) {
                        break;
                    }
                    if (string_eq(directive, WRAP_Z("endif"))) {
                        --depth;
                    }
                }
            }
        } else {
            pp_lexer_parse(lexer);
        }
    }

    if (depth) {
        // TODO: Diagnostic
    }
}

static int64_t
eval_if_expr(preprocessor *pp) {
    int64_t result = 0;

    return result;
}

static void
process_pragma(preprocessor *pp) {
    pp_lexer *lexer = pp->lexer;
    while (!lexer->tok_at_line_start) {
        if (lexer->tok_kind == TOK_ID &&
            strcmp(lexer->tok_buf, "once")) {
            preprocessor_guarded_file *guarded = bump_alloc(pp->a, sizeof(preprocessor_guarded_file));
            // TODO:
            (void)guarded;
        }
    }
}

static void 
process_pp_directive(preprocessor *pp) {
    pp_lexer *lexer = pp->lexer;
    // Looping is for conditional includes
    for (;;) {
        // Null directive is a single hash symbol followed by line break
        if (lexer->tok_at_line_start) {
            break;
        }

        if (lexer->tok_kind != PP_TOK_ID) {
            // TODO: Diagnostic
            assert(false);
        }

        string directive = string(lexer->tok_buf, lexer->tok_buf_len);
        if (string_eq(directive, WRAP_Z("include"))) {
        } else if (string_eq(directive, WRAP_Z("define"))) {
            pp_lexer_parse(lexer);
            define_macro(pp);
            break;
        } else if (string_eq(directive, WRAP_Z("undef"))) {
            pp_lexer_parse(lexer);
            undef_macro(pp);
            break;
        } else if (string_eq(directive, WRAP_Z("if"))) {
            pp_lexer_parse(lexer);
            int64_t expr = eval_if_expr(pp);
            push_cond_incl(pp, expr != 0);
            if (expr) {
                break;
            } else {
                skip_cond_incl(pp);
            }
        } else if (string_eq(directive, WRAP_Z("elif"))) {
            pp_lexer_parse(lexer);
            int64_t expr = eval_if_expr(pp);
            preprocessor_conditional_include *incl = pp->cond_incl_stack;
            if (!incl) {
                // TODO: Diagnostic
            } else {
                if (!incl->is_included && expr) {
                    incl->is_included = true;
                } else {
                    skip_cond_incl(pp);
                }
            }
        } else if (string_eq(directive, WRAP_Z("else"))) {
            preprocessor_conditional_include *incl = pp->cond_incl_stack;
            if (!incl) {
                // TODO: Diagnostic
            } else {
                if (incl->is_after_else) {
                    // TODO: Diagnostic
                }
                incl->is_after_else = true;
                if (incl->is_included) {
                    skip_cond_incl(pp);
                    continue;
                } 
                break;
            }
        } else if (string_eq(directive, WRAP_Z("endif"))) {
            pp_lexer_parse(lexer);
            preprocessor_conditional_include *incl = pp->cond_incl_stack;
            if (!incl) {
                // TODO: Diagnostic
            } else {
                pp->cond_incl_stack = incl->next;
                incl->next = pp->incl_freelist;
                pp->incl_freelist = incl;
            }
            break;
        } else if (string_eq(directive, WRAP_Z("ifdef"))) {
            pp_lexer_parse(lexer);
            if (lexer->tok_kind != PP_TOK_ID) {
                // TODO: Diagnostic
                assert(false);
            }
            uint32_t macro_name_hash = hash_string(string(lexer->tok_buf,
                                                          lexer->tok_buf_len));
            bool is_defined = (*get_macro(pp, macro_name_hash) != 0);
            push_cond_incl(pp, is_defined);
            if (!is_defined) {
                skip_cond_incl(pp);
                continue;
            }
        } else if (string_eq(directive, WRAP_Z("ifndef"))) {
            pp_lexer_parse(lexer);
            if (lexer->tok_kind != PP_TOK_ID) {
                // TODO: Diagnostic
                assert(false);
            }
            uint32_t macro_name_hash = hash_string(string(lexer->tok_buf,
                                                          lexer->tok_buf_len));
            bool is_defined = (*get_macro(pp, macro_name_hash) != 0);
            push_cond_incl(pp, !is_defined);
            if (is_defined) {
                skip_cond_incl(pp);
                continue;
            }
        } else if (string_eq(directive, WRAP_Z("line"))) {
            // Ignored
        } else if (string_eq(directive, WRAP_Z("pragma"))) {
            pp_lexer_parse(lexer);
            process_pragma(pp);
        } else if (string_eq(directive, WRAP_Z("error"))) {
        } else {
            // TODO: Diagnostic
        }
    }
}

static token
convert_pp_token(preprocessor *pp, preprocessor_token *pp_tok) {
    return (token) { 0 };
}

static void 
push_token_to_stack(preprocessor *pp, token tok) {
}

static void 
free_expansion_args(preprocessor *pp, preprocessor_macro_expansion_arg *args) {
    for (preprocessor_macro_expansion_arg *arg = args;
         arg;
         arg = arg->next) {
        for (preprocessor_token *token = arg->tokens;
             token;
             token = token->next) {
            if (!token->next) {
                token->next = pp->tok_freelist;
                pp->tok_freelist = arg->tokens;
            }
        }

        if (!arg->next) {
            arg->next = pp->macro_expansion_arg_freelist;
            pp->macro_expansion_arg_freelist = args;
        }
    }
}

static preprocessor_macro_expansion_arg *
collect_macro_call_arguments(preprocessor *pp, preprocessor_token **tokp) {
    preprocessor_macro_expansion_arg *new_args = 0;
    preprocessor_token *tok = *tokp;

    uint32_t parens_depth = 0;
    preprocessor_macro_expansion_arg *current_arg = 0;
    for (;;) {
        if (tok->kind == PP_TOK_PUNCT &&
            tok->punct_kind == ')' &&
            parens_depth == 0) {
            current_arg->next = new_args;
            new_args = current_arg;
            break;
        }

        if (!current_arg) {
            current_arg = get_new_expansion_arg(pp);
        }

        if (tok->kind == PP_TOK_PUNCT &&
            tok->punct_kind == ',') {
            current_arg->next = new_args;
            new_args = current_arg;
            parens_depth = 0;
            tok = tok->next;
            continue;
        }

        if (tok->kind == PP_TOK_PUNCT) {
            if (tok->punct_kind == '(') {
                ++parens_depth;
            } else if (tok->punct_kind == ')') {
                if (parens_depth) {
                    --parens_depth;
                } else {
                    // TODO: Diagnostic
                }
            }
        }

        preprocessor_token *token = get_new_token(pp);
        init_pp_token_from_lexer(pp, token);
        token->next = current_arg->tokens;
        current_arg->tokens = token;

        tok = tok->next;
    }

    *tokp = tok;
    return new_args;
}

static void 
expand_macro_internal(preprocessor *pp, preprocessor_macro *macro,
                      preprocessor_macro_expansion_arg *arg_values) {
    for (preprocessor_token *tok = macro->definition;
         tok;
         tok = tok->next) {
        // If token is not potential macro, treat it as usual
        if (tok->kind != PP_TOK_ID) {
            push_token_to_stack(pp, convert_pp_token(pp, tok));
        }
        
        string name = tok->str;
        // First, check if identifier corresponds to some argument
        bool is_arg = false;
        uint32_t arg_idx = 0;
        for (preprocessor_macro_arg *arg = macro->args;
             arg;
             arg = arg->next, ++arg_idx) {
            if (string_eq(arg->name, name)) {
                is_arg = true;
                break;
            }
        }

        if (is_arg) {
            uint32_t idx = 0;
            for (preprocessor_macro_expansion_arg *arg = arg_values;
                 arg;
                 arg = arg->next, ++idx) {
               if (idx == arg_idx) {
                   // Join the tokens expanded from argument into list of currently 
                   // parsed tokens without modifying the initial token list
                   for (preprocessor_token *token = arg->tokens;
                        token;
                        token = token->next) {
                       if (!token->next) {
                           token->next = tok;
                           tok = token;
                       }
                   }
               } 
            }
            continue;
        }

        uint32_t name_hash = hash_string(name);
        preprocessor_macro **new_macrop = get_macro(pp, name_hash);
        if (*new_macrop) {
            preprocessor_macro *new_macro = *new_macrop;
            if (new_macro->is_function_like &&
                tok->next &&
                tok->next->kind == TOK_PUNCT &&
                tok->next->punct_kind == '(' &&
                !tok->next->has_spaces) {

                preprocessor_macro_expansion_arg *new_args = 
                    collect_macro_call_arguments(pp, &tok);
                expand_macro_internal(pp, new_macro, new_args);
                free_expansion_args(pp, new_args);

                continue;
            } else if (!new_macro->is_function_like) {
                expand_macro_internal(pp, new_macro, 0);
                continue;
            }
        }

        push_token_to_stack(pp, convert_pp_token(pp, tok));
    }
}

static bool 
expand_macro(preprocessor *pp) {
    bool result = false;

    pp_lexer *lexer = pp->lexer;
    if (lexer->tok_kind == PP_TOK_ID) {
        string name = string(lexer->tok_buf, lexer->tok_buf_len);
        uint32_t name_hash = hash_string(name);
        preprocessor_macro **macrop = get_macro(pp, name_hash);
        if (*macrop) {
            preprocessor_macro *macro = *macrop;
            if (macro->is_function_like) {
                pp_lexer_parse(lexer);
                if (lexer->tok_kind == PP_TOK_PUNCT &&
                    lexer->tok_punct_kind == '(' &&
                    !lexer->tok_has_whitespace) {
                    pp_lexer_parse(lexer);
                    preprocessor_macro_expansion_arg *args = 0;

                    uint32_t parens_depth = 0;
                    preprocessor_macro_expansion_arg *current_arg = 0;
                    for (;;) {
                        if (lexer->tok_kind == PP_TOK_PUNCT &&
                            lexer->tok_punct_kind == ')' &&
                            parens_depth == 0) {
                            current_arg->next = args;
                            args = current_arg;
                            break;
                        }

                        if (!current_arg) {
                            current_arg = get_new_expansion_arg(pp);
                        }

                        if (lexer->tok_kind == PP_TOK_PUNCT &&
                            lexer->tok_punct_kind == ',') {
                            current_arg->next = args;
                            args = current_arg;
                            pp_lexer_parse(lexer);
                            parens_depth = 0;
                            continue;
                        }

                        if (lexer->tok_kind == PP_TOK_PUNCT) {
                            if (lexer->tok_punct_kind == '(') {
                                ++parens_depth;
                            } else if (lexer->tok_punct_kind == ')') {
                                if (parens_depth) {
                                    --parens_depth;
                                } else {
                                    // TODO: Diagnostic
                                }
                            }
                        }

                        preprocessor_token *token = get_new_token(pp);
                        init_pp_token_from_lexer(pp, token);
                        token->next = current_arg->tokens;
                        current_arg->tokens = token;

                        pp_lexer_parse(lexer);
                    }

                    expand_macro_internal(pp, macro, args);
                    free_expansion_args(pp, args);
                    result = true;
                } else {
                    push_token_to_stack(pp, (token) { 
                        .kind = TOK_ID,
                        .str = string_dup(pp->ea, name)
                    });
                }
            } else {
                expand_macro_internal(pp, macro, 0);
                result = true;
            }
        }
    }

    return result;
}

token 
preprocessor_get_token(preprocessor *pp) {
    token tok = {0};
    pp_lexer *lexer = pp->lexer;

    for (;;) {
        if (pp->token_list) {
            // TODO:
            /* token_list_entry *entry = pp->token_stack; */
            /* pp->token_stack = entry->next; */
            /* tok = entry->tok; */
            /* entry->next = pp->token_stack_freelist; */
            /* pp->token_stack_freelist = entry; */
            break;
        }

        pp_lexer_parse(lexer); 
        if (lexer->tok_kind == PP_TOK_PUNCT && 
            lexer->tok_punct_kind == '#' &&
            lexer->tok_at_line_start) {
            pp_lexer_parse(lexer);
            process_pp_directive(pp);
            continue;
        }

        if (expand_macro(pp)) {
            continue;
        }

        break;
    }

    return tok;
}
