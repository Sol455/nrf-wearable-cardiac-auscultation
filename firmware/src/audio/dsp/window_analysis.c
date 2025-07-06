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
    window_analysis->ste_mean = 0.0;
    window_analysis->num_peaks = 0;
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

void wa_calc_ste_mean(WindowAnalysis *window_analysis) {
    if (!window_analysis || window_analysis->ste_window_len <= 0) return;
    arm_mean_f32(window_analysis->ste_buffer, window_analysis->ste_window_len, &window_analysis->ste_mean);
}

void wa_hard_limit_ste(WindowAnalysis *window_analysis)
{
    if (!window_analysis || window_analysis->ste_window_len <= 0 || window_analysis->ste_mean == 0.0) return;

    _hard_limit(window_analysis->ste_buffer, window_analysis->ste_window_len, window_analysis->ste_mean, window_analysis->cfg.ste_hl_thresh, window_analysis->ste_buffer);
}

void wa_find_peaks_window(WindowAnalysis *wa) {
    if (!wa || wa->ste_window_len <= 2) return;

    float threshold = wa->ste_mean * wa->cfg.peak_thresh_scale;
    int32_t last_peak = -wa->cfg.peak_min_distance - 1; // Ensures that first peak can be at 0
    int32_t n_found = 0;

    for (int32_t i = 1; i < wa->ste_window_len - 1; ++i) {
        if (
            wa->ste_buffer[i] > threshold &&
            wa->ste_buffer[i] > wa->ste_buffer[i - 1] &&
            wa->ste_buffer[i] > wa->ste_buffer[i + 1] &&
            (i - last_peak) >= wa->cfg.peak_min_distance
        ) {
            if (n_found < MAX_NUM_WINDOW_PEAKS) {
                wa->peaks[n_found].ste_index = i;
                wa->peaks[n_found].value = wa->ste_buffer[i];
                wa->peaks[n_found].type = WINDOW_PEAK_TYPE_UNVAL;
                n_found++;
            }
            last_peak = i;
        }
    }
    wa->num_peaks = n_found; 
}

void wa_remove_close_peaks(WindowAnalysis *wa)
{
    if (!wa || wa->num_peaks <= 0) return;

    int32_t min_gap_samples = (int32_t)(wa->cfg.de_cluster_window_r * wa->ste_window_len);
    int32_t n = wa->num_peaks;

    int32_t i = 0;
    while (i < n) {
        int32_t cluster_start = i;
        int32_t cluster_end = i;
        //Find cluster end
        while (cluster_end + 1 < n &&
               (wa->peaks[cluster_end + 1].ste_index - wa->peaks[cluster_start].ste_index) < min_gap_samples)
        {
            cluster_end++;
        }
        //Find tallest in cluster
        int32_t max_idx = cluster_start;
        for (int32_t j = cluster_start + 1; j <= cluster_end; ++j) {
            if (wa->peaks[j].value > wa->peaks[max_idx].value)
                max_idx = j;
        }
        wa->peaks[max_idx].type = WINDOW_PEAK_TYPE_CANDIDATE;
        i = cluster_end + 1;
    }
}


float calc_rms(const float *window, int32_t len)
{
    return 0;
}

float calc_centroid()
{
    return 0.0f;
}
