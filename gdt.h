//
// Created by voiddreamz on 2026/9/28.
//

#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H
#include "general.h"
struct gdt_entry {
    int16 limit_low;
    int16 base_low;
    int8 base_middle;
    int8 access;
    int8 granularity;
    int8 base_high;
} __attribute__((packed));

struct gdt_ptr {
    int16 limit;
    int64 base;
}__attribute__((packed));
#endif //KERNEL_GDT_H
