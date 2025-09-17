# nRF Wearable Cardiac Auscultation Embedded Firmware

## Prerequisites

### Required Software
1. **nRF Connect SDK** (last tested with v2.9.0, newer versions should work fine)
   - Recommended: [Download VS Code](https://code.visualstudio.com/) and install the **nRF Connect for VS Code Extension Pack**
   - [Installation Guide](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation/install_ncs.html)

2. **nRF Command Line Tools (nRF Util)**
   - [Download from Nordic](https://www.nordicsemi.com/Products/Development-tools/nRF-Util)

### Hardware Requirements
- J-Link debugger (built into nRF DKs)
- Tag-Connect TC2030-NL programming cable
- Wearable patch PCB (nRF5340 + PDM MEMS microphone + PMIC)

## Quick Start

### Setup
1. **Clone the repository:**
```bash
   git clone https://github.com/Sol455/nrf-wearable-cardiac-auscultation.git
```

2. **Open in VS Code:**
   - File → Open Folder → nrf-wearable-cardiac-auscultation/firmware

3. **Create Build Configuration:**
   - nRF Connect panel → "Add Build Configuration"
   
   **MK1 Hardware:**
   - Board target: `nrf5340dk_nrf5340_cpuapp`
   - Base configuration file: `prj.conf`
   - Base Devicetree overlay: `boards/mic_patch_mk1_nrf5340_cpuapp.overlay`
   
   **MK2 Hardware:**
   - Board target: `nrf5340_audio_dk_nrf5340_cpuapp`
   - Base configuration file: `prj.conf`
   - Base Devicetree overlay: `boards/mic_patch_mk2_nrf5340_cpuapp.overlay`

4. **Build & Flash:**
   - Click "Build" in nRF Connect panel
   - Connect nRF DK with J-Link debugger to PC
   - Attach Tag-Connect to debug out and custom PCB
   - Ensure PCB is powered
   - Click "Flash" in nRF Connect panel


## Hardware Usage & Software Modes

### General Operation
- Press the user button once to enter pairing mode - LED will flash to indicate
- Launch web app and click 'Connect'
- Select 'Heart Patch' - LED will hold red indicating successful pairing
- Place device on cardiac auscultation location, affix with Tegaderm film
- From within the web app, click 'Capture' to start streaming cardiac data


### Hardware MK2: Chunked Audio Transmission
Send 5-second pre-buffered audio recordings for download as .wav files via web app.

**Setup:**
- In `prj.conf` set `CONFIG_HEART_PATCH_DSP_MODE=n` (default is `y` for normal operation)
- Rebuild and flash firmware
- Pair device as described above

**Operation:**
- From the web app, click 'Capture' - this will record and send over Web BLE
- LED will flash to indicate transmission
- When transmission is complete, LED will stop flashing and hold red
- Click 'Download' from the web app to save the .wav file recording

**Note:** Currently very slow due to Web BLE limits and not using the BLE Audio spec. 
**To-Do:** Implement real-time continuous BLE audio transmission based on the [nRF5340 Audio Application](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/applications/nrf5340_audio/index.html)

### Hardware MK1: SD Card Recording
SD card recording and test of DSP chain via .wav file playback.
@To-Do

### Debug Output
- Use RTT Viewer for real-time debug output
- In VS Code: Navigate to Terminal → + → Add nRF RTT Terminal
- Select your DK, then select Application Core

## Configuration Macros

### Audio Buffer Settings (`macros.h`)

**WAV_LENGTH_BLOCKS**
- Controls the length of each real-time stream and capture session
- Default: `200` blocks (20 seconds) for normal operation
- Can be adjusted to any required duration
- Each block = 100ms of audio data
- For audio transmission mode, this is automatically set to `50` blocks via Kconfig parameter to avoid overflowing internal RAM

**Implementation:**
```c
#ifdef CONFIG_HEART_PATCH_DSP_MODE
#define WAV_LENGTH_BLOCKS 200    // Normal DSP operation (20s)
#else
#define WAV_LENGTH_BLOCKS 50     // Audio transmission mode (5s)
#endif
```
