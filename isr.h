//
// Created by voiddreamz on 2026/9/28.
// it is used for interruput segment table.
//

#ifndef KERNEL_ISR_H
#define KERNEL_ISR_H

#include "type.h"
// 顺序需要和我们汇编的压栈的顺序完全一致
struct  regesters {
    int32 ds; //最后压入的
    int32 edi,esi,ebp,esp,ebx,edx,ecx,eax;
    int32 int_no,err_code;
    int32 eip,cs,eflags,useresp,ss;
}__attribute__((packed));

#endif //KERNEL_ISR_H
