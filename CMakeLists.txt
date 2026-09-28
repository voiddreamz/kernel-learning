cmake_minimum_required(VERSION 3.20)
project(kernel C)

# ============================================================
# 本项目是 x86 实模式/保护模式的裸机内核，没有操作系统和 libc：
#   1. nasm    -> mbr.asm    (flat bin, 放到磁盘 LBA0)
#   2. nasm    -> *.asm       (elf32 目标文件, 自动收集)
#   3. gcc     -> *.c         (32 位 freestanding, 带 DWARF, 自动收集)
#   4. gcc/ld  -> system.elf (真正的可执行目标, 带符号和调试信息, 基址 0x900)
#   5. objcopy -> system.bin (由 ELF 剥出来的裸二进制，给 QEMU 启动用)
#   6. 拼镜像: mbr.bin@0x0000 + system.bin@0x0400(LBA2)
#   7. qemu-system-i386 启动
#
# 调试（断点 / 变量 / 内存查看）：见 README.md 的「在 CLion 中调试」
# ============================================================

# 给 CLion / clangd 生成 compile_commands.json，代码补全更准
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- 工具链检查 ----------
find_program(NASM_EXECUTABLE nasm REQUIRED)
find_program(DD_EXECUTABLE   dd   REQUIRED)
find_program(TRUNCATE_EXECUTABLE truncate)
find_program(QEMU_EXECUTABLE NAMES qemu-system-i386 qemu-system-x86_64)
if (CMAKE_OBJCOPY)
    set(OBJCOPY_EXECUTABLE ${CMAKE_OBJCOPY})
else ()
    find_program(OBJCOPY_EXECUTABLE NAMES objcopy i686-elf-objcopy REQUIRED)
endif ()

# ---------- 32 位 freestanding 编译选项 ----------
# 只作用于 C 代码（本项目没有宿主机可执行文件，所以全局加上是安全的）
add_compile_options(
        -m32                     # 32 位指令集，和 loader.asm 的 BITS 32 保持一致
        -ffreestanding           # 告诉编译器：没有标准库
        -fno-pie -fno-pic        # 不生成位置无关代码，链接地址是固定的 0x900
        -fno-stack-protector     # 没有 canary 的运行时支持
        -fno-builtin             # 不要把手写循环优化成 memcpy/strlen 这类外部调用
        -mno-sse -mno-mmx        # 内核里不能随便用 SSE/MMX（CR0/CR4 未开启）
        -fno-omit-frame-pointer  # 保留 ebp 栈帧，CLion/GDB 才能正确回溯调用栈
        -fno-asynchronous-unwind-tables # 裸机用不到 .eh_frame，去掉可减小镜像
        -Wall -Wextra
        -g                       # DWARF 调试信息：断点、变量、内存查看全靠它
)

