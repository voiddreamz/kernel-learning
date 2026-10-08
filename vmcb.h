//
// Created by voiddreamz on 2026/10/7.
// 用来储存VMCB数据结构。其用来保存
//

#ifndef KERNEL_VMCB_H
#define KERNEL_VMCB_H

#include "general.h"

#pragma pack(push, 1) // 拒绝任何编译器自动对齐

// 虚拟机的段结构 (16 字节)
struct vmcb_seg {
    int16 selector;
    int16 attribute;
    int32 limit;
    int64 base;
};

// 1. 控制区 (准确占据 0x000 ~ 0x3FF，共 1024 字节)
struct vmcb_control_area {
    int16 intercept_cr_read;   // 0x00  CR0-CR15 读拦截
    int16 intercept_cr_write;  // 0x02  CR0-CR15 写拦截
    int16 intercept_dr_read;   // 0x04  DR0-DR15 读拦截
    int16 intercept_dr_write;  // 0x06  DR0-DR15 写拦截
    int32 intercept_exceptions;// 0x08  异常拦截
    int32 intercept_instr1;    // 0x0C (Bit 18=CPUID, Bit 24=HLT...)
    int32 intercept_instr2;    // 0x10 (Bit 0=VMRUN, Bit 1=VMMCALL)

    int8  pad1[68];            // 0x14 ~ 0x57（内含 pause 过滤、IOPM/MSRPM 指针等，本阶段用不到）
    int32 guest_asid;
    int8 pad2[20];
    int64 exitcode;            // 0x70 (极度重要：虚拟机死因)
    int64 exitinfo1;           // 0x78
    int64 exitinfo2;           // 0x80

    int8  pad3[888];           // 0x88 填补到 0x400，精确凑齐 1024 字节
};

// 2. 状态区 (准确占据 0x400 ~ 0xFFF，共 3072 字节)
struct vmcb_state_save_area {
    struct vmcb_seg es;           // 0x400
    struct vmcb_seg cs;           // 0x410
    struct vmcb_seg ss;           // 0x420
    struct vmcb_seg ds;           // 0x430
    struct vmcb_seg fs;           // 0x440
    struct vmcb_seg gs;           // 0x450
    struct vmcb_seg gdtr;         // 0x460
    struct vmcb_seg ldtr;         // 0x470
    struct vmcb_seg idtr;         // 0x480
    struct vmcb_seg tr;           // 0x490

    int8  pad3[43];            // 0x4A0 填补到 0x4CB
    int8  cpl;                 // 0x4CB
    int8  pad4[4];             // 0x4CC 填补到 0x4D0

    int64 efer;                // 0x4D0

    int8  pad5[112];           // 0x4D8 填补到 0x548

    int64 cr4;                 // 0x548
    int64 cr3;                 // 0x550
    int64 cr0;                 // 0x558
    int64 dr7;                 // 0x560
    int64 dr6;                 // 0x568
    int64 rflags;              // 0x570
    int64 rip;                 // 0x578 (虚拟机第一条指令位置)

    int8  pad6[88];            // 0x580 填补到 0x5D8

    int64 rsp;                 // 0x5D8
    int64 rax;                 // 0x5E0
    
    // 剩下的物理空间全部用空字节填满
    // 4096(0x1000) - 0x5E8 = 0xA18 = 2584 字节
    int8  pad_to_end[0xA18];
};

// 3. 完美缝合的 4KB VMCB
struct vmcb {
    struct vmcb_control_area control;
    struct vmcb_state_save_area state;
};

#pragma pack(pop)
#include <stddef.h>
_Static_assert(sizeof(struct vmcb) == 0x1000, "VMCB 必须 4KB");
_Static_assert(offsetof(struct vmcb_control_area, intercept_instr2) == 0x10, "");
_Static_assert(offsetof(struct vmcb_control_area, guest_asid)     == 0x58, "");
_Static_assert(offsetof(struct vmcb_control_area, exitcode)       == 0x70, "");
_Static_assert(offsetof(struct vmcb, state.cs)   == 0x410, "");
_Static_assert(offsetof(struct vmcb, state.rip)  == 0x578, "");
#endif //KERNEL_VMCB_H
