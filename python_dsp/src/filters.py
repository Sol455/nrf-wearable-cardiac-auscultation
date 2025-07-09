import numpy as np
import os
import matplotlib.pyplot as plt
from scipy.signal import butter, sosfreqz

def design_bandpass_iir(fs, low_cutoff, high_cutoff, order=8):

    sos = butter(order, [low_cutoff, high_cutoff], btype='bandpass', fs=fs, output='sos')
    return sos

def design_lowpass_iir(fs, cutoff=20, order=3):
    sos = butter(order, cutoff, btype='low', fs=fs, output='sos')
    return sos

def plot_filter_response(sos, fs, title="Filter Frequency Response"):

    w, h = sosfreqz(sos, worN=2048, fs=fs)
    magnitude_db = 20 * np.log10(np.maximum(np.abs(h), 1e-10)) 

    plt.figure(figsize=(10, 4))
    plt.plot(w, magnitude_db, label="Magnitude Response")
    plt.title(title)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude (dB)")
    plt.xscale('log')
    plt.xlim(10, fs / 2) 
    plt.ylim(-100, 5)
    plt.xticks([10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 8000])
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.tight_layout()
    plt.legend()

# def export_sos_to_cmsis_header(sos, file_path="cmsis_filter_coeffs.h", var_name="filter_coeffs"):
#     assert sos.shape[1] == 6, "Expected SOS with 6 coefficients per row"
#     coeffs = []
#     for i, section in enumerate(sos):
#         b0, b1, b2, a0, a1, a2 = section
#         assert np.isclose(a0, 1.0), f"Section {i}: a0 != 1.0 (got {a0})"
#         coeffs.extend([b0, b1, b2, -a1, -a2])

#     os.makedirs(os.path.dirname(file_path), exist_ok=True)

#     # Write header file
#     with open(file_path, "w") as f:
#         f.write(f"// Auto-generated CMSIS-DSP biquad coefficients\n")
#         f.write(f"#ifndef {var_name.upper()}_H\n#define {var_name.upper()}_H\n\n")
#         f.write(f"#define NUM_STAGES ({sos.shape[0]})\n\n")
#         f.write(f"float {var_name}[] = {{\n")
#         for i, val in enumerate(coeffs):
#             f.write(f"    {val:.8f},\n")
#         f.write("};\n\n#endif\n")

#     print(f"CMSIS coeffs written to: {file_path} with array name: {var_name}")

def export_sos_to_cmsis_header(
    sos,
    file_path="cmsis_filter_coeffs.h",
    var_name="filter_coeffs",
    macro_name="NUM_STAGES"
):
    assert sos.shape[1] == 6, "Expected SOS with 6 coefficients per row"
    coeffs = []
    for i, section in enumerate(sos):
        b0, b1, b2, a0, a1, a2 = section
        assert np.isclose(a0, 1.0), f"Section {i}: a0 != 1.0 (got {a0})"
        coeffs.extend([b0, b1, b2, -a1, -a2])

    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    num_stages = sos.shape[0]

    with open(file_path, "w") as f:
        f.write(f"// Auto-generated CMSIS-DSP biquad coefficients\n")
        f.write(f"#ifndef {var_name.upper()}_H\n#define {var_name.upper()}_H\n\n")
        f.write(f"#define {macro_name} {num_stages}\n\n")
        f.write(f"float {var_name}[] = {{\n")
        for i, val in enumerate(coeffs):
            f.write(f"    {val:.8f},\n")
        f.write("};\n\n#endif\n")

    print(f"CMSIS coeffs written to: {file_path} with array name: {var_name} ({macro_name}={num_stages})")
