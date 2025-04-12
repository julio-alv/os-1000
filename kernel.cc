#include "kernel.h"
#include "common.h"

extern "C" char __bss[], __bss_end[], __stack_top[];
extern "C" char __free_ram[], __free_ram_end[];
extern "C" char __kernel_base[];
extern "C" char _binary_shell_bin_start[], _binary_shell_bin_size[];

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
    i16 a0 asm("a0") = arg0;
    i16 a1 asm("a1") = arg1;
    i16 a2 asm("a2") = arg2;
    i16 a3 asm("a3") = arg3;
    i16 a4 asm("a4") = arg4;
    i16 a5 asm("a5") = arg5;
    i16 a6 asm("a6") = fid;
    i16 a7 asm("a7") = eid;

    asm volatile("ecall"
        : "=r"(a0), "=r"(a1)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory");

    return (struct sbiret) { .value = a1, .error = a0 };
}

// ↓ __attribute__((naked)) is very important!
__attribute__((naked)) void user_entry(void) {
    asm volatile(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        "sret                      \n"
        ::[sepc] "r" (USER_BASE),
        [sstatus] "r" (SSTATUS_SPIE | SSTATUS_SUM));
}

void map_page(u32* table1, u32 vaddr, paddr_t paddr, u32 flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        panic("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        panic("unaligned paddr %x", paddr);

    u32 vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        // Create the non-existent 2nd level page table.
        u32 pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    // Set the 2nd level page table entry to map the physical page.
    u32 vpn0 = (vaddr >> 12) & 0x3ff;
    u32* table0 = (u32*)((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

struct process procs[PROCS_MAX]; // All process control structures.

struct process* create_process(const void* image, size_t image_size) {
    // Find an unused process control structure.
    struct process* proc = nullptr;
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
    *--sp = (u32)user_entry;    // ra

    // Map kernel pages.
    u32* page_table = (u32*)alloc_pages(1);
    for (paddr_t paddr = (paddr_t)__kernel_base;
        paddr < (paddr_t)__free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);

    // Map user pages.
    for (u32 off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        // Handle the case where the data to be copied is smaller than the
        // page size.
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        // Fill and map the page.
        memcpy((void*)page, (const char*)image + off, copy_size);
        map_page(page_table, USER_BASE + off, page,
            PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (u32)sp;
    proc->page_table = page_table;
    return proc;
}

void delay(void) {
    for (int i = 0; i < 30000000; i++)
        asm volatile("nop");
}

struct process* current_proc; // Currently running process
struct process* idle;    // Idle process

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
        "ret\n");
}

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
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        ::[satp] "r" (SATP_SV32 | ((u32)next->page_table / PAGE_SIZE)),
        [sscratch] "r" ((u32)&next->stack[sizeof(next->stack)])
        );

    // Context switch
    struct process* prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

u32 virtio_reg_read32(unsigned offset) {
    return *((volatile u32*)(VIRTIO_BLK_PADDR + offset));
}

u64 virtio_reg_read64(unsigned offset) {
    return *((volatile u64*)(VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, u32 value) {
    *((volatile u32*)(VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, u32 value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

struct virtio_virtq* blk_request_vq;
struct virtio_blk_req* blk_req;
paddr_t blk_req_paddr;
unsigned blk_capacity;

struct virtio_virtq* virtq_init(unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq* vq = (struct virtio_virtq*)virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile u16*)&vq->used.index;
    // 1. Select the queue writing its index (first queue is 0) to QueueSel.
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 5. Notify the device about the queue size by writing the size to QueueNum.
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 6. Notify the device about the used alignment by writing its value in bytes to QueueAlign.
    virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
    // 7. Write the physical number of the first page of the queue to the QueuePFN register.
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
    return vq;
}

void virtio_blk_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        panic("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        panic("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        panic("virtio: invalid device id");

    // 1. Reset the device.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. Set the DRIVER status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 5. Set the FEATURES_OK status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);
    // 7. Perform device-specific setup, including discovery of virtqueues for the device
    blk_request_vq = virtq_init(0);
    // 8. Set the DRIVER_OK status bit.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // Get the disk capacity.
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("virtio-blk: capacity is %d bytes\n", blk_capacity);

    // Allocate a region to store requests to the device.
    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req*)blk_req_paddr;
}

// Notifies the device that there is a new request. `desc_index` is the index
// of the head descriptor of the new request.
void virtq_kick(struct virtio_virtq* vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

// Returns whether there are requests being processed by the device.
bool virtq_is_busy(struct virtio_virtq* vq) {
    return vq->last_used_index != *vq->used_index;
}

// Reads/writes from/to virtio-blk device.
void read_write_disk(void* buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
            sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // Construct the request according to the virtio-blk specification.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // Construct the virtqueue descriptors (using 3 descriptors).
    struct virtio_virtq* vq = blk_request_vq;
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = sizeof(u32) * 2 + sizeof(u64);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = blk_req_paddr + offset_of(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = blk_req_paddr + offset_of(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(u8);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // Notify the device that there is a new request.
    virtq_kick(vq, 0);

    // Wait until the device finishes processing.
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: If a non-zero value is returned, it's an error.
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
            sector, blk_req->status);
        return;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}

void putchar(char ch)
{
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

i32 getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

struct file files[FILES_MAX];
u8 disk[DISK_MAX_SIZE];

int oct2int(char* oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

void fs_init(void) {
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, false);

    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct tar_header* header = (struct tar_header*)&disk[off];
        if (header->name[0] == '\0')
            break;

        if (strcmp(header->magic, "ustar") != 0)
            panic("invalid tar header: magic=\"%s\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file* file = &files[i];
        file->in_use = true;
        strcpy(file->name, header->name);
        memcpy(file->data, header->data, filesz);
        file->size = filesz;
        printf("file: %s, size=%d\n", file->name, file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

void fs_flush(void) {
    // Copy all file contents into `disk` buffer.
    memset(disk, 0, sizeof(disk));
    unsigned off = 0;
    for (int file_i = 0; file_i < FILES_MAX; file_i++) {
        struct file* file = &files[file_i];
        if (!file->in_use)
            continue;

        struct tar_header* header = (struct tar_header*)&disk[off];
        memset(header, 0, sizeof(*header));
        strcpy(header->name, file->name);
        strcpy(header->mode, "000644");
        strcpy(header->magic, "ustar");
        strcpy(header->version, "00");
        header->type = '0';

        // Turn the file size into an octal string.
        int filesz = file->size;
        for (int i = sizeof(header->size); i > 0; i--) {
            header->size[i - 1] = (filesz % 8) + '0';
            filesz /= 8;
        }

        // Calculate the checksum.
        int checksum = ' ' * sizeof(header->checksum);
        for (unsigned i = 0; i < sizeof(struct tar_header); i++)
            checksum += (unsigned char)disk[off + i];

        for (int i = 5; i >= 0; i--) {
            header->checksum[i] = (checksum % 8) + '0';
            checksum /= 8;
        }

        // Copy file data.
        memcpy(header->data, file->data, file->size);
        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    // Write `disk` buffer into the virtio-blk.
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    printf("wrote %d bytes to disk\n", sizeof(disk));
}

file* fs_lookup(const char* filename) {
    for (int i = 0; i < FILES_MAX; i++) {
        file* file = &files[i];
        if (!strcmp(file->name, filename))
            return file;
    }

    return nullptr;
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

void handle_syscall(struct trap_frame* f) {
    switch (f->a3) {
    case SYS_PUTCHAR:
        putchar(f->a0);
        break;
    case SYS_GETCHAR:
        while (1) {
            i32 ch = getchar();
            if (ch >= 0) {
                f->a0 = ch;
                break;
            }

            yield();
        }
        break;
    case SYS_EXIT:
        printf("process %d exited\n", current_proc->pid);
        current_proc->state = PROC_EXITED;
        yield();
        panic("unreachable");
    case SYS_READFILE:
    case SYS_WRITEFILE: {
        const char* filename = (const char*)f->a0;
        char* buf = (char*)f->a1;
        int len = f->a2;
        struct file* file = fs_lookup(filename);
        if (!file) {
            printf("file not found: %s\n", filename);
            f->a0 = -1;
            break;
        }

        if (len > (int)sizeof(file->data))
            len = file->size;

        if (f->a3 == SYS_WRITEFILE) {
            memcpy(file->data, buf, len);
            file->size = len;
            fs_flush();
        }
        else {
            memcpy(buf, file->data, len);
        }

        f->a0 = len;
        break;
    }
    default:
        panic("unexpected syscall a3=%x\n", f->a3);
    }
}

extern "C" void handle_trap(struct trap_frame* f) {
    u32 scause = read_csr(scause);
    u32 stval = read_csr(stval);
    u32 user_pc = read_csr(sepc);

    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    }
    else {
        panic("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    write_csr(sepc, user_pc);
}


extern "C" void kernel_main(void)
{
    memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);

    printf("\n\n");

    write_csr(stvec, (u32)kernel_entry);

    virtio_blk_init();
    fs_init();

    char buf[SECTOR_SIZE];
    read_write_disk(buf, 0, false /* read from the disk */);
    printf("first sector: %s\n", buf);

    strcpy(buf, "hello from kernel!!!\n");
    read_write_disk(buf, 0, true /* write to the disk */);

    idle = create_process(nullptr, 0);
    idle->pid = 0; // idle
    current_proc = idle;

    create_process(_binary_shell_bin_start, (size_t)_binary_shell_bin_size);

    yield();
    panic("switched to idle process");

    for (;;)
        asm volatile("wfi");
}

__attribute__((naked))
__attribute__((section(".text.boot")))
extern "C" void boot(void)
{
    asm volatile(
        "mv sp, %[stack_top]\n"
        "j kernel_main\n"
        ::[stack_top] "r" (__stack_top));
}