# ---------- 源码自动收集 ----------
# 根目录下的 .c 全部参与编译；.h 放进 sources 只是为了让 CLion 把它们算进 target，
# 方便索引/跳转（CMake 不会编译头文件）。
# CONFIGURE_DEPENDS：每次构建时让 CMake 检查目录变化，有新增/删除文件就自动重新配置。
file(GLOB KERNEL_C_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 汇编：mbr.asm 要用 -f bin，loader.asm 必须排在链接顺序最前面（_start 必须落在 0x900），
# 这两个单独处理；其余 .asm（gdt_flush.asm，以后新增的中断桩等）自动按 elf32 编译。
file(GLOB KERNEL_ASM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)
list(REMOVE_ITEM KERNEL_ASM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/mbr.asm"
        "${CMAKE_CURRENT_SOURCE_DIR}/loader.asm"
)

# 产出目录统一放在 CLion 的构建目录里（源码树保持干净）
set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

# QEMU gdbserver 端口，调试时 GDB 连到这个端口
set(GDB_PORT 1234 CACHE STRING "QEMU gdbserver 监听端口")

# ============================================================
# 2. loader.asm -> loader.o（elf32，因为它 extern c_main）
#    -F dwarf -g：让汇编代码也有行号信息，可以进汇编单步调
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

# ============================================================
# 2b. 其余 .asm -> elf32 目标文件（gdt_flush.asm，以及以后新增的汇编）
# ============================================================
set(KERNEL_ASM_OBJS "")
foreach (asm_src IN LISTS KERNEL_ASM_SOURCES)
    get_filename_component(asm_name ${asm_src} NAME_WE)
    set(asm_obj ${OUT_DIR}/${asm_name}.o)
    add_custom_command(
            OUTPUT  ${asm_obj}
            COMMAND ${NASM_EXECUTABLE} -f elf32 -F dwarf -g
                    ${asm_src} -o ${asm_obj}
            DEPENDS ${asm_src}
            COMMENT "NASM [elf32] ${asm_name}.asm -> ${asm_name}.o"
            VERBATIM
    )
    list(APPEND KERNEL_ASM_OBJS ${asm_obj})
endforeach ()
add_custom_target(kernel_asm DEPENDS ${KERNEL_ASM_OBJS})

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
# 4. 编译 C 源文件 + 链接 loader.o/汇编 -> system.elf
#    C 源码直接作为 system 的源文件（不再用 OBJECT 库），这样 CLion 拿到的
#    编译上下文是完整的，头文件里的 #include 也能正确解析。
#    它是裸机 ELF（不能跑在宿主机上），但符号表和 QEMU 里的地址完全一致。
#    -nostdlib -static -no-pie : 不要 crt、不要标准库、不要 PIE
#    -Wl,-Ttext=0x900          : 必须和 mbr.asm 的 LOADER_BASE_ADDR 一致
#    顺序不能换：loader.o 在前，保证 _start 位于 0x900
# ============================================================
add_executable(system
        ${LOADER_OBJ}
        ${KERNEL_ASM_OBJS}
        ${KERNEL_C_SOURCES}
)
target_include_directories(system PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(system PROPERTIES
        OUTPUT_NAME system.elf
        LINKER_LANGUAGE C
        RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
)
foreach (cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(system PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} ${OUT_DIR})
endforeach ()
target_link_options(system PRIVATE
        -m32
        -nostdlib -static -no-pie
        -Wl,-z,noexecstack
        -Wl,--build-id=none      # 别让 build-id 之类的东西插到 .text 前面污染裸二进制
        -Wl,--no-eh-frame-hdr
        -Wl,-e,_start
        -Wl,-Ttext=0x900
        -Wl,-Map=${OUT_DIR}/system.map
)

# ============================================================
# 5. system.elf -> system.bin（启动用的裸二进制）
#    只保留 .text/.rodata/.data 这些要装进内存的段，
#    调试段和符号表不会混进镜像里
# ============================================================
set(SYSTEM_ELF ${OUT_DIR}/system.elf)
set(SYSTEM_BIN ${OUT_DIR}/system.bin)
add_custom_command(
        OUTPUT  ${SYSTEM_BIN}
        COMMAND ${OBJCOPY_EXECUTABLE} -O binary ${SYSTEM_ELF} ${SYSTEM_BIN}
        DEPENDS ${SYSTEM_ELF}
        COMMENT "OBJCOPY system.elf -> system.bin"
        VERBATIM
)

# ============================================================
# 6. 拼磁盘镜像
#    LBA0 (偏移 0x0000): mbr.bin          <- BIOS 从这里引导
#    LBA2 (偏移 0x0400): system.bin       <- mbr.asm 里 LOADER_START_ADDR equ 0x2
#    最后填充到 1MiB，避免 MBR 连续读 15 个扇区时越界
# ============================================================
set(OS_IMAGE ${OUT_DIR}/os.img)
add_custom_command(
        OUTPUT  ${OS_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${OS_IMAGE}
        COMMAND ${TRUNCATE_EXECUTABLE} -s 1M ${OS_IMAGE}
        COMMAND ${DD_EXECUTABLE} if=${MBR_BIN} of=${OS_IMAGE}
                bs=512 count=1 conv=notrunc status=none
        COMMAND ${DD_EXECUTABLE} if=${SYSTEM_BIN} of=${OS_IMAGE}
                bs=512 seek=2 conv=notrunc status=none
        DEPENDS ${MBR_BIN} ${SYSTEM_BIN}
        COMMENT "IMG  mbr.bin + system.bin -> os.img"
        VERBATIM
)

# 镜像单独出一个 target：QEMU 的启动脚本会先确保它是最新的
add_custom_target(image DEPENDS ${OS_IMAGE})

# ============================================================
# 7. 启动 QEMU
#    run  : 正常启动（终端/CLion 里跑这个 target 都行）
#    debug: 带着 gdbserver 启动并暂停在复位状态，等 GDB 连上 localhost:${GDB_PORT}
#           （CLion 里直接用自带的 Embedded GDB Server 配置，见 README.md）
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

    add_custom_target(debug
            COMMAND ${QEMU_EXECUTABLE}
                    -drive file=${OS_IMAGE},format=raw,if=ide
                    -boot c
                    -m 64M
                    -no-reboot      # 内核崩了（三重故障）就直接退出，不会被静默重启迷惑
                    -gdb tcp::${GDB_PORT}
                    -S              # 启动后先暂停，等 GDB attach
            DEPENDS ${OS_IMAGE}
            WORKING_DIRECTORY ${OUT_DIR}
            COMMENT "QEMU gdbstub localhost:${GDB_PORT}，符号文件 ${SYSTEM_ELF}"
            USES_TERMINAL
            VERBATIM
    )

    # ---------- 调试辅助脚本（生成到构建目录，终端里也能用） ----------
    set(QEMU_DEBUG_SH ${OUT_DIR}/qemu-debug.sh)
    file(WRITE ${QEMU_DEBUG_SH}
"#!/usr/bin/env bash
# 由 CMake 自动生成。这个脚本同时充当 CLion 的 GDB Server：
#   1. 先把 os.img（以及它依赖的 system.elf/system.bin）构建到最新，
#      这样即使 CLion 只构建了 target system，也不会拿旧镜像去启动；
#   2. 再启动 QEMU 并暂停在复位状态，等 GDB 连接 localhost:${GDB_PORT}。
set -e
\"${CMAKE_COMMAND}\" --build \"${OUT_DIR}\" --target image --parallel >&2
exec \"${QEMU_EXECUTABLE}\" \\
    -drive file=\"${OS_IMAGE}\",format=raw,if=ide \\
    -boot c -m 64M \\
    -gdb tcp::${GDB_PORT} -S \"\$@\"
")
    file(CHMOD ${QEMU_DEBUG_SH}
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    set(GDB_SCRIPT ${OUT_DIR}/kernel.gdb)
    file(WRITE ${GDB_SCRIPT}
"# 由 CMake 自动生成：终端 gdb 用法
#   gdb -x ${GDB_SCRIPT} ${SYSTEM_ELF}
set pagination off
set confirm off
set disassembly-flavor intel
target remote localhost:${GDB_PORT}
")
else ()
    message(WARNING "未找到 qemu-system-i386 / qemu-system-x86_64，run/debug 目标不可用（镜像仍可由 image 目标生成）")
endif ()

# 让 CLion 的 Build（Ctrl+F9）默认把镜像编译出来
add_custom_target(os ALL DEPENDS ${OS_IMAGE})
add_dependencies(os image)
