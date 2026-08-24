// hal/cpu/cpuid.c
#include "hal/cpu_features.h"
#include <string.h>

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(subleaf));
}

static uint8_t xgetbv_osxsave_ok(void) {
    // AVX requiere no solo que la CPU lo soporte, sino que el SO haya
    // habilitado OSXSAVE (CR4.OSXSAVE) y que XCR0 tenga los bits de
    // estado YMM guardados. Sin esto, ejecutar una instrucción AVX
    // produce #UD aunque CPUID diga que el hardware lo soporta — es el
    // error más común al "detectar" AVX de forma incompleta.
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    if (!(cr4 & (1 << 18))) return 0; // OSXSAVE no habilitado por el kernel todavía

    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return (eax & 0x6) == 0x6; // bits 1 (XMM) y 2 (YMM) del XCR0
}

void cpu_features_detect(cpu_features_t *out) {
    memset(out, 0, sizeof(*out));
    uint32_t a, b, c, d;

    // --- Vendor string y leaf máximo soportado ---
    cpuid(0, 0, &a, &b, &c, &d);
    uint32_t max_leaf = a;
    memcpy(&out->vendor[0], &b, 4);
    memcpy(&out->vendor[4], &d, 4);
    memcpy(&out->vendor[8], &c, 4);
    out->vendor[12] = '\0';

    // --- Familia/modelo/stepping (leaf 1) ---
    cpuid(1, 0, &a, &b, &c, &d);
    out->stepping = a & 0xF;
    out->model    = (a >> 4) & 0xF;
    out->family   = (a >> 8) & 0xF;
    if (out->family == 0xF) out->family += (a >> 20) & 0xFF;
    if (out->family == 0x6 || out->family == 0xF) out->model |= ((a >> 16) & 0xF) << 4;

    out->has_sse2    = (d >> 26) & 1;
    out->has_sse4_2  = (c >> 20) & 1;
    uint8_t cpu_says_avx = (c >> 28) & 1;
    out->has_rdrand  = (c >> 30) & 1;

    out->has_avx = cpu_says_avx && xgetbv_osxsave_ok();

    // --- Leaf extendido 7 (AVX2, AVX-512, RDSEED) ---
    if (max_leaf >= 7) {
        cpuid(7, 0, &a, &b, &c, &d);
        out->has_avx2    = out->has_avx && ((b >> 5) & 1);
        out->has_avx512f = out->has_avx && ((b >> 16) & 1);
        out->has_rdseed  = (b >> 18) & 1;
    }

    // --- Leaf extendido 0x80000001 (NX/XD, páginas de 1GB) ---
    cpuid(0x80000000, 0, &a, &b, &c, &d);
    uint32_t max_ext_leaf = a;
    if (max_ext_leaf >= 0x80000001) {
        cpuid(0x80000001, 0, &a, &b, &c, &d);
        out->has_nx        = (d >> 20) & 1;
        out->has_1gb_pages = (d >> 26) & 1;
    }

    // --- TSC invariante (leaf 0x80000007) — crítico para usar TSC como
    //     reloj del scheduler en SMP; si no está presente, el TSC puede
    //     desincronizarse entre cores o con cambios de P-state. ---
    if (max_ext_leaf >= 0x80000007) {
        cpuid(0x80000007, 0, &a, &b, &c, &d);
        out->has_tsc_invariant = (d >> 8) & 1;
    }

    // logical_cpus se completa después con el parseo de tablas ACPI
    // (MADT), no con CPUID leaf 1 EBX[23:16] que en varios vendors
    // reporta un valor no fiable por sí solo.
    out->logical_cpus = 1; // placeholder hasta integrar el parser MADT
}