extern void gdt_install();
extern void idt_install();
void c_main() {
    //在此处定义一个指针，它指向我们的文本显示的部分。
    gdt_install();
    idt_install();
    char* _video_memory = (char *)0xb8000;
    char* message ="OS is booting...";
    int i=0,j =0;
    while (message[i]!='\0') {
        _video_memory[j] = message[i];
        _video_memory[j+1] = 0x0A;
        i++;
        j+=2;
    }
    // volatile int a = 10;
    // volatile int b = 0;
    // int c = a / b;

    while (1);
}