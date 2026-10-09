//
// Created by voiddreamz on 2026/10/7.
//

#include "vmcb.h"

#define SEG_CODE_RW 0x0C9B
#define SEG_DATA_RW 0x0C93

extern int32 pmm_malloc_page();
extern void guest_payload_start();

static inline void vmrun_trigger(struct vmcb* my_vmcb) {
    __asm__ __volatile__(
        "vmrun"
        :
        : "a"((uint32_t)my_vmcb)
        : "ebx", "ecx", "edx", "esi", "edi", "ebp","memory", "cc"
    );
}

void init_vmcb_seg(struct vmcb_seg *vmcb_seg,int16 select,int16 attribute) {
    vmcb_seg->selector = select;
    vmcb_seg->base = 0;
    vmcb_seg->limit = 0xFFFFFFFF;
    vmcb_seg->attribute = attribute;
}

void start_vm() {
    struct vmcb* my_vmcb=(struct vmcb*)pmm_malloc_page();
    int8 *ptr=(uint8_t*)my_vmcb;
    for (int i = 0; i < 4096; i++) {
        ptr[i]=0;
    }
    init_vmcb_seg(&my_vmcb->state.cs,0x08,SEG_CODE_RW);
    init_vmcb_seg(&my_vmcb->state.ds,0x10,SEG_DATA_RW);
    init_vmcb_seg(&my_vmcb->state.es,0x10,SEG_DATA_RW);
    init_vmcb_seg(&my_vmcb->state.fs,0x10,SEG_DATA_RW);
    init_vmcb_seg(&my_vmcb->state.gs,0x10,SEG_DATA_RW);
    init_vmcb_seg(&my_vmcb->state.ss,0x10,SEG_DATA_RW);
    my_vmcb->state.efer=(1<<12);
    my_vmcb->state.cr0=0x00000011;
    //加载第一个程序
    my_vmcb->state.rip=(int64)(int32)&guest_payload_start;
    my_vmcb->control.guest_asid = 1;
    //调节控制区
    // 拦截 VMMCALL 指令
    my_vmcb->control.intercept_instr2 |= (1 << 0) |  (1 << 1);
    print_memory("VMRUN",10,14);
    my_vmcb->state.rax=0;
    int h=0;
    while (1) {
        vmrun_trigger(my_vmcb);
        int64 exitcode = my_vmcb->control.exitcode;
        if (exitcode == 0x81) {
            // 0x81 是 AMD 官方手册规定的 VMMCALL 退出码
            print_memory("SUCCESS! VM exited via VMMCALL.",11, 10); // 亮绿色
            my_vmcb->state.rip += 3; // 跳过vmmcall这个指令的机器码，而它的机器码是3位的
            print_memory("Resuming Guest...", 12+h,11); // 浅蓝色，表示我们把它放回去了
            h++;
        } else if (exitcode == -1) {
            // -1 (0xFFFFFFFFFFFFFFFF) 是 VMEXIT_INVALID，代表你填的 VMCB 格式有错
            print_memory("FATAL: VMCB state is invalid!",13, 12); // 亮红色
            break;
        } else {
            // 其他未知原因（比如缺页、非法指令）
            print_memory("VM exited for other reasons.", 12,12);
            break;
        }
    }
}