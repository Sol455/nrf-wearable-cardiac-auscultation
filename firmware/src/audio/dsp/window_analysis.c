#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "window_analysis.h"
#include "../../ble/heart_service.h"

LOG_MODULE_REGISTER(window_analysis);

void _generate_hann_window(float *buf, uint32_t N) {
    if (!buf || N <= 0) return;
    for (int32_t n = 0; n < N; ++n) {
        buf[n] = 0.5f * (1.0f - cosf(2.0f * M_PI * n / (N - 1)));
        //LOG_INF("%f", buf[n]);
    }
}

void wa_init(WindowAnalysis *window_analysis, const WindowAnalysisConfig *window_analysis_config)
{
    if (!window_analysis || !window_analysis_config) return;

    window_analysis->cfg = *window_analysis_config;
    window_analysis->audio_window = NULL;
    window_analysis->audio_window_len = 0;
    window_analysis->ste_window_len = 0;
    window_analysis->ste_mean = 0.0;
    window_analysis->num_peaks = 0;

    _generate_hann_window(window_analysis->hann_window, window_analysis_config->hs_window_size);
    arm_rfft_fast_init_f32(&window_analysis->fft_instance, (uint16_t)window_analysis_config->hs_window_size);
    //Memset buffers
    memset(window_analysis->ste_buffer, 0, sizeof(window_analysis->ste_buffer));
    memset(window_analysis->scratch_windowed, 0, sizeof(window_analysis->scratch_windowed));
    memset(window_analysis->scratch_fft_out, 0, sizeof(window_analysis->scratch_fft_out));
    memset(window_analysis->scratch_fft_mag, 0, sizeof(window_analysis->scratch_fft_mag));
    memset(window_analysis->peaks, 0, sizeof(window_analysis->peaks));
}

void wa_set_audio_window(WindowAnalysis *window_analysis, const float *audio_window, int32_t window_len, uint32_t window_start_idx)
{
    if (!window_analysis) return;
    window_analysis->window_start_idx = window_start_idx;
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

void wa_label_S1_S2_by_fraction(WindowAnalysis *wa)
{
    if (!wa) return;
    float reject_s1_ratio = wa->cfg.ident_s1_reject_r;      
    float ratio           = wa->cfg.ident_s1_s2_gap_r;     
    float tolerance       = wa->cfg.ident_s1_s2_gap_tol;   
    int32_t window_len    = wa->ste_window_len;

    int candidate_indices[MAX_NUM_WINDOW_PEAKS];
    int n_candidates = 0;
    for (int i = 0; i < wa->num_peaks; ++i) {
        if (wa->peaks[i].type == WINDOW_PEAK_TYPE_CANDIDATE) {
            candidate_indices[n_candidates++] = i;
        }
    }

    // If less than 2 candidates, mark as other and return
    if (n_candidates < 2) {
        for (int j = 0; j < n_candidates; ++j) {
            wa->peaks[candidate_indices[j]].type = WINDOW_PEAK_TYPE_OTHER;
        }
        return;
    }

    float target_gap   = ratio * window_len;
    float allowed_min  = (ratio - tolerance) * window_len;
    float allowed_max  = (ratio + tolerance) * window_len;
    float reject_window = reject_s1_ratio * window_len;

    int best_s1_idx = -1;
    int best_s2_idx = -1;
    float min_dist = FLT_MAX;

    for (int m = 0; m < n_candidates; ++m) {
        for (int n2 = m + 1; n2 < n_candidates; ++n2) {
            int idx1 = wa->peaks[candidate_indices[m]].ste_index;
            int idx2 = wa->peaks[candidate_indices[n2]].ste_index;

            int s1_idx, s2_idx;
            if (idx1 < idx2) {
                s1_idx = candidate_indices[m];
                s2_idx = candidate_indices[n2];
            } else {
                s1_idx = candidate_indices[n2];
                s2_idx = candidate_indices[m];
            }

            if (wa->peaks[s1_idx].ste_index > reject_window) continue;

            int gap = abs(idx2 - idx1);
            if (gap >= allowed_min && gap <= allowed_max) {
                float dist = fabsf((float)gap - target_gap);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_s1_idx = s1_idx;
                    best_s2_idx = s2_idx;
                }
            }
        }
    }

    //Assign S1/S2
    if (best_s1_idx != -1 && best_s2_idx != -1) {
        for (int i = 0; i < n_candidates; ++i) {
            int pidx = candidate_indices[i];
            if (pidx == best_s1_idx) {
                wa->peaks[pidx].type = WINDOW_PEAK_TYPE_S1;
            } else if (pidx == best_s2_idx) {
                wa->peaks[pidx].type = WINDOW_PEAK_TYPE_S2;
            } else {
                wa->peaks[pidx].type = WINDOW_PEAK_TYPE_OTHER;
            }
        }
    } else {
        // If no pair, tag all candidates as other
        for (int j = 0; j < n_candidates; ++j) {
            wa->peaks[candidate_indices[j]].type = WINDOW_PEAK_TYPE_OTHER;
        }
    }
}

