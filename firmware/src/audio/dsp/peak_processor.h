#ifndef PEAK_PROCESSOR_H
#define PEAK_PROCESSOR_H

#include "peak_validator.h"         
#include "circular_block_buffer.h" 
#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_WINDOW_LEN 32000

typedef void (*PeakProcessFn)(const float *window, int32_t window_start_idx, int32_t window_len);

typedef struct {
    float pre_ratio;
    int32_t pre_min_samples;
    int32_t pre_max_samples;
} PeakProcessorConfig;

typedef struct {
    RTPeakMessage previous_s1_event;
    bool has_previous_s1;
    float window[MAX_WINDOW_LEN];
    int32_t window_len;
    PeakProcessFn process_fn;
    PeakProcessorConfig config;
} PeakProcessor;

void peak_processor_init(PeakProcessor *proc, const PeakProcessorConfig *conf, PeakProcessFn fn);

// Process a single peak message 
void peak_processor_process_peak(PeakProcessor *proc, const RTPeakMessage *peak_message, const CircularBlockBuffer *slab_buffer);

#endif
