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
