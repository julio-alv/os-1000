#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;
typedef u32 size_t;
typedef u32 paddr_t; // Physical address type
typedef u32 vaddr_t; // Virtual address type

constexpr int SYS_PUTCHAR = 1;
constexpr int SYS_GETCHAR = 2;
constexpr int SYS_EXIT = 3;
constexpr int SYS_READFILE = 4;
constexpr int SYS_WRITEFILE = 5;

#define offset_of(type, member) __builtin_offsetof(type, member)

// Memory allocation
constexpr int PAGE_SIZE = 4096;

// Variadic function utils
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg

template <typename T, typename U>
constexpr int align_up(T value, U alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T, typename U>
constexpr bool is_aligned(T value, U alignment) {
    return (value & (alignment - 1)) == 0;
}

void* memset(void* buf, char c, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
char* strcpy(char* dst, const char* src);
int strcmp(const char* s1, const char* s2);
void putchar(char ch);
void printf(const char* fmt, ...);
