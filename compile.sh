#!/usr/bin/env bash
# ============================================================
# 裸机内核编译脚本
#   所有产物（.o / .bin / os.img / system.map）都放在 build/ 里，
#   源码树保持干净。
#
#   用法：
#     ./compile.sh          编译并启动 QEMU（默认，和以前一样）
#     ./compile.sh build    只编译，不启动 QEMU
#     ./compile.sh debug    编译后带着 gdbserver 启动 QEMU 并暂停，等 GDB 连接
#     ./compile.sh run      跳过编译，直接用已有镜像启动 QEMU
#     ./compile.sh clean    删除整个 build/ 目录
# ============================================================
set -euo pipefail

# 不管从哪里调用，都切到脚本所在目录（项目根目录）
cd "$(dirname "$0")"

BUILD_DIR=build

require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "错误：找不到 $1，请先安装。" >&2
        exit 1
    }
}

clean() {
    echo "清理 $BUILD_DIR/ ..."
    rm -rf "$BUILD_DIR"
}

build() {
    require nasm
    require gcc
    require ld
    require objcopy

    mkdir -p "$BUILD_DIR"

    # ==========================================
    # 1. 编译阶段
    # ==========================================

    echo "编译汇编跳板 (Loader)..."
    # elf32 + dwarf，方便 gdb 调试；loader.asm extern 了 C 函数所以不能用 bin
    nasm -f elf32 -F dwarf -g loader.asm -o "$BUILD_DIR/loader.o"

    # 其余 .asm 自动按 elf32 编译（gdt_flush.asm，以及以后新增的中断桩等），
    # 新增汇编文件不用改这个脚本
    ASM_OBJS=("$BUILD_DIR/loader.o")
    for asm in *.asm; do
        case "$asm" in
            mbr.asm | loader.asm) continue ;;
        esac
        obj="$BUILD_DIR/${asm%.asm}.o"
        echo "  汇编 $asm"
        nasm -f elf32 -F dwarf -g "$asm" -o "$obj"
        ASM_OBJS+=("$obj")
    done

    echo "编译 C 语言内核..."
    # -fno-stack-protector 防止 Linux 编译器强行插入栈保护代码导致裸机崩溃
    # -fno-builtin          防止编译器把手写循环优化成 memcpy/strlen 等外部调用
    # -mno-sse -mno-mmx     CR0/CR4 还没开启，内核里不能碰 SSE/MMX
    CFLAGS=(
        -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector
        -fno-builtin -mno-sse -mno-mmx -Wall -Wextra -g
        -fno-omit-frame-pointer          # 保留 ebp 栈帧，GDB 才能正确回溯调用栈
        -fno-asynchronous-unwind-tables  # 裸机用不到 .eh_frame，去掉减小镜像
    )
    # 根目录下所有 .c 自动参与编译，新增文件不用改这个脚本
    C_OBJS=()
    for src in *.c; do
        obj="$BUILD_DIR/${src%.c}.o"
        echo "  C $src"
        gcc "${CFLAGS[@]}" -c "$src" -o "$obj"
        C_OBJS+=("$obj")
    done

    # ==========================================
    # 2. 链接与缝合阶段
    # ==========================================
    echo "链接 Loader 和 Kernel..."
    # -Ttext 0x900 必须和 mbr.asm 里的 LOADER_BASE_ADDR 保持一致
    # 顺序不能换：loader.o 放最前面，保证 _start 正好落在 0x900
    # 输出 ELF（带符号表和调试信息）给 GDB 用，实际启动的二进制由它剥出来
    ld -m elf_i386 -z noexecstack -T linker.ld \
        -Map "$BUILD_DIR/system.map" \
        "${ASM_OBJS[@]}" \
        "${C_OBJS[@]}" \
        -o "$BUILD_DIR/system.elf"

    # 只保留 .text/.rodata/.data 这些要装进内存的段，调试段和符号表不进镜像
    objcopy -O binary "$BUILD_DIR/system.elf" "$BUILD_DIR/system.bin"

    KERNEL_SIZE=$(wc -c < ./build/system.bin)
    SECTORS=$(((KERNEL_SIZE+511)/512))
    #我们将mbr的部分拖到现在，因为我们的mbr需要计算system.bin的数据所占扇区大小。
    echo "编译 MBR..."
    nasm -f bin mbr.asm -D KERNEL_SECTORS=$SECTORS -o "$BUILD_DIR/mbr.bin"


    echo "制作硬盘镜像 ($BUILD_DIR/os.img)..."
    # 创建一个 1MB 的空白虚拟硬盘文件（用 0 填充）
    dd if=/dev/zero of="$BUILD_DIR/os.img" bs=512 count=2048 2>/dev/null
    # 把 MBR 写进 LBA0（最开头的 512 字节）
    dd if="$BUILD_DIR/mbr.bin" of="$BUILD_DIR/os.img" \
        bs=512 count=1 conv=notrunc 2>/dev/null
    # 把内核 (system.bin) 写进 LBA2（mbr.asm 里 LOADER_START_ADDR equ 0x2）
    dd if="$BUILD_DIR/system.bin" of="$BUILD_DIR/os.img" \
        bs=512 seek=2 conv=notrunc 2>/dev/null
}

check_image() {
    [ -f "$BUILD_DIR/os.img" ] || {
        echo "错误：$BUILD_DIR/os.img 不存在，先执行 ./compile.sh build" >&2
        exit 1
    }
    require qemu-system-x86_64
}

run_qemu() {
    check_image

    # ==========================================
    # 3. 虚拟机启动阶段
    # ==========================================
    echo "启动 QEMU..."
    qemu-system-x86_64 -drive format=raw,file="$BUILD_DIR/os.img" -m 512M -cpu qemu64,+svm
}

debug_qemu() {
    check_image
    require gdb

    # gdbserver 挂在 1234 端口，QEMU 启动后先暂停，等 GDB 连上再继续。
    # 终端可以这样调：
    #   gdb "$BUILD_DIR/system.elf" -ex "set disassembly-flavor intel" \
    #       -ex "target remote localhost:1234"
    echo "启动 QEMU（已暂停），gdbserver: localhost:1234"
    echo "符号文件: $BUILD_DIR/system.elf"
    qemu-system-x86_64 -drive format=raw,file="$BUILD_DIR/os.img" -m 512M -cpu qemu64,+svm \
        -gdb tcp::1234 -S -no-reboot
}

ACTION="${1:-all}"

case "$ACTION" in
    all | build)
        build
        [ "$ACTION" = build ] || run_qemu
        ;;
    debug)
        build
        debug_qemu
        ;;
    run)
        run_qemu
        ;;
    clean)
        clean
        ;;
    *)
        echo "用法: $0 [build|debug|run|clean]" >&2
        exit 1
        ;;
esac
