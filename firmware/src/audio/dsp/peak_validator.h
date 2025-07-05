#ifndef PEAK_VALIDATOR_H
#define PEAK_VALIDATOR_H

#include <zephyr/kernel.h>
#include "rt_peak_detector.h"

#define PEAK_BUF_SIZE 3

typedef struct {
    float close_r;
    float far_r;
    float margin;
    struct k_msgq *peak_msgq;
} RTPeakValConfig;

typedef struct {
    RTPeakMessage buffer[PEAK_BUF_SIZE];
    uint32_t write_idx;     
    uint32_t valid_count;   
    uint32_t counter_since_last_peak;
    float close_r;
    float far_r;
    float margin;
    struct k_msgq *peak_msgq;
} RTPeakValidator;

void peak_validator_init(RTPeakValidator* peak_validator, RTPeakValConfig *peak_validator_conf);

void peak_validator_update(RTPeakValidator* peak_validator);

int peak_validator_notify_peak(RTPeakValidator* peak_validator, RTPeakMessage new_peak);

#endif
