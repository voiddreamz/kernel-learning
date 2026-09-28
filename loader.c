extern void gdt_install();
void c_main() {
    //在此处定义一个指针，它指向我们的文本显示的部分。
    gdt_install();

    char* _video_memory = (char *)0xb8000;
    char* message ="Now GDT in C has been installed.";
    int i=0,j =0;
    while (message[i]!='\0') {
        _video_memory[j] = message[i];
        _video_memory[j+1] = 0x0A;
        i++;
        j+=2;
    }
    while (1);
}