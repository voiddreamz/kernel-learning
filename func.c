//
// Created by voiddreamz on 2026/9/29.
//
#include "general.h"
void print_memory(char* str,int line,int color) {
    char* _video_memory = (char *)0xb8000;
    char* message = str;
    int j=line * 160;
    int i=0;
    while (message[i]!='\0') {
        _video_memory[j] = message[i];
        _video_memory[j+1]=(uint8_t)color;
        i++;
        j+=2;
    }
};
