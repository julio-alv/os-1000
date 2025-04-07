#include "common.h"

void* memset(void* buf, char c, size_t n)
{
    u8* p = (u8*)buf;
    while (n--)
        *p++ = c;
    return buf;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

char* strcpy(char* dst, const char* src)
{
    char* d = dst;
    while (*src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(u8*)s1 - *(u8*)s2;
}

void putchar(char ch);

void printf(const char* fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt)
    {
        if (*fmt != '%') {
            putchar(*fmt++);
            continue;
        }

        fmt++;
        if (!*fmt) {
            putchar('%');
            break;
        }

        switch (*fmt)
        {
        case '%':
            putchar('%');
            break;
        case 's':
            const char* s = va_arg(vargs, const char*);
            while (*s)
                putchar(*s++);
            break;
        case 'd': // Decimal
        {
            int value = va_arg(vargs, int);
            unsigned magnitude = value; // https://github.com/nuta/operating-system-in-1000-lines/issues/64
            if (value < 0)
            {
                putchar('-');
                magnitude = -magnitude;
            }

            unsigned divisor = 1;
            while (magnitude / divisor > 9)
                divisor *= 10;

            while (divisor > 0)
            {
                putchar('0' + magnitude / divisor);
                magnitude %= divisor;
                divisor /= 10;
            }

            break;
        }
        case 'x': // Hexadecimal
        {
            unsigned value = va_arg(vargs, unsigned);
            for (int i = 7; i >= 0; i--)
            {
                unsigned nibble = (value >> (i * 4)) & 0xf;
                putchar("0123456789abcdef"[nibble]);
            }
            break;
        }
        }
        fmt++;
    }

    va_end(vargs);
}
