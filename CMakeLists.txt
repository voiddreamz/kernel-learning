cmake_minimum_required(VERSION 3.20)
project(kernel C)

# ============================================================
# 本项目是 x86 实模式/保护模式的裸机内核，没有操作系统和 libc：
#   1. nasm    -> *.asm       (elf32 目标文件, 自动收集, loader.asm 排链接顺序最前)
#   2. gcc     -> *.c         (32 位 freestanding, 带 DWARF, 自动收集)
#   3. gcc/ld  -> system.elf (真正的可执行目标, 带符号和调试信息, 基址 0x900)
#   4. objcopy -> system.bin (由 ELF 剥出来的裸二进制，给 QEMU 启动用)
#   5. nasm    -> mbr.bin    (flat bin, 放到磁盘 LBA0；
#                              KERNEL_SECTORS 由 system.bin 大小算出，所以必须后编译)
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
# 优先 qemu-system-i386：guest 是 32 位内核，i386 目标的 GDB 寄存器包正好匹配
# GDB 从 system.elf 推断出的 i386 架构；qemu-system-x86_64 会发 64 位寄存器包，
# GDB 报 "Remote 'g' packet reply is too long (expected 312, got 608)"。
# x86_64 仅作兜底（真用到时需要在 GDB 里 set architecture i386:x86-64）。
find_program(QEMU_EXECUTABLE NAMES qemu-system-i386 qemu-system-x86_64)
if (CMAKE_OBJCOPY)
    set(OBJCOPY_EXECUTABLE ${CMAKE_OBJCOPY})
else ()
    find_program(OBJCOPY_EXECUTABLE NAMES objcopy i686-elf-objcopy REQUIRED)
endif ()

# ---------- 汇编语言（nasm）----------
# 汇编统一交给 CMake 的 ASM_NASM 语言处理，等价于 nasm -f elf32 -F dwarf -g。
# 注意：OBJECT_FORMAT 必须在 enable_language 之前设置，否则 x86_64 上默认生成 elf64。
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
set(CMAKE_ASM_NASM_FLAGS "-F dwarf -g")
enable_language(ASM_NASM)

# ---------- 32 位 freestanding 编译选项（只给 C 用）----------
# 用 $<COMPILE_LANGUAGE:C> 包起来：汇编走 nasm，不认识 -m32 这类 gcc 选项。
add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:-m32>    # 32 位指令集，和 loader.asm 的 BITS 32 保持一致
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>           # 告诉编译器：没有标准库
        $<$<COMPILE_LANGUAGE:C>:-fno-pie> $<$<COMPILE_LANGUAGE:C>:-fno-pic>  # 不生成位置无关代码，链接地址固定 0x900
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>     # 没有 canary 的运行时支持
        $<$<COMPILE_LANGUAGE:C>:-fno-builtin>             # 不要把手写循环优化成 memcpy/strlen 这类外部调用
        $<$<COMPILE_LANGUAGE:C>:-mno-sse> $<$<COMPILE_LANGUAGE:C>:-mno-mmx>  # 内核里不能随便用 SSE/MMX（CR0/CR4 未开启）
        $<$<COMPILE_LANGUAGE:C>:-fno-omit-frame-pointer>  # 保留 ebp 栈帧，CLion/GDB 才能正确回溯调用栈
        $<$<COMPILE_LANGUAGE:C>:-fno-asynchronous-unwind-tables> # 裸机用不到 .eh_frame，去掉可减小镜像
        $<$<COMPILE_LANGUAGE:C>:-Wall> $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-g>       # DWARF 调试信息：断点、变量、内存查看全靠它
)

