#include "lib/strings.h"

#include "lib/memory.h"
#include "lib/stream.h"

#include <stdlib.h>
#define STB_SPRINTF_IMPLEMENTATION
#include "thirdparty/stb_sprintf.h"

uptr vfmt(char *buf, uptr buf_size, const char *format, va_list args) {
    return stbsp_vsnprintf(buf, buf_size, format, args);
}

uptr fmt(char *buf, uptr buf_size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    uptr result = vfmt(buf, buf_size, format, args);
    va_end(args);
    return result;
}

b32 is_ascii(u32 symb) {
    return symb <= 0x7F;
}

b32 is_space(u32 symb) {
    return symb == ' ' || symb == '\r' || symb == '\n';
}

b32 is_nextline(u32 symb) {
    return (symb == '\n' || symb == '\r');  
}

b32 is_digit(u32 symb) {
    return ('0' <= symb && symb <= '9');
}

b32 is_alpha(u32 symb) {
    return ('a' <= symb && symb <= 'z') || ('A' <= symb && symb <= 'Z');
}

b32 is_ident_start(u32 symb) {
    return is_alpha(symb) || symb == '_';
}

b32 is_ident(u32 symb) {
    return is_alpha(symb) || is_digit(symb) || symb == '_';
}

b32 is_punct(u32 symb) {
    return symb == '!' || symb == '\"' || symb == '#' || symb == '$' || symb == '%' || symb == '&' 
        || symb == '\'' || symb == '(' || symb == ')' || symb == '*' || symb == '+' || symb == ',' 
        || symb == '-' || symb == '.' || symb == '/' || symb == ':' || symb == ';' || symb == '<' 
        || symb == '=' || symb == '>' || symb == '?' || symb == '@' || symb == '[' || symb == '\\' 
        || symb == ']' || symb == '^' || symb == '_' || symb == '`' || symb == '{' || symb == '|' 
        || symb == '}' || symb == '~';
}

b32 is_real(u32 symb) {
    return is_digit(symb) || symb == '-' || symb == '+' || symb == 'e' || symb == 'E' || symb == '.';
}

b32 is_int(u32 symb) {
    return is_digit(symb) || symb == '-' || symb == '+';
}

f64 str_to_f64(const char *str) {
    return atof(str);
}

i64 str_to_i64(const char *str) {
    return atoll(str);
}

uptr str_cp(char *bf, uptr bf_sz, const char *str) {
    uptr bytes_to_copy = bf_sz;
    uptr len = str_len(str) + 1;
    if (len < bytes_to_copy) {
        bytes_to_copy = len;
    }
    mem_copy(bf, str, bytes_to_copy);
    return bytes_to_copy;
}

b32 str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
    }
    b32 result = *a == 0 && *b == 0;
    return result;
}

b32 str_eqn(const char *a, const char *b, uptr n) {
    while (*a && *b && (*a == *b) && n--) {
        ++a;
        ++b;
    }
    b32 result = n == 0;
    return result;
}

uptr str_len(const char *str) {
    uptr result = 0;
    while (*str++) {
        ++result;
    }
    return result;
}

u32 utf8_encode(u32 utf32, u8 *dst) {
    u32 len = 0;
    if (utf32 <= 0x0000007F) {
        *dst = utf32;   
        len = 1;    
    } else if (utf32 <= 0x000007FF) {
        *dst++ = 0xC0 | (utf32 >> 6);
        *dst++ = 0x80 | (utf32 & 0x3F);
        len = 2;
    } else if (utf32 <= 0x0000FFFF) {
        *dst++ = 0xE0 | (utf32 >> 12);
        *dst++ = 0x80 | ((utf32 >> 6) & 0x3F);
        *dst++ = 0x80 | (utf32 & 0x3F);
        len = 3;
    } else if (utf32 <= 0x0010FFFF) {
        *dst++ = 0xF0 | (utf32 >> 18);
        *dst++ = 0x80 | ((utf32 >> 12) & 0x3F);
        *dst++ = 0x80 | ((utf32 >> 6) & 0x3F);
        *dst++ = 0x80 | (utf32 & 0x3F);
        len = 4;
    } 
    return len;
}

u32 utf8_decode(const u8 *src, u32 *len_out) {
    u32 len = 0;
    u32 utf32 = 0;
    if ((src[0] & 0x80) == 0x00) {
        utf32 = src[0];
        len = 1;
    } else if ((src[0] & 0xE0) == 0xC0) {
        if ((src[1] & 0xC0) == 0x80) {
            utf32 = ((src[0] & 0x1F) << 6) | (src[1] & 0x3F);     
            len = 2; 
        }
    } else if ((src[0] & 0xF0) == 0xE0) {
        if ((src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80) {
            utf32 = ((src[0] & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);     
            len = 3;
        }
    } else if ((src[0] & 0xF8) == 0xF0) {
        if ((src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80 && (src[3] & 0xC0) == 0x80) {
            utf32 = ((src[0] & 0x03) << 18) | ((src[1] & 0x3F) << 12) | ((src[2] & 0x3F) << 6) 
                | (src[3] & 0x3F);     
            len = 4;
        }
    }
    
    *len_out = len;
    return utf32;
}

uptr outf(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    uptr result = outv(msg, args);
    va_end(args);
    return result;
}

uptr outv(const char *msg, va_list args) {
    OutStream *st = get_stdout_stream();
    uptr result = out_streamv(st, msg, args);
    out_stream_flush(st);
    return result;
}

uptr erroutf(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    uptr result = erroutv(msg, args);
    va_end(args);
    return result;
}

uptr erroutv(const char *msg, va_list args) {
    OutStream *st = get_stderr_stream();
    uptr result = out_streamv(st, msg, args);
    out_stream_flush(st);
    return result;
}
