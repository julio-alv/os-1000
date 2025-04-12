#include "user.h"

extern char __stack_top[];

int syscall(int sysno, int arg0, int arg1, int arg2) {
    int a0;
    asm volatile(
        "mv a0, %1\n"
        "mv a1, %2\n"
        "mv a2, %3\n"
        "mv a3, %4\n"
        "ecall\n"
        "mv %0, a0\n"
        : "=r"(a0)
        : "r"(arg0), "r"(arg1), "r"(arg2), "r"(sysno)
        : "a0", "a1", "a2", "a3", "memory");

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
    for (;;);
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