global isr0 ;
extern isr0_handle ;外部调用时写真的逻辑
extern fault_handle ;统一的错误处理器

;针对没有错误码的报错，我们手动push一个填位的错误码0进去
%macro ISR_NOERRORCODE 1
global isr%1
isr%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

;针对有错误码的报错
%macro ISR_ERRORCODE 1
global isr%1
isr%1:
    cli
    push byte %1
    jmp isr_common_stub
%endmacro

ISR_NOERRORCODE 0
ISR_NOERRORCODE 1
ISR_NOERRORCODE 2
ISR_NOERRORCODE 3
ISR_NOERRORCODE 4
ISR_NOERRORCODE 5
ISR_NOERRORCODE 6
ISR_NOERRORCODE 7
ISR_ERRORCODE 8
ISR_NOERRORCODE 9
ISR_ERRORCODE 10
ISR_ERRORCODE 11
ISR_ERRORCODE 12
ISR_ERRORCODE 13
ISR_ERRORCODE 14

ISR_NOERRORCODE 15
ISR_NOERRORCODE 16
ISR_ERRORCODE 17
ISR_NOERRORCODE 18
ISR_NOERRORCODE 19
ISR_NOERRORCODE 20
ISR_NOERRORCODE 21
ISR_NOERRORCODE 22
ISR_NOERRORCODE 23
ISR_NOERRORCODE 24
ISR_NOERRORCODE 25
ISR_NOERRORCODE 26
ISR_NOERRORCODE 27
ISR_NOERRORCODE 28
ISR_NOERRORCODE 29
ISR_NOERRORCODE 30
ISR_NOERRORCODE 31

isr_common_stub:
    ;压栈保护现场
    pusha
    ;压栈这个不会处理段处理器。
    ;这里把它放到eax里面是为了让它对其到32位（4byte）
    mov ax,es
    push eax
    ;然后进入我们的平坦模型的第三段，即是data段
    ;为什么我们这里要切换到data段：因为现在都在ring0里面运行没有问题。
    ;但是如果我们现在是一个ring3的程序出问题要调用中断了，CPU调用它的时候会自动把cs切换成ring0（0x08)
    ;但是我们此时es是一个诸如0x23的数字，它的权限是不够的
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax

    push esp
    call fault_handle

    pop eax
    mov ds,ax

    mov es,ax
    mov fs,ax
    mov gs,ax

    popa
    add esp, 8
    sti
    iret
    
; isr0:
;     ;先关闭中断，
;     cli
;     ; 将所有的通用寄存器压栈
;     pusha
;
;     ; 出现问题时调用handle这个函数
;     call isr0_handle
;
;     ; 解决问题后再pop出来
;     popa
;     ;开中断
;     sti
;
;     ;我们要专门写一个asm的程序的主要原因在此
;     iret
