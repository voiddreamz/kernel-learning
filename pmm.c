//
// Created by voiddreamz on 2026/10/1.
// 内存分配程序，以4KB的形式分配分页，
// 此文件包含pmm的初始化，内存分配和释放
//

#include "general.h"

//其中PAGE_SIZE每个页框由4KB组成,那么一共需要FRAME_SIZE个bit(0/1)来储存状态,则需要FRAME_SIZE/32个地址(HEX)来储存

#define PAGE_SIZE 4096
#define MEMORY_MB 128
#define MEMORY_SIZE (MEMORY_MB * 1024 * 1024)
#define FRAME_SIZE (MEMORY_SIZE / PAGE_SIZE)
#define BITMAP_SIZE (FRAME_SIZE / 32)

extern int32 _kernel_end;

int32 bit_map[BITMAP_SIZE];

// 其中的index/32是我们想要放的大槽位，我们要通过index % 32计算它在每个槽位的小位置。然后通过将0000 0001移动多少个位置来设置它。
static void set_bit(int32 index) {
    bit_map[index / 32] |= (1 << (index % 32));
}
// ~是按位取反
static void clear_bit(int32 index) {
    bit_map[index / 32] &= ~(1 << (index % 32));
}

static int check_bit(int32 index) {
    return (bit_map[index / 32] & (1 << (index % 32))) != 0;
}

void pmm_init() {
    //
    for (int i=0; i<BITMAP_SIZE; i++) {
        bit_map[i] = 0;
    }
    //计算内核所占的大小
    // _kernel_end 是链接脚本在 .bss 末尾定义的符号，它的“地址”才是内核结束地址，
    // 所以要用 &_kernel_end 取值（写成 _kernel_end 会变成读取该地址处的内存内容）
    int32 kernel_end_addr = (int32)&_kernel_end;
    int32 kernel_pages = (kernel_end_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    //关于这里的+4095的原因？因为是取整运算，我们加4095去除以4096不会增加数字。但是如果我们刚好多了一个，多了两个位，即是说如果我们是4097个，那么就是直接增加了一个
    for (int32 i=0;i<kernel_pages;i++) {
        set_bit(i);
    }
}

int32 pmm_malloc_page() {
    for (int32 i=0; i<BITMAP_SIZE; i++) {
        if (bit_map[i] != 0xFFFFFFFF) {
            for (int32 j = 0; j < 32; j++) {
                int32 index = (i * 32) + j;
                if (!check_bit(index)) {
                    set_bit(index);
                    // 返回物理地址（页框号 * 4KB），和 pmm_free_page(addr) 以及
                    // page_init() 里直接当指针用的写法保持一致
                    return index;
                }
            }
        }
    }
    return 0;
}

void pmm_free_page(int32 addr) {
    int32 index = addr / PAGE_SIZE;
    clear_bit(index);
}
