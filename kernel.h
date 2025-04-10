#pragma once

#include "common.h"

// The base virtual address of an application image. This needs to match the
// starting address defined in `user.ld`.
#define USER_BASE 0x1000000

#define SSTATUS_SPIE (1 << 5)

#define SCAUSE_ECALL 8


#define PROCS_MAX 8     // Maximum number of processes

#define PROC_UNUSED   0 // Unused process control structure
#define PROC_RUNNABLE 1 // Runnable process
#define PROC_EXITED   2

// Memory Paging
#define SATP_SV32 (1u << 31)
#define PAGE_V    (1 << 0)   // "Valid" bit (entry is enabled)
#define PAGE_R    (1 << 1)   // Readable
#define PAGE_W    (1 << 2)   // Writable
#define PAGE_X    (1 << 3)   // Executable
#define PAGE_U    (1 << 4)   // User (accessible in user mode)

#define panic(fmt, ...) \
    do {                \
        printf("\033[1;37;41m panic! \033[0m %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
        while (1) {}    \
    } while (0)

#define read_csr(reg)   \
    ({                  \
        u32 tmp;        \
        __asm__ volatile("csrr %0, " #reg : "=r"(tmp));\
        tmp;            \
    })

#define write_csr(reg, value)   \
    do {                        \
        u32 tmp = (value);      \
        __asm__ volatile("csrw " #reg ", %0" ::"r"(tmp));\
    } while (0)

struct process {
    int pid;
    int state;      // Process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp;
    u32* page_table;
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
    i32 value;
    i32 error;
};