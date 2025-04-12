#include "user.h"

extern "C" char __stack_top[];

int syscall(int sysno, int arg0, int arg1, int arg2) {
    int a0 asm("a0") = arg0;
    int a1 asm("a1") = arg1;
    int a2 asm("a2") = arg2;
    int a3 asm("a3") = sysno;

    asm volatile("ecall"
        : "=r"(a0)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
        : "memory");

    return a0;
}

void putchar(char ch) {
    syscall(SYS_PUTCHAR, ch, 0, 0);
}

int getchar(void) {
    return syscall(SYS_GETCHAR, 0, 0, 0);
}

__attribute__((noreturn)) void exit(void) {
    syscall(SYS_EXIT, 0, 0, 0);
    for (;;); // Just in case!
}

int readfile(const char* filename, char* buf, int len) {
    return syscall(SYS_READFILE, (int)filename, (int)buf, len);
}

int writefile(const char* filename, const char* buf, int len) {
    return syscall(SYS_WRITEFILE, (int)filename, (int)buf, len);
}

__attribute__((section(".text.start")))
__attribute__((naked))
extern "C" void start(void) {
    asm volatile(
        "mv sp, %[stack_top] \n"
        "call main           \n"
        "call exit           \n"
        ::[stack_top] "r" (__stack_top)
        );
}