void wa_assign_audio_peaks(WindowAnalysis *wa)
{
    if (!wa || !wa->audio_window) return;

    int32_t block_size = wa->cfg.ste_block_size_samples;
    int32_t audio_len = wa->audio_window_len;

    for (int i = 0; i < wa->num_peaks; i++) {
        WindowPeak *peak = &wa->peaks[i];

        if (peak->type == WINDOW_PEAK_TYPE_S1 || peak->type == WINDOW_PEAK_TYPE_S2) {
            int32_t start = peak->ste_index * block_size;
            int32_t end = start + block_size;
            if (start >= audio_len) {
                peak->audio_index = start;
                continue;
            }
            if (end > audio_len) end = audio_len;

            float max_val = 0.0f;
            int32_t max_idx = start;
            for (int32_t j = start; j < end; j++) {
                float abs_sample = fabsf(wa->audio_window[j]);
                if (abs_sample > max_val) {
                    max_val = abs_sample;
                    max_idx = j;
                }
            }
            peak->audio_index = max_idx;
            //peak->audio_value = wa->audio_window[max_idx];
        }
    }
}

float _calc_rms(const float *window, uint32_t len) {
    float rms_val = 0.0f;
    arm_rms_f32(window, len, &rms_val);
    return rms_val;
}

float _calc_spectral_centroid(
    const float *audio_start,
    const float *hann_window,
    uint32_t N,
    uint32_t fs,
    arm_rfft_fast_instance_f32 *rfft_inst,
    float *scratch_windowed,
    float *fft_out,
    float *fft_mag
) {
    // 1. Apply Hann window 
    arm_mult_f32(audio_start, hann_window, scratch_windowed, N);

    // 2. Compute RFFT
    arm_rfft_fast_f32(rfft_inst, scratch_windowed, fft_out, 0);

    // 3. Magnitude spectrum for first N/2+1 bins
    arm_cmplx_mag_f32(fft_out, fft_mag, N/2 + 1);

    // 4. Spectral centroid calculation
    float freq_sum = 0.0f, mag_sum = 0.0f;
    float bin_width = (float)fs / (float)N;

    for (int k = 0; k < N/2 + 1; ++k) {
        float freq = k * bin_width;
        float mag  = fft_mag[k];
        freq_sum  += freq * mag;
        mag_sum   += mag;
    }
    if (mag_sum < 1e-6f)
        return 0.0f;

    return freq_sum / mag_sum;
}

void wa_extract_peak_features(WindowAnalysis *wa)
{
    if (!wa) return;

    for (int32_t i = 0; i < wa->num_peaks; i++) {
        if (wa->peaks[i].type == WINDOW_PEAK_TYPE_S1 || wa->peaks[i].type == WINDOW_PEAK_TYPE_S2) {

            int32_t center = wa->peaks[i].audio_index;
            int32_t half = (int32_t)(wa->cfg.hs_window_size / 2);
            int32_t start = center - half;
            if (start < 0) start = 0;
            int32_t end = start + (int32_t)wa->cfg.hs_window_size;
            if (end > wa->audio_window_len) end = wa->audio_window_len;

            int32_t sub_len = end - start;
            const float *sub_window = &wa->audio_window[start];
            if (sub_len != wa->cfg.hs_window_size) {
                LOG_ERR("Sub window sizes don't match");
                continue;
            }

            // Calculate RMS
            wa->peaks[i].rms = _calc_rms(sub_window, (uint32_t)sub_len);

            // Calculate Spectral Centroid
            wa->peaks[i].centroid = _calc_spectral_centroid(
                sub_window,
                wa->hann_window,
                wa->cfg.hs_window_size,
                MAX_SAMPLE_RATE,
                &wa->fft_instance,
                wa->scratch_windowed,
                wa->scratch_fft_out,
                wa->scratch_fft_mag
            );

            LOG_INF("RMS: %f, SPECTRAL CENTROID: %f", wa->peaks[i].rms, wa->peaks[i].centroid);
        }
    }
}

void wa_make_send_ble(WindowAnalysis *wa) {
    struct heart_packet packet;  
    for (int32_t i = 0; i < wa->num_peaks; i++) {
        if (wa->peaks[i].type == WINDOW_PEAK_TYPE_S1) {
            packet.centroid = wa->peaks[i].centroid;
            packet.rms = wa->peaks[i].rms;
            uint32_t absolute_sample_index = wa->window_start_idx + wa->peaks[i].audio_index;
            uint32_t timestamp_ms = (uint32_t)(((float)absolute_sample_index / (float)MAX_SAMPLE_RATE) * 1000.0f);
            packet.timestamp_ms = timestamp_ms;
            bt_heart_service_notify_packet(&packet);
        }
    }
}

