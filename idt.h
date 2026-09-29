//
// Created by voiddreamz on 2026/9/28.
// it is used for stack the idt's data struct
//

#ifndef KERNEL_IDT_H
#define KERNEL_IDT_H
#include "type.h"

struct idt_entry {
    uint16_t base_low; // 急救函数地址的低 16 位
    uint16_t sel;       // 段选择子（我们的内核代码段 0x08）
    uint8_t  always0;       // 英特尔规定的死板空位，必须填 0
    uint8_t  flags;         // 权限标志（比如是不是允许用户态呼叫这个急救员）
    uint16_t base_high;     // 急救函数地址的高 16 位
} __attribute__((packed));

// 喂给 IDTR 寄存器的 6 字节指针（跟 GDT 一模一样）
// 必须 packed：lidt 硬件上只读 6 个连续字节，若中间插入对齐填充，
// base 会被读错，最终导致三重故障（表现为直接重启）。
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));


#endif //KERNEL_IDT_H