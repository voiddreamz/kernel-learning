# kernel.md — 从零开始做一个冷切换 Hypervisor

## 这个项目是什么
一个 Type-1 hypervisor（跑在硬件之上、所有操作系统之下的第一层软件），
它的设计目标和现有所有 hypervisor（Xen / KVM / VirtualBox）都不同：

- 同一时刻，**只有一个子内核在裸跑硬件**（独占 CPU、GPU、PCI、内存）
- 不在运行的子内核，**物理 RAM 上一个字节都不占**——它的状态全部在磁盘快照里
- 切换 = 保存当前子内核的完整状态到磁盘 → reset 所有硬件 → 加载另一个子内核的快照
- hypervisor 自己的 RAM 占用固定 ≤ 32MB，永不释放

目前**没有任何现成开源项目**做这件事。你是在造轮子。
最接近的参考是 Jailhouse（MIT 协议，极简），但 Jailhouse 是"多 cell 并行跑不同 CPU 核"，
而我们的是"单 cell 互斥跑同一个硬件集"。

---

## 为什么它和 Xen/KVM 本质不同

| 维度           | Xen / KVM                                         | 这个项目                                                                                   |
| -------------- | ------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| 有几个内核活着 | 所有 guest 同时在 RAM，hypervisor 调度轮流跑      | **只有 1 个**活着跑硬件，其他只在磁盘里                                                    |
| RAM 占用       | 每个 guest 预分配的 RAM 永久占着，paused 也不释放 | 不跑的子内核占 0 RAM，运行的子内核独占 100% 物理 RAM（除了 hypervisor 自己的 32MB 保留区） |
| 核心机制       | VM entry/exit 分时共享                            | snapshot + reset + chainboot 状态切换                                                      |
| 复杂度         | 极高（设备模拟 + grant table + event channel）    | 极低（无设备模拟、无调度器、无跨子内核通信）                                               |

---

## 时间线和学习顺序（从 0 开始，诚实估计）

下面的时间是**每天 2-3 小时学习 + 动手**的估计。
如果你每天能投入更多，可以快一些；如果很少接触底层，可能更久。

### Phase -1：补基础（**6-8 周，先把这个过完再动任何代码**）

你需要同时学三样东西，按这个顺序来（前一个是后一个的前提）：

#### Step A：x86 汇编（2-3 周）
**你需要能看懂/写出**：基本寄存器操作、栈、函数调用（cdecl 约定）、跳转、简单 IO 端口读写。

**免费资源（按顺序）**：
1. 《Programming from the Ground Up》by Jeff Duntemann（免费合法 PDF）——从变量/流程控制讲起，用 Linux `nasm` + `ld` 跑汇编程序，到函数调用、指针、字符串处理为止。**做到 Chapter 8 就够了**，后面的 ELF 格式和高级技巧可以以后再看。
2. NASM 官方手册前 3 章（nasm.us/doc/nasmdoc0.html）——查指令语法。
3. OSDev Wiki "Bare Bones" 教程的 ASM 部分（wiki.osdev.org/Bare_bones）——把它的 boot.asm 自己敲一遍，理解 multiboot2 header 是什么、为什么要在文件最前面。

**动手验证**：
- 写一个汇编程序，在屏幕上打印 "Hello from asm"，然后循环。用 `nasm -f elf64 hello.asm -o hello.o && ld hello.o -o hello && ./hello` 跑起来。
- 写一个汇编程序，用 `out` 指令往 COM1 串口发送字符。用 `qemu-system-x86_64 -kernel hello -serial stdio` 验证串口有输出。
- 能在笔记里画出 `push` / `pop` 后栈指针的移动。

#### Step B：CPU 架构基础（2-3 周）
**你需要能解释**：
- 实模式（Real Mode）、保护模式（Protected Mode）、IA-32e / 长模式（Long Mode）三者的区别和切换条件
- CR0 / CR3 / CR4 / EFER 这四个控制寄存器每个位大概干嘛的
- 段寄存器（CS/DS/ES/SS/FS/GS）是什么，段选择子和段描述符的关系
- GDT / IDT 是什么、干嘛用的
- 物理内存地址、虚拟地址、页表（PML4 → PDPT → PD → PT）怎么把虚拟地址翻译成物理地址

**免费资源**：
1. Intel SDM Vol 1 Chapter 3（Basic Execution Environment）+ Chapter 4（Instruction Set Summary）——只看概念，不用死背指令。尤其是 3.3 节（Modes of Operation）和 3.4 节（Registers）。
2. Intel SDM Vol 3A Chapter 5（Memory Management）+ Chapter 6（Interrupt and Exception Handling）——GDT/IDT/页表的完整定义。
3. OSDev Wiki "CPU" 入口页（wiki.osdev.org/CPU_Registers_x86）——快速速查表，配合上面 SDM 一起看。
4. 《Operating Systems: Three Easy Pieces》免费在线版（pages.cs.wisc.edu/~remzi/OSTEP/），Chapter 13-18（内存虚拟化）和 Chapter 4（中断）——从操作系统视角理解 MMU 和中断，比纯硬件文档好懂。

