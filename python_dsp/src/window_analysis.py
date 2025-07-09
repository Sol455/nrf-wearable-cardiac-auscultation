import numpy as np

def compute_energy_blocks(signal, fs=16000, samples_per_window = 160):
    num_windows = len(signal) // samples_per_window

    energies = np.zeros(num_windows)
    for k in range(num_windows):
        start = k * samples_per_window
        end = start + samples_per_window
        window = signal[start:end]
        energies[k] = np.sum(window ** 2)
    return energies

def hard_limit(window, threshold):
    limited_window= np.zeros_like(window)
    hard_limit_thresh = np.mean(np.abs(window)) * threshold
    for i, sample in enumerate(window):
        if np.abs(sample) <= hard_limit_thresh:
            limited_window[i] = 0.0
        else:
            limited_window[i] = sample
    return limited_window

def find_peaks_window(window, mean, threshold_scale=1.5, min_distance=1):

    threshold = mean * threshold_scale
    segment_peaks = []
    last_peak = -min_distance - 1  # Ensures first peak can be at index 0

    for i in range(1, len(window) - 1):
        if (
            window[i] > threshold and
            window[i] > window[i - 1] and
            window[i] > window[i + 1] and
            (i - last_peak) >= min_distance
        ):
            window_peak = {
                "ste_index": i,
                "value": window[i],
                "audio_loc": "unval",
                "type": "unval"
            }
            segment_peaks.append(window_peak)
            last_peak = i

    return segment_peaks

def remove_close_peaks(peaks, min_gap_ratio, ste_cycle_length):
    min_gap_samples = int(min_gap_ratio * ste_cycle_length)

    if not peaks:
        return []

    peaks.sort(key=lambda p: p['ste_index'])  # sort in place
    n = len(peaks)

    i = 0
    while i < n:
        cluster_start = i
        cluster_end = i
        while (cluster_end + 1 < n and
               peaks[cluster_end + 1]['ste_index'] - peaks[cluster_start]['ste_index'] < min_gap_samples):
            cluster_end += 1

        max_idx = cluster_start
        for j in range(cluster_start + 1, cluster_end + 1):
            if peaks[j]['value'] > peaks[max_idx]['value']:
                max_idx = j

        peaks[max_idx]['type'] = 'candidate' 

        i = cluster_end + 1

    return peaks


def label_S1_S2_by_fraction(
    peaks, window_len, reject_s1_ratio=0.3, ratio=0.3, tolerance=0.1
):

    candidate_indices = [i for i, p in enumerate(peaks) if p.get('type') == 'candidate']
    n = len(candidate_indices)
    if n < 2:
        for i in candidate_indices:
            peaks[i]['type'] = 'other'
        return peaks

    best_pair = None
    min_dist = float('inf')
    target_gap = ratio * window_len
    allowed_min = (ratio - tolerance) * window_len
    allowed_max = (ratio + tolerance) * window_len

    reject_window = reject_s1_ratio * window_len

    for m in range(n):
        for n2 in range(m + 1, n):
            idx1 = peaks[candidate_indices[m]]['ste_index']
            idx2 = peaks[candidate_indices[n2]]['ste_index']

            # Assign S1/S2 (earlier is S1)
            if idx1 < idx2:
                s1_idx, s2_idx = candidate_indices[m], candidate_indices[n2]
            else:
                s1_idx, s2_idx = candidate_indices[n2], candidate_indices[m]

            # Enforce S1 is early enough
            if peaks[s1_idx]['ste_index'] > reject_window:
                continue

            gap = abs(idx2 - idx1)
            if allowed_min <= gap <= allowed_max:
                dist = abs(gap - target_gap)
                if dist < min_dist:
                    min_dist = dist
                    best_pair = (s1_idx, s2_idx)

    # Tag best pair S1/S2; all other candidates as 'other'
    if best_pair is not None:
        s1_idx, s2_idx = best_pair
        for i in candidate_indices:
            if i == s1_idx:
                peaks[i]['type'] = 'S1'
            elif i == s2_idx:
                peaks[i]['type'] = 'S2'
            else:
                peaks[i]['type'] = 'other'
    else:
        for i in candidate_indices:
            peaks[i]['type'] = 'other'

    return peaks


def find_audio_peak_in_block(audio, ste_index, block_size):
    start = ste_index * block_size
    end = start + block_size
    n = len(audio)
    if start >= n:
        return start, 0.0  # fallback
    if end > n:
        end = n

    max_val = 0.0
    max_idx = start
    for i in range(start, end):
        v = abs(audio[i])
        if v > max_val:
            max_val = v
            max_idx = i
    return max_idx, audio[max_idx]

def label_s1_s2_audio(peaks, ste_window, audio_window, block_size):
    events = []
    for peak in peaks:
        # Only consider S1 or S2 
        if peak['type'] in ['S1', 'S2']:
            audio_idx, audio_val = find_audio_peak_in_block(audio_window, peak['ste_index'], block_size)
            event = {
                'type': peak['type'],
                'audio_index': audio_idx,
                'audio_value': audio_val
            }
            events.append(event)
    return events

def extract_fixed_window(audio, peak_index, window_size):
    half = window_size // 2
    start = max(0, peak_index - half)
    end = min(len(audio), peak_index + half)
    window = audio[start:end]
    # # Optionally pad to window_size if at signal edges
    # if len(window) < window_size:
    #     window = np.pad(window, (0, window_size - len(window)), mode='constant')
    return window

def calc_rms(window):
    return np.sqrt(np.mean(window ** 2))

def calc_centroid(signal, fs):
    signal = np.asarray(signal)
    N = len(signal)
    window = np.hanning(N)
    windowed = signal * window

    X = np.abs(np.fft.rfft(windowed))
    freqs = np.fft.rfftfreq(N, d=1/fs)
    Y_dB = 20 * np.log10(X + np.finfo(float).eps)

    # Spectral centroid based on magnitude
    centroid = np.sum(freqs * X) / (np.sum(X) + 1e-12)

    return freqs, Y_dB, centroid

def make_window_event(window, ste, window_time_ms):
    cardiac_window_event = {
        "cardiac_period": len(window),
        "period_time_stamp_ms": window_time_ms,
        "ste_profile": ste,  # e.g. list or np.array, as appropriate
        "S1_rms": "unval",
        "S1_centroid": "unval",
        "S2_rms": "unval",
        "S2_centroid": "unval",
        "debug_S1_fft_freq": "unval",
        "debug_S1_y_dB": "unval",
        "debug_S2_fft_freq": "unval",
        "debug_S2_y_dB": "unval",
    }
    return cardiac_window_event

def populate_window_features(window_event, peaks, window, fs, s_window_size):
    for peak in peaks:
        if peak['type'] not in ('S1', 'S2'):
            continue  # Skip non-heart-sound peaks
        peak_index = peak['audio_index']
        s_window = extract_fixed_window(window, peak_index, s_window_size)
        rms = calc_rms(s_window)
        freqs, Y_dB, spectral_centroid = calc_centroid(s_window, fs)
        print(f"RMS: {rms}, CENTROID: {spectral_centroid}")
        if peak['type'] == 'S1':
            window_event["S1_rms"] = rms
            window_event["S1_centroid"] = spectral_centroid
            window_event["debug_S1_fft_freq"] = freqs
            window_event["debug_S1_y_dB"] = Y_dB
        elif peak['type'] == 'S2':
            window_event["S2_rms"] = rms
            window_event["S2_centroid"] = spectral_centroid
            window_event["debug_S2_fft_freq"] = freqs
            window_event["debug_S2_y_dB"] = Y_dB
    return window_event