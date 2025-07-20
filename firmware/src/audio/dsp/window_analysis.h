#ifndef WINDOW_ANALYSIS_H
#define WINDOW_ANALYSIS_H

#include <zephyr/kernel.h>
#include "../../macros.h"
#include "arm_math.h"
#include "trend_analysis.h"

typedef enum {
    WINDOW_PEAK_TYPE_UNVAL,
    WINDOW_PEAK_TYPE_S1,
    WINDOW_PEAK_TYPE_S2,
    WINDOW_PEAK_TYPE_CANDIDATE,
    WINDOW_PEAK_TYPE_OTHER,
} WindowPeakType;

typedef struct {
    int32_t ste_index;     
    float value;          
    WindowPeakType type;
    int32_t global_index; 
    int32_t audio_index;   
    float rms;
    float centroid;        
} WindowPeak;

typedef struct {
    float audio_hl_thresh;
    uint32_t ste_block_size_samples;
    float ste_hl_thresh;
    float peak_thresh_scale;
    uint32_t peak_min_distance;
    float de_cluster_window_r;
    float ident_s1_reject_r; //reject S1 if now within this ratio of cardiac window
    float ident_s1_s2_gap_r; //timing gap between S1 and S2
    float ident_s1_s2_gap_tol; //timing gap tolerance
    uint32_t hs_window_size;
    //Trend analysis
    int32_t ta_rms_buf_size;
    float ta_rms_slope_thresh;
    int32_t ta_rms_min_windows;

    int32_t ta_centroid_buf_size;
    float ta_centroid_slope_thresh;
    int32_t ta_centroid_min_windows;

} WindowAnalysisConfig;

typedef struct {
    const float *audio_window;
    WindowAnalysisConfig cfg;
    uint32_t window_start_idx;
    int32_t audio_window_len;
    float ste_buffer[STE_MAX_BUF_LEN];
    int32_t ste_window_len;
    float ste_mean;
    WindowPeak peaks[MAX_NUM_WINDOW_PEAKS];
    int32_t num_peaks;
    float hann_window[HS_WINDOW_SIZE];
    arm_rfft_fast_instance_f32 fft_instance;
    float scratch_windowed[HS_WINDOW_SIZE];
    float scratch_fft_out[HS_WINDOW_SIZE + 2];
    float scratch_fft_mag[(HS_WINDOW_SIZE / 2) - 1];
    TrendAnalyser ta_s1_rms;
    TrendAnalyser ta_s2_rms;
    TrendAnalyser ta_s1_centroid;
    TrendAnalyser ta_s2_centroid;
} WindowAnalysis;

void wa_init(WindowAnalysis *window_analysis, const WindowAnalysisConfig *window_analysis_config);

void wa_set_audio_window(WindowAnalysis *window_analysis, const float *audio_window, int32_t window_len, uint32_t window_start_idx);

float compute_mean_abs(const float *window, int32_t len);

void wa_calc_ste_mean(WindowAnalysis *window_analysis);

void wa_calc_ste_blocks(WindowAnalysis *window_analysis);

void wa_hard_limit_ste(WindowAnalysis *window_analysis);

void wa_find_peaks_window(WindowAnalysis *wa);

void wa_remove_close_peaks(WindowAnalysis *wa);

void wa_label_S1_S2_by_fraction(WindowAnalysis *wa);

void wa_assign_audio_peaks(WindowAnalysis *wa);

void wa_extract_peak_features(WindowAnalysis *wa);

void wa_push_trends(WindowAnalysis *wa);

void wa_make_send_ble(WindowAnalysis *wa);

#endif 
