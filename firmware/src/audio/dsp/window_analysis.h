#ifndef WINDOW_ANALYSIS_H
#define WINDOW_ANALYSIS_H

#include <zephyr/kernel.h>
#include "../../macros.h"

typedef enum {
    WINDOW_PEAK_TYPE_UNVAL,
    WINDOW_PEAK_TYPE_S1,
    WINDOW_PEAK_TYPE_S2,
    WINDOW_PEAK_TYPE_CANDIDATE,
} WindowPeakType;

typedef struct {
    int32_t ste_index;     
    float value;          
    WindowPeakType type;
    int32_t audio_index;           
} WindowPeak;

typedef struct {
    float audio_hl_thresh;
    uint32_t ste_block_size_samples;
    float ste_hl_thresh;
    float peak_thresh_scale;
    uint32_t peak_min_distance;
    float de_cluster_window_r;
} WindowAnalysisConfig;

typedef struct {
    const float *audio_window;
    WindowAnalysisConfig cfg;
    int32_t audio_window_len;
    float ste_buffer[STE_MAX_BUF_LEN];
    int32_t ste_window_len;
    float ste_mean;
    WindowPeak peaks[MAX_NUM_WINDOW_PEAKS];
    int32_t num_peaks;
} WindowAnalysis;

void wa_init(WindowAnalysis *window_analysis, const WindowAnalysisConfig *window_analysis_config);

void wa_set_audio_window(WindowAnalysis *window_analysis, const float *audio_window, int32_t window_len);

float compute_mean_abs(const float *window, int32_t len);

void wa_calc_ste_mean(WindowAnalysis *window_analysis);

void wa_calc_ste_blocks(WindowAnalysis *window_analysis);

void wa_hard_limit_ste(WindowAnalysis *window_analysis);

void wa_find_peaks_window(WindowAnalysis *wa);

void wa_remove_close_peaks(WindowAnalysis *wa);

float calc_rms();

float calc_centroid();

#endif 
