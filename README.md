整个项目，mbr是随着BIOS启动后的首个引导程序。
然后loader.asm被它加载，然后loader.asm进入保护模式，此时再通过loader.c加载手动编写的GDT。其中关于加载GDT的函数放在gdt.c
