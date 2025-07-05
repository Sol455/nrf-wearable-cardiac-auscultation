#include "peak_validator.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(peak_validator);

void peak_validator_init(RTPeakValidator* peak_validator, RTPeakValConfig *peak_validator_conf) {
    peak_validator->write_idx = 0;
    peak_validator->valid_count = 0;
    peak_validator->counter_since_last_peak = 0;
    peak_validator->close_r = peak_validator_conf->close_r;
    peak_validator->far_r = peak_validator_conf->far_r;
    peak_validator->margin = peak_validator_conf->margin;
    peak_validator->peak_msgq = peak_validator_conf->peak_msgq;
}

void peak_validator_update(RTPeakValidator* peak_validator) {
    peak_validator->counter_since_last_peak += 1;
}

int peak_validator_notify_peak(RTPeakValidator* peak_validator, RTPeakMessage new_peak) {
    peak_validator->buffer[peak_validator->write_idx] = new_peak;
    peak_validator->write_idx = (peak_validator->write_idx + 1) % PEAK_BUF_SIZE;
    peak_validator->counter_since_last_peak = 0;

    //Start -up logic
    if (peak_validator->valid_count < PEAK_BUF_SIZE)
        peak_validator->valid_count++;

    if (peak_validator->valid_count < PEAK_BUF_SIZE)
        return 0; // Not enough peaks yet

    int idx0 = (peak_validator->write_idx + PEAK_BUF_SIZE - 3) % PEAK_BUF_SIZE; //oldest
    int idx1 = (peak_validator->write_idx + PEAK_BUF_SIZE - 2) % PEAK_BUF_SIZE; //middle
    int idx2 = (peak_validator->write_idx + PEAK_BUF_SIZE - 1) % PEAK_BUF_SIZE; //newest

    RTPeakMessage* p0 = &peak_validator->buffer[idx0];
    RTPeakMessage* p1 = &peak_validator->buffer[idx1];
    RTPeakMessage* p2 = &peak_validator->buffer[idx2];

    // Classification Logic 
    float t1 = (float)(p1->global_index - p0->global_index);
    float t2 = (float)(p2->global_index - p1->global_index);
    float T = t1 + t2;

    float close_thresh = peak_validator->close_r * T;
    float far_thresh = peak_validator->far_r * T;
    float r1 = t1 / T;
    float r2 = t2 / T;
    float lower_bound = 0.5f - peak_validator->margin;
    float upper_bound = 0.5f + peak_validator->margin;

    if (t1 < close_thresh && t2 > far_thresh) {
        if (p0->type == RT_PEAK_S2) {
            p1->type = RT_PEAK_S1; 
            //Accept as S1: previous peak = S2 therefore this peak = S1
        } else {
            p1->type = RT_PEAK_S2; 
            //Accept as S2
        }
    } else if (t1 > close_thresh && t2 < far_thresh) { //Missing S2 therefore its an S1
        p1->type = RT_PEAK_S1;
        //Accept as S1: Missing S2
    } else if ((r1 > lower_bound && r1 < upper_bound) && (r2 > lower_bound && r2 < upper_bound)) {
        p1->type = RT_PEAK_S1;
        //Accept as S1: (S1 S1 S1 case)
    } else {
        p1->type = RT_PEAK_UNVAL;
        //Don't accept peak
    }
    int ret = k_msgq_put(peak_validator->peak_msgq, p1, K_NO_WAIT);
    if (ret != 0) {
        LOG_INF("peak_validator: Failed to put message: global_index=%d type=%d, k_msgq_put returned %d", p1->global_index, p1->type, ret);
    }
    return ret;
}
