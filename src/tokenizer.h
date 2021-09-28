// Author: Holodome
// Date: 21.08.2021 
// File: pkby/src/tokenizer.h
// Version: 0
//
// Defines functions used in parsing text files via splitting them into tokens.
#pragma once 

#include "lib/general.h"
#include "lib/memory.h"
#include "lib/stream.h" 
#include "error_reporter.h"
#include "string_storage.h"

// @NOTE(hl): These limits are set for easier manipulation in case of future updates and usages
// of this code.
// Basically, since u8 is not enough to store all simple token kinds, 
// we have to use u16, which offers a lot of space to assign token kinds to
#define TOKENIZER_DEFAULT_SCRATCH_BUFFER_SIZE KB(1)
#define TOKEN_GENERAL (0x100)
#define MAX_GENERAL_TOKEN_COUNT 0x100
#define TOKEN_KEYWORD (TOKEN_GENERAL + MAX_GENERAL_TOKEN_COUNT)
#define MAX_KEYWORD_TOKEN_COUNT 0x100
#define TOKEN_MULTISYMB (TOKEN_KEYWORD + MAX_KEYWORD_TOKEN_COUNT)
#define MAX_MULTISYMB_TOKEN_COUNT 0x100
#define IS_TOKEN_ASCII(_tok) (0 <= (_tok) && (_tok) <= 0xFF)
#define IS_TOKEN_GENERAL(_tok) (TOKEN_GENERAL <= (_tok) && (_tok) < TOKEN_GENERAL + MAX_GENERAL_TOKEN_COUNT)
#define IS_TOKEN_KEYWORD(_tok) (TOKEN_KEYWORD <= (_tok) && (_tok) < TOKEN_KEYWORD + MAX_KEYWORD_TOKEN_COUNT)
#define IS_TOKEN_MULTISYMB(_tok) (TOKEN_MULTISYMB <= (_tok) && (_tok) < TOKEN_MULTISYMB + MAX_MULTISYMB_TOKEN_COUNT)
// Token kind space is reserved to handle expanding easilly
// Values 0-255 correspond to single-symbol ASCII tokens
// Values 256-287 (32) correspond to basic kinds
// Values 288-351 (64) correspond to keywords 
// Values 352-(TOKEN_COUNT - 1) correspond to multisymbol tokens
enum {
    TOKEN_NONE = 0x0,
    // Mentally insert ASCII tokens here...
    TOKEN_EOS = TOKEN_GENERAL,
    TOKEN_ERROR, 
    TOKEN_IDENT, // value_str
    TOKEN_INT, // value_int
    TOKEN_REAL, // value_real
    TOKEN_STR, // value_str,
    
    TOKEN_KW_PRINT = TOKEN_KEYWORD, // print
    TOKEN_KW_WHILE, // while
    TOKEN_KW_RETURN, // return 
    TOKEN_KW_IF, // if 
    TOKEN_KW_ELSE, // else
    TOKEN_KW_INT, // int 
    TOKEN_KW_FLOAT, // float
    
    // Digraphs and trigraphs
    TOKEN_ILSHIFT = TOKEN_MULTISYMB, // <<=
    TOKEN_IRSHIFT, // >>= 
    
    TOKEN_AUTO_DECL, // :=
    TOKEN_ARROW, // ->
    TOKEN_LE, // <=
    TOKEN_GE, // >=
    TOKEN_EQ, // == 
    TOKEN_NEQ, // !=
    TOKEN_LSHIFT, // <<
    TOKEN_RSHIFT, // >>
    TOKEN_IADD, // +=
    TOKEN_ISUB, // -=
    TOKEN_IAND, // &=
    TOKEN_IOR, // |=
    TOKEN_IXOR, // ^=
    TOKEN_IDIV, // /=
    TOKEN_IMUL, // *=
    TOKEN_IMOD, // %=
    TOKEN_LOGICAL_AND, // &&
    TOKEN_LOGICAL_OR, // ||
};

// Does token mean assignment (e.g. =, +=, etc.)
b32 is_token_assign(u32 tok);

typedef struct Token {
    u32 kind;
    union {
        StringID value_str;
        i64 value_int;
        f64 value_real;
    };
    SrcLoc src_loc;
} Token;

// Structure storing state of parsing some buffer 
//
// @NOTE(hl): Single tokenizer object is meant to be parsing single file as individual code unit 
//
// @NOTE All tokens generated by tokenizer have the same lifetime as it is.
// :With freeing of tokenizer, all of its tokens are freed too.
typedef struct Tokenizer {
    MemoryArena arena;
    InStream *st;
    ErrorReporter *er;
    StringStorage *ss;
    // @NOTE(hl): FileID for location is get from st
    SrcLoc curr_loc;
    // Buffer for internal use. When parsing multiline symbols, 
    // this buffer is used for it. Basically size of scratch buffer defines maximum length 
    // of identifier.
    // @TODO(hl): There is special case for parsing big strings - we need to think how we would handle that
    u32 scratch_buffer_size;
    u32 scratch_buffer_used; 
    u8 *scratch_buffer;
    
    u32 keyword_count;
    u64 keyword_hashes[MAX_KEYWORD_TOKEN_COUNT];
    
    Token *active_token;
} Tokenizer;

Tokenizer *create_tokenizer(ErrorReporter *er, StringStorage *ss, InStream *st, FileID file);
void destroy_tokenizer(Tokenizer *tokenizer);
// Returns current token. Stores token until it's eaten
Token *peek_tok(Tokenizer *tokenizer);
// Tell tokenizer to return next token on next peek_tok call
void eat_tok(Tokenizer *tokenizer);
// call eat_tok and return peek_tok
// @NAMING(hl): Unclear name (eat_peek_tok?)
Token *peek_next_tok(Tokenizer *tokenizer);

uptr fmt_tok_kind(char *buf, uptr buf_sz, u32 kind);
uptr fmt_tok(char *buf, uptr buf_sz, StringStorage *ss, Token *token);
