;gdt_flush.asm

global gdt_flush

gdt_flush:
    mov eax,[esp+4] ; 这里向esp传入的是一个指针，一个32位的指针，占据4个byte，所以我们+4
    lgdt [eax]

    ;接下来就是刷新的操作：用0x10给我们的所有的寄存器赋值
    mov ax, 0x10
    mov bx,ax
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    mov ss,ax

    jmp 0x08:.flush
.flush:
    ret ; return 回到调用前的内容，即是说我们的loader.c