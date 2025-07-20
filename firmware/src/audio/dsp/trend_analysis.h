#ifndef TREND_ANALYSIS_H
#define TREND_ANALYSIS_H

#include <zephyr/kernel.h>
#include "../../macros.h"


typedef struct {
    float timestamp_ms[TREND_ANALYSER_MAX_BUFFER];
    float feature_values[TREND_ANALYSER_MAX_BUFFER];
    int32_t buffer_size;
    int32_t min_windows;
    float slope_alert_thresh;
    int32_t write_idx;
    int32_t count;
} TrendAnalyser;

void trend_analyser_init(TrendAnalyser* ta, int32_t buffer_size, float slope_thresh, int32_t min_windows);

void trend_analyser_update(TrendAnalyser* ta, float timestamp_ms, float feature_val);

int trend_analyser_get_slope(TrendAnalyser* ta, float* slope_out);

bool trend_analyser_is_alert(TrendAnalyser* ta);

#endif // TREND_ANALYSIS_H
