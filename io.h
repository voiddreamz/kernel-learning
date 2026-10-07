//
// Created by voiddreamz on 2026/9/29.
// 重映射io端口，手动写一个非内存映射端口的io读取与写入
//

#ifndef KERNEL_IO_H
#define KERNEL_IO_H
#include "general.h"

static inline void outb(int16 port,int8 val) {
    __asm__ __volatile__ ("outb %0,%1" : : "a"(val),"Nd"(port));
}

static inline int8 inb(int16 port) {
    int8 ret;
    __asm__ __volatile__ ("in %1,%0" :  "=a"(ret): "Nd"(port));
    return ret;
}
// 减速，防止老IO接口快速被写人出错
static  inline void io_wait(void) {
    outb(0x80,0);
}
#endif //KERNEL_IO_H
