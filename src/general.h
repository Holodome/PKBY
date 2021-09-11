#pragma once 
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef u32 b32;
typedef u8 b8;
typedef uintptr_t uptr;
typedef float f32;
typedef double f64;
#define TRUE 1
#define FALSE 0
#define TO_BOOL(_expr) ((_expr) ? TRUE : FALSE)
#define ARRAY_SIZE(_a) ((uptr)(sizeof(_a) / sizeof(*(_a))))
#define CT_ASSERT(_expr) static_assert(_expr, "Assertion " #_expr " failed")
#define KB(_b) ((uptr)(_b) << 10)
#define MB(_b) (KB(_b) << 10)
#define IS_POW2(_n) (((_n) & (_n - 1)) == 0)
#define LLIST_ADD(_list, _node) \
do { \
    (_node)->next = (_list); \
    (_list) = (_node); \
} while (0); 
#define LLIST_POP(_list) \
do { \
    (_list) = (_list)->next; \
} while (0);

#include <assert.h>
#include <stdarg.h>
#ifdef _MSC_VER
#define DBG_BREAKPOINT __debugbreak()
#else 
#define DBG_BREAKPOINT __asm__("int $3")
#endif 
#define STRUCT_FIELD(_struct, _field) (((_struct *)0)->_field)
#define STRUCT_OFFSET(_struct, _field) ((uptr)((u8 *)(&STRUCT_FIELD(_struct, _field))))
#define NOT_IMPLEMENTED assert(!"NotImplemented")
#define INVALID_DEFAULT_CASE assert(!"InvalidDefaultCase")