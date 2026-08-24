// hal/mem/hal_mem.c
#include "hal/hal_mem.h"
#include "hal/cpu_features.h"

// --- Implementaciones candidatas -------------------------------------------
static void *memcpy_baseline(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

static void *memcpy_sse2(void *dst, const void *src, size_t n) {
    // Copia en bloques de 16 bytes con movaps cuando está alineado;
    // cae a memcpy_baseline para el remanente y para bloques no alineados
    // pequeños. Implementación de ejemplo — sustituye por tu rutina SSE2
    // ya probada si el proyecto tiene una en lib/.
    uint8_t *d = dst; const uint8_t *s = src;
    size_t blocks = n / 16;
    if (((uintptr_t)d % 16 == 0) && ((uintptr_t)s % 16 == 0)) {
        for (size_t i = 0; i < blocks; i++) {
            __asm__ volatile(
                "movaps (%0), %%xmm0\n"
                "movaps %%xmm0, (%1)\n"
                :: "r"(s + i*16), "r"(d + i*16) : "xmm0", "memory");
        }
        return memcpy_baseline(d + blocks*16, s + blocks*16, n - blocks*16);
    }
    return memcpy_baseline(dst, src, n); // no alineado: no vale la pena la complejidad de manejar el prólogo
}

static void *memset_baseline(void *dst, int val, size_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)val;
    return dst;
}

// Puntero de función global, resuelto una vez.
void *(*hal_memcpy)(void *dst, const void *src, size_t n) = memcpy_baseline;
void *(*hal_memset)(void *dst, int val, size_t n)         = memset_baseline;

void hal_mem_init(void) {
    cpu_features_t feat;
    cpu_features_detect(&feat);

    // Selección en orden de preferencia descendente. AVX2 quedaría como
    // siguiente escalón natural (32 bytes por iteración con ymm) — lo
    // dejo fuera del código de ejemplo para no inflarlo, pero el patrón
    // es idéntico: añadir memcpy_avx2() y anteponerla aquí.
    if (feat.has_sse2) {
        hal_memcpy = memcpy_sse2;
    } else {
        hal_memcpy = memcpy_baseline;
    }
    hal_memset = memset_baseline; // añade memset_sse2 con el mismo patrón si te hace falta

    // Registro para diagnóstico en boot — esto es lo que hace la
    // adaptabilidad *observable*, no solo interna:
    kprintf("[HAL] CPU: %s family=%u model=%u | SSE2=%d AVX=%d AVX2=%d AVX512F=%d NX=%d 1GB_pages=%d\n",
        feat.vendor, feat.family, feat.model,
        feat.has_sse2, feat.has_avx, feat.has_avx2, feat.has_avx512f,
        feat.has_nx, feat.has_1gb_pages);
}