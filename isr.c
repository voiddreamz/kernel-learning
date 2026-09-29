//
// Created by voiddreamz on 2026/9/28.
//

#include "isr.h"

char* exception_message[]={"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"};


void print_string(char* str,int line) {
    char *video_memory=(char *)0xb8000;
    int i = 0;
    int j = line * 160; // 每行160个字符
    while (str[i]!= '\0') {
        video_memory[j] = str[i];
        video_memory[j+1] = 0x4F;
        i++;
        j+=2;
    }
}

extern void fault_handle(struct regesters *r) {
    if (r->int_no<32) {
        print_string(" >> SYSTEM KERNEL PANIC <<",4);
        print_string(exception_message[r->int_no],5);
    }
}