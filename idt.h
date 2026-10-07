//
// Created by voiddreamz on 2026/9/28.
// it is used for stack the idt's data struct
//

#ifndef KERNEL_IDT_H
#define KERNEL_IDT_H
#include "general.h"

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

//定义我们isr(num)里面调用isr_common_stub调用fault_handle时需要的参数
struct  regesters {
    int32 ds; //最后压入的
    int32 edi,esi,ebp,esp,ebx,edx,ecx,eax;
    int32 int_no,err_code;
    int32 eip,cs,eflags,useresp,ss;
}__attribute__((packed));


// 先定义完isr内部需要的fault_handle
char* exception_message[]={"0: Divide By Zero (除以零)",
    "1: Debug (调试异常)",
    "2: Non Maskable Interrupt (不可屏蔽中断)",
    "3: Breakpoint (断点)",
    "4: Into Detected Overflow (溢出)",
    "5: Out of Bounds (越界)",
    "6: Invalid Opcode (无效的机器码指令)",
    "7: No Coprocessor (无协处理器)",
    "8: Double Fault (双重错误 - 极其致命!)",
    "9: Coprocessor Segment Overrun",
    "10: Bad TSS (无效的任务状态段)",
    "11: Segment Not Present (段不存在)",
    "12: Stack Fault (栈错误)",
    "13: General Protection Fault (一般保护性错误 - 越权!)",
    "14: Page Fault (缺页异常 - 内存管理最常见的异常)",
    "15: Unknown Interrupt (未知中断)",
    "16: Coprocessor Fault (浮点错误)",
    "17: Alignment Check (对齐检查)",
    "18: Machine Check (机器检查错误)",
    "19: SIMD Floating-Point Exception",
    "20: Virtualization Exception (VT-x 虚拟化异常!)",
    "21: Reserved", "22: Reserved", "23: Reserved",
    "24: Reserved", "25: Reserved", "26: Reserved",
    "27: Reserved", "28: Reserved", "29: Reserved",
    "30: Reserved", "31: Reserved"
};
unsigned char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* <-- control key */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    // ... 后面的省略，全填 0 即可
};
#endif //KERNEL_IDT_H