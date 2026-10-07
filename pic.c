//
// Created by voiddreamz on 2026/9/29.
// 我们这个程序制作的目的：
// 8259相当于南桥，而我们要控制它的内容，我们要通过往ICW的端口写入数据来控制（就像一个gdt）
// 我们通过向0x20,0x21端口
//

#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC2_COMMAND 0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10 //代表开启重置
#define ICW1_ICW4 0x01 //它是1，代表icw开启icw4
#define ICW4_8086 0x01 //icw4的一个参数，代表架构为8086

void pic_remap(int offset1, int offset2) {
    int8 a1,a2;
    //backup
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    //发送初始化程序(ICW1)
    outb(PIC1_COMMAND, ICW1_INIT | ICW4_8086); //这里将两个or，代表开启icw4和开启重置
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW4_8086); //同上
    io_wait();
    // 发送icw2，指定中断起始偏移量。
    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);
    // 我们通过改变偏移量，让这些无所谓的中断不占用0-31的中断表
    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);
    io_wait();

    //发送icw3，里面描述的是硬件级联关系（PIC1告诉它IRQ2连接从芯片，PIC2告诉它级联表识是2）
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    //发送ICW4，指定架构为8086
    outb(PIC1_DATA,  ICW4_8086);
    outb(PIC2_DATA,  ICW4_8086);

    //恢复备份
    outb(PIC1_DATA,  a1);
    outb(PIC2_DATA,  a2);
}

//屏蔽所有的中断

void pic_disable() {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}