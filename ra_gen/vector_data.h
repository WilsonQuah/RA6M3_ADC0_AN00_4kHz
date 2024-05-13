/* generated vector header file - do not edit */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
        extern "C" {
        #endif
/* Number of interrupts allocated */
#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (2)
#endif
/* ISR prototypes */
void adc_scan_end_isr(void);
void gpt_counter_overflow_isr(void);

/* Vector table allocations */
#define VECTOR_NUMBER_ADC0_SCAN_END ((IRQn_Type) 0) /* ADC0 SCAN END (End of A/D scanning operation) */
#define ADC0_SCAN_END_IRQn          ((IRQn_Type) 0) /* ADC0 SCAN END (End of A/D scanning operation) */
#define VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW ((IRQn_Type) 1) /* GPT0 COUNTER OVERFLOW (Overflow) */
#define GPT0_COUNTER_OVERFLOW_IRQn          ((IRQn_Type) 1) /* GPT0 COUNTER OVERFLOW (Overflow) */
#ifdef __cplusplus
        }
        #endif
#endif /* VECTOR_DATA_H */
