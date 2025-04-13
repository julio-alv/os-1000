#pragma once

#include "common.h"

// The base virtual address of an application image. This needs to match the
// starting address defined in `user.ld`.
#define USER_BASE 0x1000000

#define SECTOR_SIZE              512
#define VIRTQ_ENTRY_NUM          16
#define VIRTIO_DEVICE_BLK        2
#define VIRTIO_BLK_PADDR         0x10001000
#define VIRTIO_REG_MAGIC         0x00
#define VIRTIO_REG_VERSION       0x04
#define VIRTIO_REG_DEVICE_ID     0x08
#define VIRTIO_REG_QUEUE_SEL     0x30
#define VIRTIO_REG_QUEUE_NUM_MAX 0x34
#define VIRTIO_REG_QUEUE_NUM     0x38
#define VIRTIO_REG_QUEUE_ALIGN   0x3c
#define VIRTIO_REG_QUEUE_PFN     0x40
#define VIRTIO_REG_QUEUE_READY   0x44
#define VIRTIO_REG_QUEUE_NOTIFY  0x50
#define VIRTIO_REG_DEVICE_STATUS 0x70
#define VIRTIO_REG_DEVICE_CONFIG 0x100
#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEAT_OK   8
#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

struct file {
    bool in_use;      // Indicates if this file entry is in use
    char name[100];   // File name
    char data[1024];  // File content
    size_t size;      // File size
};

constexpr int FILES_MAX = 2;
constexpr int DISK_MAX_SIZE = align_up(sizeof(file) * FILES_MAX, SECTOR_SIZE);

#define SSTATUS_SPIE (1 << 5)
#define SSTATUS_SUM  (1 << 18)
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
        printf("panic! %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
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
    int value;
    int error;
};

// Virtqueue Descriptor area entry.
struct virtq_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed));

// Virtqueue Available Ring.
struct virtq_avail {
    u16 flags;
    u16 index;
    u16 ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

// Virtqueue Used Ring entry.
struct virtq_used_elem {
    u32 id;
    u32 len;
} __attribute__((packed));

// Virtqueue Used Ring.
struct virtq_used {
    u16 flags;
    u16 index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

// Virtqueue.
struct virtio_virtq {
    virtq_desc descs[VIRTQ_ENTRY_NUM];
    virtq_avail avail;
    virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile u16* used_index;
    u16 last_used_index;
} __attribute__((packed));

// Virtio-blk request.
struct virtio_blk_req {
    // First descriptor: read-only from the device
    u32 type;
    u32 reserved;
    u64 sector;

    // Second descriptor: writable by the device if it's a read operation (VIRTQ_DESC_F_WRITE)
    u8 data[512];

    // Third descriptor: writable by the device (VIRTQ_DESC_F_WRITE)
    u8 status;
} __attribute__((packed));

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
    char data[];      // Array pointing to the data area following the header
    // (flexible array member)
} __attribute__((packed));

