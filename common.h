#pragma once

typedef int bool;
typedef char i8;
typedef short i16;
typedef long i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;
typedef u32 size_t;
typedef u32 paddr_t; // Physical address type
typedef u32 vaddr_t; // Virtual address type

#define true 1
#define false 0
#define null ((void *)0)
#define align_up(value, align) __builtin_align_up(value, align)
#define is_aligned(value, align) __builtin_is_aligned(value, align)
#define offset_of(type, member) __builtin_offsetof(type, member)

// Memory allocation
#define PAGE_SIZE 4096

// Variadic function utils
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg

void* memset(void* buf, char c, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
char* strcpy(char* dst, const char* src);
int strcmp(const char* s1, const char* s2);
void putchar(char ch);
void printf(const char* fmt, ...);
