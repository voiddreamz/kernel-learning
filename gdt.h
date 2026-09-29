//
// Created by voiddreamz on 2026/9/28.
//

#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H
#include "type.h"
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
}__attribute__((packed));
#endif //KERNEL_GDT_H

