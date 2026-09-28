cmake_minimum_required(VERSION 3.20)
project(kernel C)

# ============================================================
# 本项目是 x86 实模式/保护模式的裸机内核，没有操作系统和 libc：
#   1. nasm  -> mbr.asm    (flat bin, 放到磁盘 LBA0)
#   2. nasm  -> loader.asm (elf32 目标文件, 带 extern c_main)
#   3. gcc   -> loader.c   (32 位 freestanding, 导出 c_main)
#   4. ld    -> system.bin (-Ttext 0x900, --oformat binary, 裸二进制)
#   5. 拼镜像: mbr.bin@0x0000 + system.bin@0x0400(LBA2)
#   6. qemu-system-i386 启动
# ============================================================

# ---------- 工具链检查 ----------
find_program(NASM_EXECUTABLE nasm REQUIRED)
find_program(LD_EXECUTABLE   ld   REQUIRED)
find_program(DD_EXECUTABLE   dd   REQUIRED)
find_program(TRUNCATE_EXECUTABLE truncate)
find_program(QEMU_EXECUTABLE NAMES qemu-system-i386)

# ---------- 32 位 freestanding 编译选项 ----------
# 只作用于 C 代码（本项目没有宿主机可执行文件，所以全局加上是安全的）
add_compile_options(
        -m32                     # 32 位指令集，和 loader.asm 的 BITS 32 保持一致
        -ffreestanding           # 告诉编译器：没有标准库
        -fno-pie -fno-pic        # 不生成位置无关代码，链接地址是固定的 0x900
        -fno-stack-protector     # 没有 canary 的运行时支持
        -fno-builtin             # 不要把手写循环优化成 memcpy/strlen 这类外部调用
        -mno-sse -mno-mmx        # 内核里不能随便用 SSE/MMX（CR0/CR4 未开启）
        -Wall -Wextra
        -g                       # 带调试信息，CLion 可以配合 gdb 使用
)

# C 源文件，后续新增内核 .c 文件时往这里加即可
set(KERNEL_C_SOURCES
        loader.c
        gdt.c
)

# 产出目录统一放在 CLion 的构建目录里（源码树保持干净）
set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ============================================================
# 1. loader.c -> 32 位目标文件（C 语言部分）
#    用 OBJECT 库，CLion 才能把 loader.c 识别进项目并给出代码提示
# ============================================================
add_library(c_kernel OBJECT ${KERNEL_C_SOURCES})

# ============================================================
# 2. loader.asm -> loader.o（elf32，因为它 extern c_main）
# ============================================================
set(LOADER_OBJ ${OUT_DIR}/loader.o)
add_custom_command(
        OUTPUT  ${LOADER_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f elf32 -F dwarf -g
                ${CMAKE_CURRENT_SOURCE_DIR}/loader.asm -o ${LOADER_OBJ}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/loader.asm
        COMMENT "NASM [elf32] loader.asm -> loader.o"
        VERBATIM
)
add_custom_target(loader DEPENDS ${LOADER_OBJ})
add_dependencies(loader c_kernel)

# ============================================================
# 3. mbr.asm -> mbr.bin（flat 二进制，末尾 0xaa55）
# ============================================================
set(MBR_BIN ${OUT_DIR}/mbr.bin)
add_custom_command(
        OUTPUT  ${MBR_BIN}
        COMMAND ${NASM_EXECUTABLE} -f bin
                ${CMAKE_CURRENT_SOURCE_DIR}/mbr.asm -o ${MBR_BIN}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mbr.asm
        COMMENT "NASM [bin] mbr.asm -> mbr.bin"
        VERBATIM
)
add_custom_target(mbr DEPENDS ${MBR_BIN})

# ============================================================
# 4. 链接 loader.o + C 对象文件 -> system.bin
#    -Ttext 0x900   : loader 被 MBR 加载到 0x900，代码基址必须一致
#    --oformat binary: 去掉 ELF 头，直接吐出裸机器码
#    顺序不能换：loader.o 在前，保证 _start 位于 0x900
# ============================================================
set(SYSTEM_BIN ${OUT_DIR}/system.bin)
add_custom_command(
        OUTPUT  ${SYSTEM_BIN}
        COMMAND ${LD_EXECUTABLE} -m elf_i386 -z noexecstack
                -Ttext 0x900
                --oformat binary
                -Map ${OUT_DIR}/system.map
                -o ${SYSTEM_BIN}
                ${LOADER_OBJ}
                $<TARGET_OBJECTS:c_kernel>
        DEPENDS ${LOADER_OBJ} $<TARGET_OBJECTS:c_kernel>
        COMMENT "LD  loader.o + c_kernel -> system.bin"
        VERBATIM
)
add_custom_target(system DEPENDS ${SYSTEM_BIN})
add_dependencies(system loader)

# ============================================================
# 5. 拼磁盘镜像
#    LBA0 (偏移 0x0000): mbr.bin          <- BIOS 从这里引导
#    LBA2 (偏移 0x0400): system.bin       <- mbr.asm 里 LOADER_START_ADDR equ 0x2
#    最后填充到 1MiB，避免 MBR 连续读 15 个扇区时越界
# ============================================================
set(OS_IMAGE ${OUT_DIR}/os.img)
set(IMAGE_PARTS ${MBR_BIN} ${SYSTEM_BIN})
add_custom_command(
        OUTPUT  ${OS_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${OS_IMAGE}
        COMMAND ${TRUNCATE_EXECUTABLE} -s 1M ${OS_IMAGE}
        COMMAND ${DD_EXECUTABLE} if=${MBR_BIN} of=${OS_IMAGE}
                bs=512 count=1 conv=notrunc status=none
        COMMAND ${DD_EXECUTABLE} if=${SYSTEM_BIN} of=${OS_IMAGE}
                bs=512 seek=2 conv=notrunc status=none
        DEPENDS ${IMAGE_PARTS}
        COMMENT "IMG  mbr.bin + system.bin -> os.img"
        VERBATIM
)
add_custom_target(image DEPENDS ${OS_IMAGE})

# ============================================================
# 6. 启动 QEMU
#    在 CLion 里把构建目标切成 run（或新建 Custom Build Target 指向 run）
# ============================================================
if (QEMU_EXECUTABLE)
    add_custom_target(run
            COMMAND ${QEMU_EXECUTABLE}
                    -drive file=${OS_IMAGE},format=raw,if=ide
                    -boot c
                    -m 64M
            DEPENDS ${OS_IMAGE}
            WORKING_DIRECTORY ${OUT_DIR}
            COMMENT "QEMU ${OS_IMAGE}"
            USES_TERMINAL
            VERBATIM
    )
else ()
    message(WARNING "未找到 qemu-system-i386，run 目标不可用（镜像仍可由 image 目标生成）")
endif ()

# 让 CLion 的 Build（Ctrl+F9）默认把镜像编译出来
add_custom_target(os ALL DEPENDS ${OS_IMAGE})
