;idt_flush.asm

global idt_flush

idt_flush:
    mov eax,[esp+4]
    lidt [eax]
    ;it not of necesserity to refresh register
    ret