global guest_payload_start

guest_payload_start:
    ; 虚拟机启动后，立刻往自己的寄存器里塞个显眼的值
    mov eax, 0xDEADBEEF
    mov ebx, 0x1337

    ; 主动敲门求救，这会触发咱们在控制区布置的拦截点，导致 VMEXIT
    vmmcall

    ; 如果宿主机把它放回来了，就死循环，防止它乱跑导致真机崩溃
    jmp $