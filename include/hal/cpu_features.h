// include/hal/cpu_features.h
#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <stdint.h>

typedef struct {
    char     vendor[13];
    uint32_t family, model, stepping;

    // Flags relevantes para decisiones de dispatch, no un espejo 1:1 de CPUID
    uint8_t has_sse2;
    uint8_t has_sse4_2;
    uint8_t has_avx;
    uint8_t has_avx2;
    uint8_t has_avx512f;
    uint8_t has_rdrand;
    uint8_t has_rdseed;
    uint8_t has_nx;        // execute-disable — condiciona cómo se marcan páginas en VMM
    uint8_t has_1gb_pages; // condiciona granularidad de paginación
    uint8_t has_tsc_invariant; // condiciona si el TSC sirve como reloj fiable entre cores
    uint32_t logical_cpus;
} cpu_features_t;

void cpu_features_detect(cpu_features_t *out);

#endif // CPU_FEATURES_H