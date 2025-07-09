#include "peak_processor.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(peak_processor);

static int32_t compute_pre_samples(int32_t avg_cardiac_period_samples, float pre_ratio, int32_t pre_min_samples, int32_t pre_max_samples)
{
    int32_t pre = (int32_t)(pre_ratio * avg_cardiac_period_samples);
    if (pre < pre_min_samples) pre = pre_min_samples;
    if (pre_max_samples > 0 && pre > pre_max_samples) pre = pre_max_samples;
    return pre;
}

void peak_processor_init(PeakProcessor *proc, const PeakProcessorConfig *conf, PeakProcessFn fn)
{ 
    proc->has_previous_s1 = false;
    proc->process_fn = fn;
    proc->config = *conf;
    proc->window_len = 0;
}

void peak_processor_process_peak(PeakProcessor *proc, const RTPeakMessage *peak_message, const CircularBlockBuffer *slab_buffer)
{
    if (peak_message->type == RT_PEAK_S1) {
        if (proc->has_previous_s1) {
            int32_t s1_idx_prev = proc->previous_s1_event.global_index;
            int32_t s1_idx_curr = peak_message->global_index;
            if (s1_idx_curr > s1_idx_prev) {
                int32_t cardiac_period_samples = s1_idx_curr - s1_idx_prev;
                int32_t pre_samples = compute_pre_samples(
                    cardiac_period_samples,
                    proc->config.pre_ratio,
                    proc->config.pre_min_samples,
                    proc->config.pre_max_samples
                );
                int32_t window_start_index = s1_idx_prev - pre_samples;
                int32_t window_len = s1_idx_curr - window_start_index;

                if (window_len > 0 && window_len <= PP_MAX_WINDOW_LEN) {
                    int ret = cbb_extract_window(
                        slab_buffer, s1_idx_prev, s1_idx_curr,
                        pre_samples, -pre_samples,
                        proc->window, &window_len
                    );
                    if (ret == 0 && proc->process_fn) {
                        proc->window_len = window_len;
                        proc->process_fn(proc->window, window_start_index, window_len);
                    } else {
                        LOG_ERR("Window extraction failed or window too long");
                    }
                } else {
                    LOG_ERR("Invalid window_len: %d (MAX %d)", window_len, PP_MAX_WINDOW_LEN);
                }
            }
        }
        proc->previous_s1_event = *peak_message;
        proc->has_previous_s1 = true;
    }
}
