//
// Created by voiddreamz on 2026/10/7.
//
#include "general.h"

static void cpuid(int32 id, int32 *eax, int *ebx, int32 *ecx, int32 *edx) {
    __asm__ __volatile__ ("cpuid": "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx): "a"(id));
}
int check_vmx_support() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (ecx & 1 << 5) {
        return 1;
    }
    return 0;
}