;本文件同时处理CPU异常和IO接口异常

extern fault_handler ;ISR的统一的错误处理器
extern irq_handler   ;IO接口
;这一部分我们在做CPU内部异常处理
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
    mov ax,ds
    push eax
    ;然后进入我们的平坦模型的第三段，即是data段
    ;为什么我们这里要切换到data段：因为现在的程序都在ring0里面运行没有问题。
    ;但是如果我们现在是一个ring3的程序出问题要调用中断了，CPU调用它的时候会自动把cs切换成ring0（0x08)
    ;但是我们此时es是一个诸如0x23的数字，它的权限是不够的
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax

    push esp
    call fault_handler
    add esp, 4  ;关于这里，我们之前push了esp，栈

    pop eax
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax

    popa
    add esp, 8
    sti
    iret

;定义IRQ宏

%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push byte 0 ;由于它不提供错误码，我们在此直接填充为0
        push byte %2
        jmp irq_common_stub
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47


irq_common_stub:
    pusha

    mov ax,ds
    push eax

    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax

    push esp
    call irq_handler
    add esp,4

    pop eax
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax

    popa
    add esp,8
    sti
    iret
