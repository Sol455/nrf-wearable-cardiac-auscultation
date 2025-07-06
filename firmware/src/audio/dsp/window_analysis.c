#include "window_analysis.h"
#include <zephyr/logging/log.h>
#include "arm_math.h"

LOG_MODULE_REGISTER(window_analysis);


void wa_init(WindowAnalysis *window_analysis, const WindowAnalysisConfig *window_analysis_config)
{
    if (!window_analysis || !window_analysis_config) return;

    window_analysis->cfg = *window_analysis_config;
    window_analysis->audio_window = NULL;
    window_analysis->audio_window_len = 0;
    window_analysis->ste_window_len = 0;
    //Memset STE buffer
    memset(window_analysis->ste_buffer, 0, sizeof(window_analysis->ste_buffer));
   
}

void wa_set_audio_window(WindowAnalysis *window_analysis, const float *audio_window, int32_t window_len)
{
    if (!window_analysis) return;
    window_analysis->audio_window = audio_window;
    window_analysis->audio_window_len = window_len;
}


float compute_mean_abs(const float *window, int32_t len)
{
    float abs_sum = 0.0f;
    for (int32_t i = 0; i < len; i++)
        abs_sum += fabsf(window[i]);
    return abs_sum / len;
}

void _hard_limit(const float *window, int32_t len, float mean, float threshold, float *out)
{
    float hl_thresh = mean * threshold;
    for (int32_t i = 0; i < len; i++)
        out[i] = (fabsf(window[i]) > hl_thresh) ? window[i] : 0.0f;
}

void wa_calc_ste_blocks(WindowAnalysis *window_analysis)
{
    if (!window_analysis || !window_analysis->audio_window) return;

    uint32_t block_size = window_analysis->cfg.ste_block_size_samples;
    int32_t num_blocks = window_analysis->audio_window_len / block_size;
    window_analysis->ste_window_len = num_blocks; 

    for (int32_t k = 0; k < num_blocks; k++) {
        float sum = 0.0f;
        int32_t start = k * block_size;
        for (uint32_t i = 0; i < block_size; i++) {   
            float v = window_analysis->audio_window[start + i];
            sum += v * v;
        }
        window_analysis->ste_buffer[k] = sum;
    }
}

void wa_hard_limit_ste(WindowAnalysis *window_analysis)
{
    if (!window_analysis || window_analysis->ste_window_len <= 0) return;
    float mean;
    arm_mean_f32(window_analysis->ste_buffer, window_analysis->ste_window_len, &mean);
    _hard_limit(window_analysis->ste_buffer, window_analysis->ste_window_len, mean, window_analysis->cfg.ste_hl_thresh, window_analysis->ste_buffer);
}

int32_t find_peaks_window()
{
    return 0;

}

int32_t remove_close_peaks()
{
    return 0;
}

float calc_rms(const float *window, int32_t len)
{
    return 0;
}

float calc_centroid()
{
    return 0.0f;
}
