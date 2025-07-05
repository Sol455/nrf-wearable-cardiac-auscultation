#ifndef RT_PEAK_DETECTOR_H
#define RT_PEAK_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>

typedef struct {
    uint32_t block_size;   
    uint32_t num_blocks;   
    float alpha;
    float threshold_scale;
    uint32_t min_distance_samples;        
} RTPeakConfig;

typedef enum {
    RT_PEAK_UNVAL,
    RT_PEAK_S1,
    RT_PEAL_S2
} RTPeakType;

typedef struct {
    float value;     
    int32_t global_index;  
    RTPeakType type;  
} RTPeakMessage;

typedef struct {
    uint32_t block_size;
    uint32_t num_blocks;
    float samples[3];
    uint8_t index;
    float running_mean;
    float alpha;
    float threshold_scale;
    uint32_t min_distance;
    uint32_t samples_since_peak;
} RTPeakDetector;

void rt_peak_detector_init(RTPeakDetector *det, RTPeakConfig *rt_peak_config);

bool rt_peak_detector_update(RTPeakDetector *det,
                             float x,
                             int32_t global_index,
                             RTPeakMessage *out_msg);

#endif
