//
// Created by voiddreamz on 2026/9/28.
//

#include "idt.h"

struct idt_entry idt[256];
struct idt_ptr idtr;

extern void idt_flush(uint32_t);
extern void isr0();

extern void isr0_handle() {
    char* video_momory_addr= (char *)0xb8000;
    char* error_message= "System Panic! Devide by zero Exception";
    //将报错打印在屏幕的第二行
    int j=160;
    int i=0;
    while (error_message[i]!='\0') {
        video_momory_addr[j]=error_message[i];
        video_momory_addr[j+1]= 0x4F;
        i++;
        j+=2;
    }
    // 既然发生除以零这种严重物理错误，我们就不让它往下跑了，直接把机器死锁在这里
    __asm__ __volatile__("cli; hlt");
}

void set_idt_gate(int num, uint32_t base, uint16_t sel,uint8_t flags) {
    idt[num].base_low=(base & 0xFFFF);
    idt[num].always0=0;
    idt[num].base_high=(base >> 16)& 0xFFFF;
    idt[num].flags=flags;
    idt[num].sel=sel;                    //在内核里面的段选择子选用0x08
}

void idt_install() {
    idtr.limit=(sizeof(struct idt_entry)*256) -1;
    idtr.base=(uint32_t)&idt;
    //将其中的内容全部初始化。
    for (int i = 0; i < 256; i++) {
        set_idt_gate(i, 0, 0, 0);
    }
    //于此处写下所有的IDT
    set_idt_gate(0, (uint32_t)isr0,0x08,0x8E);
    idt_flush((uint32_t)&idtr);
}