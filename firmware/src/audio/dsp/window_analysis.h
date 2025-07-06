#ifndef WINDOW_ANALYSIS_H
#define WINDOW_ANALYSIS_H

#include <zephyr/kernel.h>
#include "../../macros.h"

typedef struct {
    int32_t ste_index;
    float value;
    char type[8];
} WindowPeak;

typedef struct {
    float audio_hl_thresh;
    uint32_t ste_block_size_samples;
    float ste_hl_thresh;
} WindowAnalysisConfig;

typedef struct {
    const float *audio_window;
    WindowAnalysisConfig cfg;
    int32_t audio_window_len;
    float ste_buffer[STE_MAX_BUF_LEN];
    int32_t ste_window_len;
} WindowAnalysis;

void wa_init(WindowAnalysis *window_analysis, const WindowAnalysisConfig *window_analysis_config);

void wa_set_audio_window(WindowAnalysis *window_analysis, const float *audio_window, int32_t window_len);

float compute_mean_abs(const float *window, int32_t len);

void wa_calc_ste_blocks(WindowAnalysis *window_analysis);

void wa_hard_limit_ste(WindowAnalysis *window_analysis);

int32_t find_peaks_window();

int32_t remove_close_peaks();

float calc_rms();

float calc_centroid();

#endif 
