//
// Created by voiddreamz on 2026/10/2.
// 此文件用于开启分页，以及建立分页表
//

#include "general.h"

#define PAGE_PRESENT 0x01  //表示该PAGE存在
#define PAGE_RW      0x2   //表示该PAGE可读写
#define PAGE_USER    0x4   //为 1 表示 Ring 3 用户态可访问，为 0 表示仅限内核

extern int32 pmm_malloc_page();
extern int32 pmm_free_page();

// 把页目录的物理地址塞进 CR3 寄存器
static inline void load_cr3(uint32_t dir_addr) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(dir_addr));
}

// 开启分页
static inline void enable_paging() {
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // 将 PG 位 (Bit 31) 设为 1
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));
}

void page_init() {
    int32* page_dic= (int32 *)pmm_malloc_page();
    int32* page_table= (int32 *)pmm_malloc_page();
    //clear
    for (int i=0; i < 1024; i++) {
        page_table[i]= 0;
        // 0,1,0，内核态，可写，不存在
        page_dic[i]= 0x02;
    }
    // 映射第一个表，即是说映射第一个4MB
    for (int i=0; i<1024; i++) {
        page_table[i]= (i * 4096) | PAGE_PRESENT| PAGE_RW;
    }
    page_dic[0]= ((uint32_t)page_table) | PAGE_PRESENT | PAGE_RW;
    load_cr3((uint32_t)page_dic);
    enable_paging();
}