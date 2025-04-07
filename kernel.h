#pragma once

#include "common.h"

// redefinition of __asm__ to improve readability
#define asm __asm__

#define PROCS_MAX 8     // Maximum number of processes

#define PROC_UNUSED   0 // Unused process control structure
#define PROC_RUNNABLE 1 // Runnable process

#define panic(fmt, ...) \
    do {                \
        printf("\033[1;37;41m panic! \033[0m %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
        while (1) {}    \
    } while (0)

#define read_csr(reg)   \
    ({                  \
        u32 tmp;        \
        asm volatile("csrr %0, " #reg : "=r"(tmp));\
        tmp;            \
    })

#define write_csr(reg, value)   \
    do {                        \
        u32 tmp = (value);      \
        asm volatile("csrw " #reg ", %0" ::"r"(tmp));\
    } while (0)

struct process {
    int pid;        // Process ID
    int state;      // Process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp;     // Stack pointer
    u8 stack[8192]; // Kernel stack
};

struct trap_frame {
    u32 ra;
    u32 gp;
    u32 tp;
    u32 t0;
    u32 t1;
    u32 t2;
    u32 t3;
    u32 t4;
    u32 t5;
    u32 t6;
    u32 a0;
    u32 a1;
    u32 a2;
    u32 a3;
    u32 a4;
    u32 a5;
    u32 a6;
    u32 a7;
    u32 s0;
    u32 s1;
    u32 s2;
    u32 s3;
    u32 s4;
    u32 s5;
    u32 s6;
    u32 s7;
    u32 s8;
    u32 s9;
    u32 s10;
    u32 s11;
    u32 sp;
} __attribute__((packed));

struct sbiret
{
    long value;
    long error;
};