class PeakProcessor:
    def __init__(self, peak_process_fn):
        self.previous_s1_event = None
        self.peak_process_fn = peak_process_fn

    def compute_pre_samples(self, avg_cardiac_period_samples, pre_ratio=0.1, pre_min_samples=8, pre_max_samples=None):
        pre = int(pre_ratio * avg_cardiac_period_samples)
        pre = max(pre, pre_min_samples)
        if pre_max_samples is not None:
            pre = min(pre, pre_max_samples)
        return pre

    def process_peak_events(self, peak_queue, slab_buffer, pre_samples_ratio, pre_min_samples, pre_max_samples):
        while peak_queue:
            event = peak_queue.pop(0)
            if event.get('type') == 'S1':
                prev = self.previous_s1_event
                if prev is not None:
                    s1_idx_prev = prev['global_index']
                    s1_idx_curr = event['global_index']
                    if s1_idx_curr > s1_idx_prev:
                        cardiac_period_samples = s1_idx_curr - s1_idx_prev
                        pre_samples = self.compute_pre_samples(cardiac_period_samples, pre_samples_ratio, pre_min_samples, pre_max_samples)
                        window_start_index = s1_idx_prev - pre_samples
                        #print(f"PRE: {pre_samples}")
                        window = slab_buffer.extract_window(s1_idx_prev, s1_idx_curr, pre_samples, -pre_samples)
                        if window is not None:
                            self.peak_process_fn(window, window_start_index)
                self.previous_s1_event = event