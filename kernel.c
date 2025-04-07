#include "kernel.h"
#include "common.h"

extern i8 __bss[], __bss_end[], __stack_top[];
extern i8 __free_ram[], __free_ram_end[];

paddr_t alloc_pages(u32 n) {
    static paddr_t next_paddr = (paddr_t)__free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t)__free_ram_end)
        panic("out of memory");

    memset((void*)paddr, 0, n * PAGE_SIZE);
    return paddr;
}

struct sbiret sbi_call(
    i16 arg0, i16 arg1, i16 arg2, i16 arg3,
    i16 arg4, i16 arg5, i16 fid, i16 eid)
{
    register i16 a0 asm("a0") = arg0;
    register i16 a1 asm("a1") = arg1;
    register i16 a2 asm("a2") = arg2;
    register i16 a3 asm("a3") = arg3;
    register i16 a4 asm("a4") = arg4;
    register i16 a5 asm("a5") = arg5;
    register i16 a6 asm("a6") = fid;
    register i16 a7 asm("a7") = eid;

    asm volatile("ecall"
        : "=r"(a0), "=r"(a1)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory");

    return (struct sbiret) { .value = a1, .error = a0 };
}

void putchar(char ch)
{
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    asm volatile(
        // Retrieve the kernel stack of the running process from sscratch.
        "csrrw sp, sscratch, sp\n"

        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // Retrieve and save the sp at the time of exception.
        "csrr a0, sscratch\n"
        "sw a0,  4 * 30(sp)\n"

        // Reset the kernel stack.
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n");
}

void handle_trap(struct trap_frame* f) {
    u32 scause = read_csr(scause);
    u32 stval = read_csr(stval);
    u32 user_pc = read_csr(sepc);

    panic("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

__attribute__((naked))
void switch_context(
    u32* prev_sp,
    u32* next_sp
) {
    asm volatile(
        // Save callee-saved registers onto the current process's stack.
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n"         // *prev_sp = sp;
        "lw sp, (a1)\n"         // Switch stack pointer (sp) here

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"  // We've popped 13 4-byte registers from the stack
        "ret\n"
        );
}

struct process procs[PROCS_MAX]; // All process control structures.

struct process* create_process(u32 pc) {
    // Find an unused process control structure.
    struct process* proc = null;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        panic("no free process slots");

    // Stack callee-saved registers. These register values will be restored in
    // the first context switch in switch_context.
    u32* sp = (u32*)&proc->stack[sizeof(proc->stack)];
    *--sp = 0;          // s11
    *--sp = 0;          // s10
    *--sp = 0;          // s9
    *--sp = 0;          // s8
    *--sp = 0;          // s7
    *--sp = 0;          // s6
    *--sp = 0;          // s5
    *--sp = 0;          // s4
    *--sp = 0;          // s3
    *--sp = 0;          // s2
    *--sp = 0;          // s1
    *--sp = 0;          // s0
    *--sp = (u32)pc;    // ra

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (u32)sp;
    return proc;
}

void delay(void) {
    for (int i = 0; i < 30000000; i++)
        asm volatile("nop");
}

struct process* proc_a;
struct process* proc_b;

struct process* current_proc; // Currently running process
struct process* idle;    // Idle process

void yield(void) {
    // Search for a runnable process
    struct process* next = idle;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process* proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    // If there's no runnable process other than the current one, return and continue processing
    if (next == current_proc)
        return;

    asm volatile(
        "csrw sscratch, %[sscratch]\n"
        ::[sscratch] "r" ((u32)&next->stack[sizeof(next->stack)]));

    // Context switch
    struct process* prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}


void proc_a_entry(void) {
    printf("starting process A\n");
    while (1) {
        putchar('A');
        yield();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        putchar('B');
        yield();
    }
}

void kernel_main(void)
{
    memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);

    printf("\n\n");

    write_csr(stvec, (u32)kernel_entry);
    // asm volatile("unimp");

    idle = create_process((u32)null);
    idle->pid = 0; // idle
    current_proc = idle;

    proc_a = create_process((u32)proc_a_entry);
    proc_b = create_process((u32)proc_b_entry);
    yield();

    panic("switched to idle process");

    for (;;)
        asm volatile("wfi");
}

__attribute__((naked))
__attribute__((section(".text.boot")))
void boot(void)
{
    asm volatile(
        "mv sp, %0\n"
        "j kernel_main\n"
        :: "r"(__stack_top));
}
