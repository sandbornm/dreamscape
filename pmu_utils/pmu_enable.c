#include <stdio.h>
#include <stdint.h>

#define PMCR_E          (1 << 0)
#define PMCR_C          (1 << 2)
#define PMCR_X          (1 << 4)
#define PMCR_DP         (1 << 5)
#define PMCR_XP         (1 << 6)

//volatile uint32_t *pmu;
//volatile uint32_t *pmcntenset;
//volatile uint32_t *pmxevtyper;
//volatile uint32_t *pmxevcntr;
//volatile uint32_t *pmcntenset;

void pmu_enable() {
    uint32_t pmcr;
    asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r" (pmcr));
    pmcr |= PMCR_E | PMCR_C | PMCR_X | PMCR_DP | PMCR_XP;
    asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r" (pmcr));
}

void pmu_disable() {
    uint32_t pmcr;
    asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r" (pmcr));
    pmcr &= ~PMCR_E;
    asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r" (pmcr));
}

int main() {
    pmu_enable();
    pmu_disable();

    return 0;
}
