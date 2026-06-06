#pragma once
#include <stdint.h>   // uint64_t — safe with bare-metal toolchain
#include <stdio.h>

// ── Read RISC-V hardware cycle counter ──
// rdcycle reads a 64-bit hardware counter that increments every CPU cycle
// This is a real RISC-V instruction — works on QEMU and real hardware
// No time.h needed — no include conflicts!
static inline uint64_t get_cycles() {
    uint64_t c;
    asm volatile("rdcycle %0" : "=r"(c));
    return c;
}

// Print cycle count for a stage
static inline void print_cycles(const char* label,
                                  uint64_t start, uint64_t end) {
    printf("  %-22s %llu cycles\n",
           label, (unsigned long long)(end - start));
}
