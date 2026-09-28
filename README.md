整个项目，mbr是随着BIOS启动后的首个引导程序。
然后loader.asm被它加载，然后loader.asm进入保护模式，此时再通过kernel.c加载手动编写的GDT。其中关于加载GDT的函数放在gdt.c

## 构建产物

CMake 会把所有产物放在 CLion 的构建目录（默认 `cmake-build-debug/`）里：

| 文件 | 用途 |
| --- | --- |
| `mbr.bin` | 第 0 扇区的引导程序 |
| `system.elf` | 带符号表和 DWARF 调试信息，**给 GDB 用**（`system` 目标的产物） |
| `system.bin` | 由 ELF 剥出来的裸二进制，QEMU 实际启动的内容 |
| `system.map` | 链接地址表，可以查每个符号落在哪个地址 |
| `os.img` | 拼好的硬盘镜像 |
| `qemu-debug.sh` / `kernel.gdb` | 自动生成的调试辅助脚本 |

## 新增源码

根目录下的 `*.c`、`*.h`、`*.asm` 都是自动收集的，新建文件不用改 `CMakeLists.txt`，也不用改 `compile.sh`：

- `*.c`：自动编译并参与链接
- `*.h`：自动算进 target，CLion 可以直接索引、跳转
- `*.asm`：自动按 elf32 编译（`mbr.asm`、`loader.asm` 除外：前者是 `-f bin`，后者固定在链接顺序最前面，保证 `_start` 落在 0x900）

CMake 用的是 `CONFIGURE_DEPENDS`：构建时会自己检查目录变化，发现新文件就自动重新配置，CLion 里点一下 Build 就能带上新代码。

> 小知识：头文件自己不构成编译单元，CLion 的分析引擎要靠「某个 .c include 了它」才有完整的编译上下文。
> 新建一个 `xxx.h` 之后，最好尽快在对应的 `xxx.c` 里 `#include "xxx.h"`（本项目已经把工程根目录加进 include 路径），
> 这样头文件里的类型、宏、`<stdint.h>` 之类才会正常高亮；否则它会作为"弱源文件"被分析，容易满屏红。

## 在 CLion 中调试

> **前置条件**：`Settings → Plugins` 里启用 **Embedded Development（嵌入式开发）** 插件。
> 如果它被禁用，下面的 `QEMU Debug` 配置会显示成"未知运行配置类型"。
>
> **另一个大坑**：CLion 会拿"当前选中的运行配置的目标"当作整个项目的解析上下文（resolve context）。
> 请让运行配置一直保持在 `QEMU Debug`（或 `system`）上；如果选中 `debug`、`run`、`mbr` 这类
> 只负责构建的自定义目标，引擎拿不到编译参数，头文件里的 `#include` 会全部变红。
> 切回 `QEMU Debug` / `system` 后等几秒就会恢复，和 CMake 配置无关。

项目里带了一个共享运行配置 **QEMU Debug**（`.idea/runConfigurations/QEMU_Debug.xml`）。
在右上角的运行配置里选中它，直接点 Debug（虫子图标）就好：

1. CLion 构建 `system` 目标，链接出带符号的 `system.elf`；
2. 然后用 `qemu-debug.sh` 启动 GDB Server：脚本会先把 `os.img` 构建到最新，
   再用 `-gdb tcp::1234 -S` 启动 QEMU 并暂停在复位状态等待 GDB；
3. CLion 自带 GDB 连上 `localhost:1234`，符号文件就是 `system.elf`。

调试时的小提示：

- **早期地址要用硬件断点**：MBR 会把 loader 拷到 `0x900`，普通软件断点会被这次加载覆盖。
  在 Debug 窗口的 GDB 控制台里输入 `hbreak c_main` 再 Continue 最稳妥；
  等跑过加载阶段之后，普通断点就和平时一样好使。
- Memory 视图填 `0xb8000` 看显存、填 `0x900` 看内核本体；变量、调用栈、反汇编都能直接用。
- `kernel.c` 里的 `int a = 1/0;` 会触发除零 → 没有 IDT 时就是三重故障（QEMU 会重启，
  调试会话还在，随时可以 Pause 下来看现场）。

> 如果 QEMU Debug 配置没有自动出现，可以在 `Run | Edit Configurations` 里手动新建一个
> **Embedded GDB Server**：GDB Server 选 `qemu-system-i386`，args 填
> `-drive file=<构建目录>/os.img,format=raw,if=ide -boot c -m 64M -gdb tcp::1234 -S`，
> `'target remote'` 填 `localhost:1234`，Target 选 `system`，Download 选 `None`。

CMake 目标：

- `os`：默认 Build（Ctrl+F9）就会生成镜像
- `system`：真正的可执行目标，产出 `system.elf`。CLion 会为它生成运行配置，
  别点 Run（裸机 ELF 不能在宿主机上跑），要调试就用上面的 QEMU Debug 配置
- `run`：正常启动 QEMU
- `debug`：启动 QEMU 并暂停，开好 gdbserver（`localhost:1234`）

终端方式（不打开 CLion）：

```sh
cmake --build cmake-build-debug --target image      # 或者 ./compile.sh build
cmake-build-debug/qemu-debug.sh                     # 启动 QEMU + gdbserver
# 另一个终端：
gdb -x cmake-build-debug/kernel.gdb cmake-build-debug/system.elf
```
