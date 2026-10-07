#include "general.h"
extern void gdt_install();
extern void idt_install();
extern void pic_remap(int offset1, int offset2);
extern void pmm_init();
extern void page_init();
extern int check_svm_support();
void c_main() {
    //在此处定义一个指针，它指向我们的文本显示的部分。
    gdt_install();
    idt_install();
    pic_remap(0x20, 0x28);
    pmm_init();
    char* message ="OS is booting...";
    print_memory(message,0,0x0A);
    page_init();
    volatile int a = 10;
    volatile int b = 0;
    // int c = a / b;
    __asm__ __volatile__ ("sti");
    print_memory(message,0,0x0A);
    if (check_svm_support()) {
        print_memory(">>> AMD SVM (AMD-V) SUPPORTED BY CPU! <<<", 5,0x0A);
    } else {
        print_memory("AMD SVM NOT SUPPORTED OR DISABLED!", 5,0x0A);
    }
    while (1);
}