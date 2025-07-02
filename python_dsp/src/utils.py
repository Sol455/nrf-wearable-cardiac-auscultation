import numpy as np
import scipy.io.wavfile as wav
import matplotlib.pyplot as plt
import os
import math

def read_wav_blocks(filename, block_size_samples, process_block_fn):
    sample_rate, data = wav.read(filename)

    if data.ndim > 1:
        data = data[:, 0]

    if data.dtype == np.int16:
        data = data.astype(np.float32) / 32768.0

    total_samples = len(data)
    num_blocks = total_samples // block_size_samples

    for i in range(num_blocks):
        start = i * block_size_samples
        end = start + block_size_samples
        block = data[start:end]
        process_block_fn(block, sample_rate)

    return sample_rate

def plot_waveform(signal, fs, title="Audio Waveform", xlim=None):
    t = np.arange(len(signal)) / fs
    plt.figure(figsize=(10, 4))
    plt.plot(t, signal, label="Waveform", linewidth=1)
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.title(title)
    if xlim:
        plt.xlim(xlim)
    plt.grid(True)
    plt.tight_layout()

def plot_debug_audio_and_peaks(
    debug_audio_blocks, 
    debug_global_peaks, 
    debug_envelope, 
    energies=None, 
    fs=16000, 
    file_loc="../output/img.png"
):
    
    all_indices = [p["global_index"] for p in debug_global_peaks]
    for idx_prev, idx_next, p_prev, p_next in zip(all_indices, all_indices[1:], debug_global_peaks, debug_global_peaks[1:]):
        if idx_next < idx_prev:
            print(f"[ERROR] Peak went backwards: {idx_prev} -> {idx_next}")
            print(f"  Previous peak: {p_prev}")
            print(f"  Next peak:     {p_next}")
    if not debug_audio_blocks:
        print("No audio blocks to plot.")
        return

    full_audio = np.concatenate(debug_audio_blocks)
    full_envelope = np.concatenate(debug_envelope)
    t = np.arange(len(full_audio)) / fs 

    type_styles = {
        "unval": {"color": "gray", "marker": "x", "label": "Raw Peak"},
        "S1": {"color": "red", "marker": "o", "label": "Validated Systole"},
        "S2": {"color": "blue", "marker": "o", "label": "Validated Dysystole"},
    }

    plt.figure(figsize=(12, 4))
    plt.plot(t, full_audio, label="Signal", alpha=0.6)
    plt.plot(t, full_envelope, label="Envelope", alpha=0.8)

    if debug_global_peaks:
        for peak_type in set(p.get("type", "unval") for p in debug_global_peaks):
            matching_peaks = [p for p in debug_global_peaks if p.get("type") == peak_type]
            if not matching_peaks:
                continue

            indices = [p["global_index"] for p in matching_peaks]
            values = [p["value"] for p in matching_peaks]
            peak_times = np.array(indices) / fs

            style = type_styles.get(peak_type, {"color": "black", "marker": ".", "label": peak_type})
            plt.scatter(
                peak_times,
                values,
                color=style["color"],
                marker=style["marker"],
                label=style["label"],
                zorder=3
            )

    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.title("Peaks and Envelope (Real-time)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    #plt.savefig(file_loc, dpi=300, bbox_inches='tight')


def plot_energies(debug_audio_blocks,energies, energy_peaks, fs=16000, file_loc="output/img.png"):

    if not debug_audio_blocks:
        print("No audio blocks to plot.")
        return

    full_audio = np.concatenate(debug_audio_blocks)
    
    samples_per_energy = len(full_audio) // len(energies)
    repeated_energy = np.repeat(energies, 160)

    t = np.arange(len(full_audio)) / fs 
    if energy_peaks:
        indices = [p["global_index"] for p in energy_peaks]
        values = [p["value"] for p in energy_peaks]
        peak_times = np.array(indices) / fs
    else:
        peak_times = []
        values = []

    plt.figure(figsize=(12, 4))
    plt.plot(t, full_audio, label="Signal", alpha=0.6)
    plt.plot(t, repeated_energy, label="Energy", alpha=0.7)

    if len(peak_times) > 0:
        plt.scatter(peak_times, values, color="red", marker="x", label="Detected Peaks", zorder=3)

    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")
    plt.title("Energies vs audio (Real Time)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(file_loc, dpi=300, bbox_inches='tight')

def plot_energy_direct(energies, energy_peaks, fs=16000, file_loc="../output/img.png"):

    full_energy = np.concatenate(energies)


    t = np.arange(len(full_energy))# / fs 
    if energy_peaks:
        indices = [p["global_index"] for p in energy_peaks]
        values = [p["value"] for p in energy_peaks]
        peak_times = np.array(indices)# / fs
    else:
        peak_times = []
        values = []

    plt.figure(figsize=(12, 4))
    plt.plot(t, full_energy, label="Energy", alpha=0.6)

    if len(peak_times) > 0:
        plt.scatter(peak_times, values, color="red", marker="x", label="Detected Peaks", zorder=3)

    plt.xlabel("Energy Block Index")
    plt.ylabel("Amplitude")
    plt.title("Energies vs audio (Real Time)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(file_loc, dpi=300, bbox_inches='tight')

def plot_audio_windows(
    segments, 
    audio_peaks_list=None, 
    y_lim=None, 
    sample_rate=16000, 
    max_cols=4, 
    figsize_per_plot=(4, 2),
    type_styles=None
):
    n = len(segments)
    if n == 0:
        print("No segments to plot!")
        return

    if type_styles is None:
        type_styles = {
            "S1": {"color": "red", "label": "S1"},
            "S2": {"color": "blue", "label": "S2"},
            "candidate": {"color": "purple", "label": "Candidate"},
            "other": {"color": "orange", "label": "Other"},
            "unval": {"color": "gray", "label": "Unval"},
        }

    ncols = min(max_cols, n)
    nrows = math.ceil(n / ncols)
    figsize = (figsize_per_plot[0] * ncols, figsize_per_plot[1] * nrows)

    fig, axes = plt.subplots(nrows, ncols, figsize=figsize, squeeze=False)

    for i, segment in enumerate(segments):
        row, col = divmod(i, ncols)
        ax = axes[row][col]
        t = np.arange(len(segment)) / sample_rate
        ax.plot(t, segment)
        ax.set_title(f"Segment {i+1}")
        ax.set_xlabel("Time [s]")
        ax.set_ylabel("Amplitude")
        if y_lim is not None:
            ax.set_ylim(-y_lim, y_lim)
        ax.grid(True)

        if audio_peaks_list is not None and i < len(audio_peaks_list):
            these_peaks = audio_peaks_list[i]
            plotted_types = set()
            for peak in these_peaks:
                peak_type = peak.get("type", "other")
                style = type_styles.get(
                    peak_type, {"color": "black", "label": peak_type}
                )
                x = peak['audio_index'] / sample_rate
                label = style["label"] if peak_type not in plotted_types else None
                ax.axvline(x, color=style["color"], linestyle="--", linewidth=1.2, label=label)
                plotted_types.add(peak_type)
        handles, labels = ax.get_legend_handles_labels()
        by_label = dict(zip(labels, handles))
        ax.legend(by_label.values(), by_label.keys(), fontsize="small")

    # Hide unused subplots 
    for i in range(n, nrows * ncols):
        row, col = divmod(i, ncols)
        axes[row][col].axis('off')

    plt.tight_layout()

def plot_STE_windows(
    ste_segments,
    peaks,
    y_lim=None,
    sample_rate=16000,
    max_cols=4,
    figsize_per_plot=(4, 2),
    type_styles=None,
):
    n = len(ste_segments)
    if n == 0:
        print("No segments to plot!")
        return

    # Default styles if none given
    if type_styles is None:
        type_styles = {
            "unval": {"color": "gray", "marker": "x", "label": "Raw Peak"},
            "candidate": {"color": "purple", "marker": "v", "label": "Candidate"},
            "S1": {"color": "red", "marker": "o", "label": "S1"},
            "S2": {"color": "blue", "marker": "o", "label": "S2"},
            "other": {"color": "orange", "marker": "d", "label": "Other"},
        }

    ncols = min(max_cols, n)
    nrows = math.ceil(n / ncols)
    figsize = (figsize_per_plot[0] * ncols, figsize_per_plot[1] * nrows)

    fig, axes = plt.subplots(nrows, ncols, figsize=figsize, squeeze=False)

    for i, segment in enumerate(ste_segments):
        row, col = divmod(i, ncols)
        ax = axes[row][col]
        t = np.arange(len(segment)) / sample_rate

        ax.plot(t, segment, label="STE", alpha=0.8)

        if peaks is not None and i < len(peaks):
            these_peaks = peaks[i]
            if these_peaks:
                type_set = set(p.get("type", "unval") for p in these_peaks)
                for peak_type in type_set:
                    matching_peaks = [p for p in these_peaks if p.get("type", "unval") == peak_type]
                    if not matching_peaks:
                        continue
                    peak_indices = [p["ste_index"] for p in matching_peaks]
                    peak_values = [p["value"] for p in matching_peaks]
                    style = type_styles.get(
                        peak_type, {"color": "black", "marker": ".", "label": peak_type}
                    )
                    ax.scatter(
                        np.array(peak_indices) / sample_rate,
                        peak_values,
                        color=style["color"],
                        marker=style["marker"],
                        label=style["label"],
                        zorder=3,
                    )

        ax.set_title(f"Segment {i+1}")
        ax.set_xlabel("Time [s]")
        ax.set_ylabel("Amplitude")
        if y_lim is not None:
            ax.set_ylim(-y_lim, y_lim)
        ax.grid(True)
        handles, labels = ax.get_legend_handles_labels()
        by_label = dict(zip(labels, handles))
        ax.legend(by_label.values(), by_label.keys(), fontsize='small')

    # Hide unused subplots
    for i in range(n, nrows * ncols):
        row, col = divmod(i, ncols)
        axes[row][col].axis('off')

    plt.tight_layout()

def plot_fft_overlay(window_events, freq_cutoff=200):
    s1_ffts = []
    s1_centroids = []
    s2_ffts = []
    s2_centroids = []

    freq_axis = None

    for ev in window_events:
        if isinstance(ev['debug_S1_y_dB'], np.ndarray) and isinstance(ev['debug_S1_fft_freq'], np.ndarray):
            s1_ffts.append(ev['debug_S1_y_dB'])
            s1_centroids.append(ev['S1_centroid'])
            if freq_axis is None:
                freq_axis = ev['debug_S1_fft_freq']
        if isinstance(ev['debug_S2_y_dB'], np.ndarray) and isinstance(ev['debug_S2_fft_freq'], np.ndarray):
            s2_ffts.append(ev['debug_S2_y_dB'])
            s2_centroids.append(ev['S2_centroid'])

    if freq_axis is None:
        print("No FFT data to plot.")
        return

    cutoff_idx = np.where(freq_axis <= freq_cutoff)[0]
    if len(cutoff_idx) == 0:
        print("Frequency cutoff too low, nothing to plot.")
        return
    cutoff_idx = cutoff_idx[-1] + 1

    plt.figure(figsize=(10, 4))

    # S1 subplot
    plt.subplot(1, 2, 1)
    for y, c in zip(s1_ffts, s1_centroids):
        plt.plot(freq_axis[:cutoff_idx], y[:cutoff_idx], color='gray', alpha=0.3)
        if c < freq_axis[cutoff_idx-1]:
            plt.axvline(c, color='r', linestyle=':', alpha=0.5)
    if s1_ffts:
        plt.plot(freq_axis[:cutoff_idx], np.mean([y[:cutoff_idx] for y in s1_ffts], axis=0), color='blue', label='Mean S1 FFT', lw=2)
        mean_centroid = np.mean([c for c in s1_centroids if c < freq_axis[cutoff_idx-1]])
        plt.axvline(mean_centroid, color='red', linestyle='-', label='Mean S1 Centroid', lw=2)
    plt.title("S1 FFTs")
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude (dB)")
    plt.legend()
    plt.grid(True)

    # S2 subplot
    plt.subplot(1, 2, 2)
    for y, c in zip(s2_ffts, s2_centroids):
        plt.plot(freq_axis[:cutoff_idx], y[:cutoff_idx], color='gray', alpha=0.3)
        if c < freq_axis[cutoff_idx-1]:
            plt.axvline(c, color='r', linestyle=':', alpha=0.5)
    if s2_ffts:
        plt.plot(freq_axis[:cutoff_idx], np.mean([y[:cutoff_idx] for y in s2_ffts], axis=0), color='green', label='Mean S2 FFT', lw=2)
        mean_centroid = np.mean([c for c in s2_centroids if c < freq_axis[cutoff_idx-1]])
        plt.axvline(mean_centroid, color='red', linestyle='-', label='Mean S2 Centroid', lw=2)
    plt.title("S2 FFTs")
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude (dB)")
    plt.legend()
    plt.grid(True)

    plt.tight_layout()
    plt.show()

def plot_rms_vs_event(window_events, y_lim=None):
    s1_rms = [ev['S1_rms'] for ev in window_events if isinstance(ev['S1_rms'], (float, int, np.floating))]
    s2_rms = [ev['S2_rms'] for ev in window_events if isinstance(ev['S2_rms'], (float, int, np.floating))]
    plt.figure(figsize=(8, 4))
    plt.scatter(range(len(s1_rms)), s1_rms, label='S1 RMS', color='blue')
    plt.scatter(range(len(s2_rms)), s2_rms, label='S2 RMS', color='green')
    plt.xlabel("Event Number")
    plt.ylabel("RMS Amplitude")
    plt.title("RMS of S1 and S2 Over Events")
    if y_lim is not None:
        plt.ylim(-y_lim, y_lim)
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()

def plot_rms_distribution(window_events, y_lim=None):
    s1_rms = [ev['S1_rms'] for ev in window_events if isinstance(ev['S1_rms'], (float, int, np.floating))]
    s2_rms = [ev['S2_rms'] for ev in window_events if isinstance(ev['S2_rms'], (float, int, np.floating))]
    plt.figure(figsize=(6, 4))
    plt.boxplot([s1_rms, s2_rms], labels=['S1', 'S2'])
    plt.ylabel("RMS Amplitude")
    plt.title("RMS Distribution: S1 vs S2")
    if y_lim is not None:
        plt.ylim(-y_lim, y_lim)
    plt.tight_layout()
    plt.show()


