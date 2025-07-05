#include "rt_peak_detector.h"

LOG_MODULE_REGISTER(rt_peak_detector);

void rt_peak_detector_init(RTPeakDetector *det, RTPeakConfig *rt_peak_config)
{
    det->block_size = rt_peak_config->block_size;
    det->num_blocks = rt_peak_config->num_blocks;
    for (int i = 0; i < 3; ++i)
        det->samples[i] = 0.0f;
    det->index = 0;
    det->running_mean = 0.0f;
    det->alpha = rt_peak_config->alpha;
    det->threshold_scale = rt_peak_config->threshold_scale;
    det->min_distance = rt_peak_config->min_distance_samples;
    det->samples_since_peak = UINT32_MAX;
}

bool rt_peak_detector_update(RTPeakDetector *det,
                             float x,
                             int32_t global_index,
                             RTPeakMessage *out_msg)
{
    det->running_mean += det->alpha * (x - det->running_mean);
    float threshold = det->threshold_scale * det->running_mean;
    det->samples[det->index] = x;

    int i_prev = (det->index + 1) % 3;
    int i_mid = (det->index + 2) % 3;
    int i_next = det->index;

    float prev = det->samples[i_prev];
    float mid = det->samples[i_mid];
    float next = det->samples[i_next];

    bool is_peak = false;

    if (mid > threshold && mid >= prev && mid >= next && (mid > prev || mid > next) && det->samples_since_peak >= det->min_distance){
        is_peak = true;
        det->samples_since_peak = 0;
    } else {
        det->samples_since_peak += 1;
    }

    det->index = (det->index + 1) % 3;

    if (is_peak && out_msg) {
        out_msg->global_index = global_index - 1;
        out_msg->value = mid;
        out_msg->type = RT_PEAK_UNVAL;
        return true;
    }
    return false;
}
