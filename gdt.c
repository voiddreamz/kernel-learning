//
// Created by voiddreamz on 2026/9/28.
//

#include "gdt.h"
struct gdt_entry gdt[4]; // 这里定义四个，第一个是空的，第二个是给ring0的数据段，第三个给ring0的代码段，第四个给未来的TSS
struct gdt_ptr gdtr;
extern void gdt_flush(int64);

void set_gdt_gate(int num, uint32_t base,uint32_t limit,uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle= (base>> 16) & 0xFF;
    gdt[num].base_high= (base>> 24) & 0xFF;
    //设置界限：
    gdt[num].limit_low =(limit & 0xFF);
    //设置粒度
    gdt[num].granularity=(limit>>16)& 0x0F;
    gdt[num].granularity |= (gran & 0xF0);  //此处的|= 相当于 a = a | b ，按位或
    gdt[num].access  = access;
}

// 内核初始化 GDT 的主入口
void gdt_install() {
    gdtr.limit=(sizeof(struct gdt_entry)* 4) -1;
    gdtr.base = (int64)&gdt;
    // 于此配置 GDT 表项：
    set_gdt_gate(0, 0, 0, 0, 0);                // 第 0 项：英特尔规定的死板空位
    set_gdt_gate(1,0,0xFFFFFFFF,0x9A,0xAF);
    set_gdt_gate(2,0,0xFFFFFFFF,0x92,0xCF);
    gdt_flush((uint32_t)&gdtr);
}