**动手验证**：
- 能在笔记里画出：用户程序里 `mov rax, [0x12345678]` 时，页表怎么一步步把 0x12345678 翻译成物理地址。
- 能解释：为什么 Linux 内核启动时要先关掉分页（`mov cr0` 关掉 PG 位），设好 GDT，再打开分页？
- 能说清楚：CR4.PAE 位和 EFER.LM 位分别控制什么？长模式下必须先开哪个？

#### Step C：内核启动和最小内核（2-3 周）
**你需要能解释**：
- BIOS → MBR → bootloader（GRUB）→ multiboot2 → 你的内核 这条启动链路
- bootloader 给你的内核传了什么信息（multiboot2 info structure）
- 你的内核代码怎么从汇编入口跳到 C 入口（`kmain()`）
- 怎么在 C 语言里写一个最小的 IDT 和页表

**免费资源**：
1. OSDev Wiki "Bare Bones" 整个系列（wiki.osdev.org/Bare_bones）——**把它的代码全部自己敲一遍，一行一行理解**。它教你：boot.asm 怎么从实模式切到 64 位长模式、怎么写 kmain()、怎么设置页表、怎么让串口打印。
2. 视频教程（可选，辅助理解）：Stephen Smith 的 YouTube 系列 "Writing an OS in Rust" 或 Nick Blundell 的 "How Does a Computer Work?"——看概念，不要管具体语言。
3. Linux 内核源码 `arch/x86/boot/` 目录（仅看 `boot.S` 和 `compressed/head.S` 的开头 ~100 行）——理解 Linux 自己是怎么从实模式切到长模式的。

**动手验证（里程碑）**：
你必须独立写出以下内容（不能抄，能解释每一行）：
- 一个 boot.S：从 multiboot2 header 开始，自己设好临时页表，切到长模式，跳转到 kmain()
- 一个 kmain()：初始化 UART COM1，在 QEMU 串口打印 "Hello from my tiny kernel"，然后 hlt 死循环
- 一个 linker.ld：把你的内核放到物理地址 0x100000（1MB）以上，设好栈
- 一个 Makefile：`make` 能编出一个可以用 QEMU `-kernel` 直接加载的 elf 文件

**到这里你才算有资格碰 hypervisor 代码**。别跳。

---

### Phase 0：VT-x 宿主最小化（1-2 周）

**目标**：让你的 tiny kernel 执行 VMXON 成功、VMXOFF 成功，串口打印成功标志。不加载子内核。

**你需要先理解**（Phase -1 之后）：
- VT-x root mode / non-root mode 的区别（Intel SDM Vol 3C Chapter 23.1）
- VMXON region 必须 4KB 对齐、里面要先写 IA32_VMX_BASIC MSR 的 revision ID
- VMXON 指令的失败标志怎么查（CF 和 ZF，SDM Vol 3C Table 23-5）

**免费资源**：
1. Intel SDM Vol 3C Chapter 23.1 ~ 23.3（VT-x 基本概念 + VMXON/VMXOFF）
2. Jailhouse `hypervisor/vmx.c` 的 `vmx_enable()` 和 `vmx_disable()` 函数（各十几行）
3. 你的机器上 `rdmsr 0x480` 输出的是什么（IA32_VMX_BASIC MSR）

**动手任务**：
- 在你 Phase -1 的 tiny kernel 里加 `vmx_enable.S` 和 `vmx_disable.S` 两个汇编文件
- vmx_enable.S 里：检查 CPUID(1).ECX 有没有 VMX 支持、解锁 CR4 的 VMXE 位、分配 4KB 对齐的 VMXON region、读 revision ID 写进去、执行 VMXON、检查成功
- vmx_disable.S 里：VMXOFF 指令
- kmain() 里：初始化串口 → 打印 "KSwitch booted" → 调 vmx_enable() → 打印成功/失败 → 调 vmx_disable() → 打印 → hlt

**验证**：
```bash
qemu-system-x86_64 -enable-kvm -cpu max -smp 1 -m 512M -serial stdio -nographic -kernel build/ks.elf
```
串口依次打印：bootloader 信息 → "KSwitch booted" → "VMXON success" → "VMXOFF success"。

**必踩的坑**（自己查原因，不要直接问）：
- VMXON region 没 4KB 对齐 → VMXON 失败 CF=1
- 没写 revision ID → VMXON 失败 CF=1
- CR4.VMXE 没开 → VMXON 触发 #GP

---