# ============================================================
# 2. 源文件自动收集
#    - *.c / *.h：根目录下全部参与编译；.h 放进 sources 只是为了让 CLion
#      把它们算进 target，方便索引/跳转（CMake 不会编译头文件）。
#    - *.asm：全部按 elf32 汇编（mbr.asm 除外，它要用 -f bin）。
#      loader.asm 必须排在列表最前：CMake 按源码列表顺序把目标文件传给
#      链接器，这样 _start 才能正好落在 0x900。
#    CONFIGURE_DEPENDS：每次构建时让 CMake 检查目录变化，有新增/删除文件
#    就自动重新配置，新增源码不用改这个文件。
# ============================================================
file(GLOB KERNEL_C_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
file(GLOB KERNEL_ASM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)
list(REMOVE_ITEM KERNEL_ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/mbr.asm")
list(REMOVE_ITEM KERNEL_ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/loader.asm")
list(PREPEND KERNEL_ASM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/loader.asm")

# 产出目录统一放在 CLion 的构建目录里（源码树保持干净）
set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

# QEMU gdbserver 端口，调试时 GDB 连到这个端口
set(GDB_PORT 1234 CACHE STRING "QEMU gdbserver 监听端口")

# ---------- QEMU 运行参数（所有启动入口共用，别各写各的）----------
# +svm 必须保留：否则 CPUID 不会报告 AMD-V，内核 check_svm_support() 会判为不支持
set(QEMU_CPU "qemu64,+svm" CACHE STRING "QEMU CPU 模型（+svm 供内核检测 AMD-V）")
# 内存必须和 pmm.c 的 MEMORY_MB (512) 以及 compile.sh 的 -m 一致
set(QEMU_MEMORY "512M" CACHE STRING "QEMU 虚拟机内存（需与 pmm.c 的 MEMORY_MB 一致）")

# ============================================================
# 3. 汇编 + C -> system.elf
#    源码直接作为 system 的源文件（不用 OBJECT 库），这样 CLion 拿到的
#    编译上下文是完整的，头文件里的 #include 也能正确解析。
#    它是裸机 ELF（不能跑在宿主机上），但符号表和 QEMU 里的地址完全一致。
#    -nostdlib -static -no-pie : 不要 crt、不要标准库、不要 PIE
#    -Wl,-T,linker.ld          : 用项目自带链接脚本，基址 0x900（必须和 mbr.asm 的
#                                LOADER_BASE_ADDR 一致），pmm.c 用的 _kernel_end
#                                也由它定义
#    顺序不能换：KERNEL_ASM_SOURCES 里 loader.asm 在最前，保证 _start 位于 0x900
# ============================================================
add_executable(system
        ${KERNEL_ASM_SOURCES}
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
        -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
        -Wl,-Map=${OUT_DIR}/system.map
)
# 链接脚本改了也要重新链接（否则 CMake 认为目标已是最新，不会重新执行 ld）
set_target_properties(system PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

# ============================================================
# 4. system.elf -> system.bin（启动用的裸二进制）
#    只保留 .text/.rodata/.data 这些要装进内存的段，
#    调试段和符号表不会混进镜像里
# ============================================================
set(SYSTEM_ELF ${OUT_DIR}/system.elf)
set(SYSTEM_BIN ${OUT_DIR}/system.bin)
add_custom_command(
        OUTPUT  ${SYSTEM_BIN}
        COMMAND ${OBJCOPY_EXECUTABLE} -O binary ${SYSTEM_ELF} ${SYSTEM_BIN}
        # DEPENDS 必须写目标名 system：写成 ${SYSTEM_ELF} 文件路径时 CMake 不会
        # 接上跨目标依赖，单独构建 image/mbr 会报 No rule to make target 'system.elf'。
        DEPENDS system
        COMMENT "OBJCOPY system.elf -> system.bin"
        VERBATIM
)

# ============================================================
# 5. system.bin + mbr.asm -> mbr.bin（flat 二进制，末尾 0xaa55）
#    mbr.asm 需要 KERNEL_SECTORS（内核占用的扇区数），所以必须等
#    system.bin 生成之后再编译：扇区数 = ceil(system.bin 字节数 / 512)。
#    构建时用一个小脚本量出真实大小，再把 -DKERNEL_SECTORS 传给 nasm，
#    这样内核变大后不用改任何文件，重新构建即可。
# ============================================================
set(MBR_BUILD_SCRIPT ${OUT_DIR}/build_mbr.cmake)
file(WRITE ${MBR_BUILD_SCRIPT} [=[
# 由 CMakeLists.txt 自动生成：
# 量出 system.bin 的大小，算出它占用的扇区数，作为 KERNEL_SECTORS 传给 mbr.asm。
if(NOT NASM OR NOT MBR_ASM OR NOT MBR_BIN OR NOT SYSTEM_BIN)
    message(FATAL_ERROR "build_mbr.cmake: 缺少 -D 参数（NASM/MBR_ASM/MBR_BIN/SYSTEM_BIN）")
endif()

file(SIZE "${SYSTEM_BIN}" KERNEL_SIZE)
math(EXPR KERNEL_SECTORS "(${KERNEL_SIZE} + 511) / 512")
message(STATUS "system.bin 共 ${KERNEL_SIZE} 字节 -> KERNEL_SECTORS=${KERNEL_SECTORS}")

execute_process(
        COMMAND "${NASM}" -f bin "${MBR_ASM}"
                -DKERNEL_SECTORS=${KERNEL_SECTORS} -o "${MBR_BIN}"
        RESULT_VARIABLE nasm_result
)
if(NOT nasm_result EQUAL 0)
    message(FATAL_ERROR "nasm 编译 mbr.asm 失败（退出码 ${nasm_result}）")
endif()
]=])

set(MBR_BIN ${OUT_DIR}/mbr.bin)
add_custom_command(
        OUTPUT  ${MBR_BIN}
        COMMAND ${CMAKE_COMMAND}
                -DNASM=${NASM_EXECUTABLE}
                -DMBR_ASM=${CMAKE_CURRENT_SOURCE_DIR}/mbr.asm
                -DMBR_BIN=${MBR_BIN}
                -DSYSTEM_BIN=${SYSTEM_BIN}
                -P ${MBR_BUILD_SCRIPT}
        DEPENDS ${SYSTEM_BIN} ${CMAKE_CURRENT_SOURCE_DIR}/mbr.asm ${MBR_BUILD_SCRIPT}
        COMMENT "NASM [bin] mbr.asm (KERNEL_SECTORS 由 system.bin 大小决定) -> mbr.bin"
        VERBATIM
)
add_custom_target(mbr DEPENDS ${MBR_BIN})

# ============================================================
# 6. 拼磁盘镜像
#    LBA0 (偏移 0x0000): mbr.bin          <- BIOS 从这里引导
#    LBA2 (偏移 0x0400): system.bin       <- mbr.asm 里 LOADER_START_ADDR equ 0x2
#    最后填充到 1MiB，保证镜像足够大，内核扇区数变化时也不会越界
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
                    -m ${QEMU_MEMORY}
                    -cpu ${QEMU_CPU}
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
                    -m ${QEMU_MEMORY}
                    -cpu ${QEMU_CPU}
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
    -boot c -m ${QEMU_MEMORY} -cpu ${QEMU_CPU} \\
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
    message(WARNING "未找到 qemu-system-x86_64 / qemu-system-i386，run/debug 目标不可用（镜像仍可由 image 目标生成）")
endif ()

# 让 CLion 的 Build（Ctrl+F9）默认把镜像编译出来
add_custom_target(os ALL DEPENDS ${OS_IMAGE})
add_dependencies(os image)
