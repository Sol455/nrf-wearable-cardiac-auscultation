#include "trend_analysis.h"

void trend_analyser_init(TrendAnalyser* ta, int32_t buffer_size, float slope_thresh, int32_t min_windows)
{
    if (buffer_size > TREND_ANALYSER_MAX_BUFFER) {
        buffer_size = TREND_ANALYSER_MAX_BUFFER;
    }
    ta->buffer_size = buffer_size;
    ta->min_windows = min_windows;
    ta->slope_alert_thresh = slope_thresh;
    ta->write_idx = 0;
    ta->count = 0;

    for (int32_t i = 0; i < TREND_ANALYSER_MAX_BUFFER; i++) {
        ta->timestamp_ms[i] = 0.0f;
        ta->feature_values[i] = 0.0f;
    }
}

void trend_analyser_update(TrendAnalyser* ta, float timestamp_ms, float feature_val)
{
    ta->timestamp_ms[ta->write_idx] = timestamp_ms;
    ta->feature_values[ta->write_idx] = feature_val;

    ta->write_idx = (ta->write_idx + 1) % ta->buffer_size;

    if (ta->count < ta->buffer_size) {
        ta->count++;
    }
}

int trend_analyser_get_slope(TrendAnalyser* ta, float* slope_out)
{
    if (ta->count < ta->min_windows) {
        return -1; // Not enough data
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_x2 = 0.0f;
    int32_t n = ta->count;

    int32_t start_idx = ta->write_idx - ta->count;
    if (start_idx < 0) {
        start_idx += ta->buffer_size;
    }

    for (int32_t i = 0; i < n; i++) {
        int32_t idx = (start_idx + i) % ta->buffer_size;
        float x = ta->timestamp_ms[idx];
        float y = ta->feature_values[idx];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    float numerator = n * sum_xy - sum_x * sum_y;
    float denominator = n * sum_x2 - sum_x * sum_x;

    if (denominator == 0.0f) {
        *slope_out = 0.0f;
    } else {
        *slope_out = numerator / denominator;
    }

    return 0;
}

bool trend_analyser_is_alert(TrendAnalyser* ta)
{
    float slope;
    if (trend_analyser_get_slope(ta, &slope) != 0) {
        return false; // Not enough data
    }
    return slope < ta->slope_alert_thresh;
}