### Phase 1：VMLAUNCH 进入子内核（2-3 周）

**目标**：你的 hypervisor 通过 VMLAUNCH 进入子内核 A（A 只打印 "Hello from A" 然后 hlt），VM exit handler 能抓到这个 hlt。

**你需要先理解**：
- VMCS 是什么（SDM Vol 3C Chapter 23.6 + Table 23-7/23-8）
- VMCS 的 Guest State 区 / Host State 区 / Execution Controls 区分别存什么
- VMLAUNCH（第一次进子内核）和 VMRESUME（从子内核切回来）的区别
- VM exit 是什么触发的（SDM Vol 3C Table 23-23，exit code 0 就是 hlt）

**动手任务**：
- 写一个极简子内核 A：同样 multiboot2 启动，只做串口打印 → 设一个全局标志位 → hlt
- 分配 4KB 对齐的 VMCS region，VMCLEAR → VMPTRLD
- 配置 VMCS 的三个区域
- 写 vmx_vmlaunch.S / vmx_vmresume.S / vmx_vmexit_handler.S
- 写最小 VM exit handler：读 VM_EXIT_REASON，判断是不是 hlt，打印信息

**验证**：串口依次打印 hypervisor 启动信息 → "Hello from A" → "VM exit reason: HLT"。

**必踩的坑**：
- Primary Processor-Based Controls 填了不允许的位 → VMRESUME #GP。解决：先读 IA32_VMX_PROCBASED_CTLS_MIN/MAX MSR，min 里的位必须设 1，max 里的位不能设 1。
- Guest CR0/CR4 没满足 IA32_VMX_CR0_FIXED0/1 约束 → VMLAUNCH 失败。

---

### Phase 2：热键切换（1-2 周）

**目标**：Ctrl+Alt+F1 / Ctrl+Alt+F2 切换子内核 A 和 B。

**你需要先理解**：
- VM exit handler 怎么区分不同的 exit reason（热键会触发 keyboard interrupt，对应 exit code 大概是外部中断）
- PS/2 键盘控制器的 8042 端口（OSDev Wiki "Keyboard" 页）

**动手任务**：
- 写一个极简子内核 B（和 A 对称，打印 "Hello from B"）
- 写最小 PS/2 键盘驱动：读 scancode → 检测组合键
- 状态机：`enum { KS_BOOTING, KS_RUNNING_A, KS_RUNNING_B, KS_SWITCHING }`
- 切换流程：VM exit → 换 VMCS → VMLAUNCH/VMRESUME 进入 B

**验证**：按热键切换 A → B → A，串口输出正确跟随变化。

---

### Phase 3：Snapshot / Restore（3-4 周）

**目标**：切走子内核时保存它的完整状态（寄存器 + 物理页）到磁盘快照，切回来时恢复到原来的执行位置。

**你需要先理解**：
- Linux hibernation 怎么 dump 所有物理页（kernel/power/snapshot.c，~3000 行，GPL 可读）
- 怎么用 EPT 遍历子内核的物理页（或者 Phase 3 还没 EPT 就用简单假设）
- AHCI/SATA 控制器怎么读写磁盘（OSDev Wiki "ATA"）

**动手任务**：
- 定义 `.ksnap` 快照文件格式（自己设计头文件，包含：magic、寄存器区、页表区、物理页数据）
- 实现 snapshot 采集：从 VMCS 读所有 Guest State 区的寄存器（vmread 指令）+ 遍历物理页
- 实现快照存储：先写 AHCI/SATA 最小磁盘驱动，支持读/写 512 字节扇区
- 实现 restore：读快照 → 恢复 VMCS Guest State → 恢复物理页 → VMRESUME

**验证**：子内核 A 里加一个递增计数器，切走 → 切到 B → 切回 A → 计数器应该接着上次的值继续加。

---

### Phase 4：PCI reset + 真实 Linux 加载（4-6 周）

**目标**：切走子内核时 reset 所有 PCI 设备，加载真实的 Linux bzImage 当子内核 A。

**你需要先理解**：
- PCI 配置空间怎么读（CF8/CFC 端口，OSDev Wiki "PCI"）
- bzImage 格式（bootsect.S + setup.S + 内核主体 + struct boot_params）
- Linux 内核怎么接收 bootloader 传的参数（arch/x86/include/uapi/asm/bootparam.h）

**动手任务**：
- PCI 遍历 + reset：CF8/CFC 循环枚举所有 bus/device/function，逐个 reset
- Linux bzImage 加载器：解压内核（如果压缩了）、组装 struct boot_params、设内核命令行
- 把子内核 A 换成编译好的 Linux bzImage（先在 QEMU 上联调）

---

## 工具清单

Phase -1 就能装好的东西：
```bash
sudo apt install qemu-system-x86 gcc-multilib nasm ld make gdb msr-tools
```

