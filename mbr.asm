; 写一个mbr，加载loader到0x900
LOADER_BASE_ADDR equ 0x900 ; 我们想把loader程序放到的位置
LOADER_START_ADDR equ 0x2 ; 表示以LBA的形式储存

SECTION MBR vstart=0x7c00
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov gs,ax
    ; 这一段内容并非此程序的主要内容，而是在做一个打印，强调我们现在运行在MBR。
    ;调用0x06的功能，调用10号中断
    ;AH=0x06
    ;AL=0 表示全部要清除
    ;BH= 上卷行的属性
    ;(CL,CH)=x,y(左上角)
    ;(DL,DH)=x,y(右下角)
    mov ax,0600h
    mov bx,0700h
    mov cx,0
    mov dx,184fh  ;(80,25)相当于设定分辨率
    int 10h
    ; 输出一下当前在MBR
    mov byte [gs:0x00], 'M'
    mov byte [gs:0x01], 0xA4
    mov byte [gs:0x02], 'B'
    mov byte [gs:0x03], 0xA4
    mov byte [gs:0x04], 'R'
    mov byte [gs:0x05], 0xA4
    ;此处开始调用磁盘读写。
    mov eax, LOADER_START_ADDR ;LBA方式读入的扇区
    mov bx,LOADER_BASE_ADDR    ;写入的地址
    mov cx, 15                  ;读入的扇区数
    call rd_disk
    jmp LOADER_BASE_ADDR       ;跳到实际内存

rd_disk:
    ;目前，eax放的是LBA的扇区号
    ;bx 数据写入的地址
    ;cx 读入的扇区数
    mov esi,eax ;备份
    mov di,cx ;备份cx

    ;0x1f2 读写硬盘寄存器:表示要读取的扇区数
    mov dx,0x1f2 ;设置dx为端口号
    mov al,cl ;令al为扇区数
    out dx, al ;将al这个数据写入dx这个端口
    mov eax, esi ; 恢复备份
    ;0x1f3 表示LBA读取地址，低位8位（0-7位）
    mov dx,0x1f3
    out dx,al
    ;0x1f4 表示LBA读取地址，高位8位（8-15位）
    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al
    ;0x1f5 表示LBA读取地址，23-16位（16-23位）
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    ;0x1f6 表示LBA读取地址，24-23位（24-27位）
    shr eax,cl
    and al,0b00001111 ;0000 1111 保留低4位，高4位清0（早期LBA的设计规范，只能保留低4位，高四位见下行）
    or al,0b11100000  ;设置7-4位为1110，此时才是lBA模式
    mov dx,0x1f6
    out dx,al

    ;从这里开始将向0x1f7写入「读取」命令
    mov dx, 0x1f7
    mov al, 0x20 ;20h 表示读取
    out dx, al

    ;在这里进行了一次重构。之前是一次直接读取了15个扇区，但是这和DRQ的逻辑相悖
    ;DRQ每次读取一`个扇区后就会直接变成0，在进行下一次读取后再亮起1
    ; 自行检查硬盘状态：硬盘是「一个扇区」拉一次 DRQ，所以每个扇区都要重新等一次。所以我们需要一个计数器
    xor si, si        ; si = 已读完的扇区计数 = int i=0 in CPP.
.next_sector:               ;for(@i !=di ){}
    cmp si, di
    je .done

    .not_ready:
        nop
        in al,dx                ; dx 仍然是 0x1f7
        test al,0b10000000      ; BSY=1 时 DRQ 位不可信，继续等
        jnz .not_ready
        test al,0b00001000      ; DRQ=1 表示本扇区数据已就绪
        jz .not_ready

    mov cx, 256     ;一个扇区是512个byte，而一个1f0的接口吐出的是一个Word=2 byte，所以我们需要每个扇区读512/2= 256次。
    mov dx,0x1f0 ;实际数据读取端口

    .go_on:
        in ax, dx                ;将dx的数据写入ax。
        mov [bx], ax             ;将ax的数据写入bx所在的地址
        add bx, 2                ;一个ax读取两个byte，而每个内存地址只能放下1个byte的数据，所以添加两个。
        loop .go_on              ;cx不是一个简单的计数器，每次执行完loop会自动减1，当减到0时，loop自动停止

    inc si          ; 相当于si++，扇区读完，回去等下一个扇区的 DRQ
    mov dx, 0x1f7
    jmp .next_sector

    .done:
        ret
times 510 - ($-$$)	db 0
dw 0xaa55
