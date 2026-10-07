//
// Created by voiddreamz on 2026/10/7.
//
#include "general.h"

static void cpuid(int32 id, int32 *eax, int *ebx, int32 *ecx, int32 *edx) {
    __asm__ __volatile__ ("cpuid": "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx): "a"(id));
}

int check_svm_support() {
    uint32_t eax, ebx, ecx, edx;
    // CPUID EAX=0 会在 ebx, edx, ecx 中返回厂商字符串
    cpuid(0, &eax, &ebx, &ecx, &edx);
    // "Auth" = 0x68747541, "enti" = 0x69746E65, "cAMD" = 0x444D4163
    if (ebx != 0x68747541 || ecx != 0x444D4163 || edx != 0x69746E65) {
        return 0;
    }
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    if (ecx & (1 << 2)) {
        return 1;
    }
    return 0;
}

static inline void rdmsr(int32 msr_id,int32 *low, int32 *hi) {
    __asm__ __volatile__("rdmsr" : "=a"(*low), "=d"(*hi) : "c"(msr_id));
}

static inline void wrmsr(int32 msr_id, int32 low, int32 hi) {
    __asm__ __volatile__("wrmsr" : : "a"(low), "d"(hi), "c"(msr_id));
}

// 开启 AMD SVM 引擎
void enable_svm() {
    uint32_t eax, edx;
    // EFER 寄存器的物理地址是 0xC0000080
    rdmsr(0xC0000080, &eax, &edx);

    // 将 EFER 的第 12 位 (SVME - Secure Virtual Machine Enable) 设为 1
    eax |= (1 << 12);

    wrmsr(0xC0000080, eax, edx);
}