QEMU 启动参数（后面每个 Phase 都要用到）：
```bash
# Phase -1 到 Phase 1：不带 KVM 的 QEMU
qemu-system-x86_64 -cpu max -smp 1 -m 512M -serial stdio -nographic -kernel build/ks.elf

# Phase 2+：带 KVM 加速的 QEMU
qemu-system-x86_64 -enable-kvm -cpu host -smp 2 -m 2G -serial stdio -nographic -kernel build/ks.elf

# 带 GDB 远程调试：
qemu-system-x86_64 -enable-kvm -cpu host -smp 1 -m 512M -serial stdio -nographic -s -S -kernel build/ks.elf
# 然后另开一个终端：gdb build/ks.elf → target remote :1234 → continue
```

---

## 免费资源总表（全部合法，免费）

| 资源                                         | 链接                                                                    | 什么时候用                                     |
| -------------------------------------------- | ----------------------------------------------------------------------- | ---------------------------------------------- |
| Programming from the Ground Up               | 免费合法 PDF（搜就能找到）                                              | Phase -1 Step A                                |
| OSDev Wiki                                   | wiki.osdev.org                                                          | 整个项目的速查手册，遇到任何概念先搜这里       |
| Intel SDM Vol 1 / 3A / 3C                    | intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html | Phase -1 Step B + Phase 0/1                    |
| Jailhouse 源码                               | github.com/siemens/jailhouse                                            | Phase 1 起，直接读 hypervisor/ 目录            |
| Linux 内核源码                               | kernel.org                                                              | Phase 3/4，hibernation 和 PCI reset 参考       |
| Linux hibernation 文档                       | Documentation/power/snapshot.rst                                        | Phase 3                                        |
| OSTEP (Operating Systems: Three Easy Pieces) | pages.cs.wisc.edu/~remzi/OSTEP/                                         | Phase -1 Step B，理解 MMU 和中断的操作系统视角 |

---

## 诚实的时间预估

| Phase                        | 时间（每天 2-3h） | 能不能跳                                             |
| ---------------------------- | ----------------- | ---------------------------------------------------- |
| Phase -1 Step A（ASM）       | 2-3 周            | **绝对不能**——不认识 mov/push/pop 就看不懂 boot.S    |
| Phase -1 Step B（CPU 架构）  | 2-3 周            | **绝对不能**——不知道长模式怎么开就写不了任何内核代码 |
| Phase -1 Step C（最小内核）  | 2-3 周            | **绝对不能**——你的 hypervisor 本身就是个内核         |
| Phase 0（VMXON/VMXOFF）      | 1-2 周            | 可以稍快，但每步要验证                               |
| Phase 1（VMLAUNCH）          | 2-3 周            | 取决于你对 VMCS 的理解深度                           |
| Phase 2（热键切换）          | 1-2 周            | OK                                                   |
| Phase 3（Snapshot）          | 3-4 周            | 磁盘驱动 + 快照格式需要反复调                        |
| Phase 4（PCI reset + Linux） | 4-6 周            | OK                                                   |

**总计**：从 0 到能切换真实 Linux 和另一个内核，大约 **6-8 个月**（每天 2-3h）。
如果投入更多时间可以压缩。但 Phase -1 那至少 6 周是省不掉的——
因为你现在的状态相当于"想学写 WebApp 但不会写 HTML"。

---

## 里程碑检查点

每个 Phase 结束时你必须能独立回答这些问题，才能进入下一个 Phase：

**Phase -1 结束时**：
- [ ] 能在 QEMU 上跑自己写的汇编程序，串口能打印
- [ ] 能解释 CR0/CR4/EFER 怎么把 CPU 从实模式切到长模式
- [ ] 能画出一个虚拟地址怎么通过页表翻译成物理地址
- [ ] 能独立写出能在 QEMU 上跑的 tiny kernel（自己写 boot.S + kmain() + linker.ld）

**Phase 0 结束时**：
- [ ] 你的 tiny kernel 能执行 VMXON 成功
- [ ] 能解释 VMXON 为什么要先写 IA32_VMX_BASIC 的 revision ID
- [ ] 能说清楚 VMXON 失败时 CF 和 ZF 分别表示什么

**Phase 1 结束时**：
- [ ] 能独立配置 VMCS 的三个区域
- [ ] 能跑 VMLAUNCH 进入子内核
- [ ] 能抓到 VM exit 并读 VM_EXIT_REASON 判断退出原因

**Phase 2 结束时**：
- [ ] 能写 PS/2 键盘驱动读 scancode
- [ ] 能按热键在两个子内核之间切换

**Phase 3 结束时**：
- [ ] 能 dump 和 restore 子内核的完整状态
- [ ] 切回来时子内核能从上次的执行位置继续跑
