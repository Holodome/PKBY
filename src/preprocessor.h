#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "types.h"

struct pp_lexer;
struct bump_allocator;
struct pp_token;
struct c_type;
struct token;

#define PREPROCESSOR_MACRO_HASH_SIZE 2048

typedef struct pp_macro_arg {
    struct pp_macro_arg *next;
    // Name of the argument (__VA_ARGS__ for variadic arguments)
    string name;
    struct pp_token *toks;
} pp_macro_arg;

typedef enum {
    // Object-like macro
    PP_MACRO_OBJ = 0x1,
    // Function-like macro
    PP_MACRO_FUNC = 0x2,
    // NOTE: These macros generated depending on location in source code,
    // so their definition differs. That's why they are put in their own macro
    // kinds
    // __FILE__
    PP_MACRO_FILE = 0x3,
    // __LINE__
    PP_MACRO_LINE = 0x4,
    // __COUNTER__
    PP_MACRO_COUNTER = 0x5,
    // __INCLUDE_LEVEL__
    PP_MACRO_INCLUDE_LEVEL = 0x6,
} pp_macro_kind;

typedef struct pp_macro {
    struct pp_macro *next;

    pp_macro_kind kind;
    string name;
    uint32_t name_hash;

    bool is_variadic;
    uint32_t arg_count;
    pp_macro_arg *args;
    struct pp_token *definition;
} pp_macro;

typedef struct pp_conditional_include {
    bool is_included;
    bool is_after_else;
    struct pp_conditional_include *next;
} pp_conditional_include;

typedef struct pp_macro_expansion_arg {
    struct p_token *tokens;
    struct pp_macro_expansion_arg *next;
} pp_macro_expansion_arg;

typedef struct preprocessor {
    struct bump_allocator *a;
    struct allocator *ea;

    struct file_storage *fs;
    struct pp_token *toks;

    // Value for __COUNTER__
    uint32_t counter_value;
    pp_conditional_include *cond_incl_stack;
    pp_macro *macro_hash[PREPROCESSOR_MACRO_HASH_SIZE];

    pp_macro_arg *macro_arg_freelist;
    pp_conditional_include *cond_incl_freelist;
    pp_macro_expansion_arg *macro_expansion_arg_freelist;
} preprocessor;

void init_pp(preprocessor *pp, string filename);
bool pp_parse(preprocessor *pp, struct token *tok);

#endif
