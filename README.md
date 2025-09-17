# Wearable Digital Auscultation Device for Monitoring Traumatic Chest Injuries

A networked, wearable cardiac auscultation device built around the nRF5340, designed for real-time monitoring of severe chest injuries and the detection of pericardial effusion and cardiac tamponade.

## Overview

During operation, the device is affixed to a patient's chest using Tegaderm film. Audio is captured via a PDM MEMS microphone, real-time Heart Sound Segmentation (HSS) is performed on-device, then features and trends hypothesised to indicate cardiac tamponade are extracted and transmitted over **Bluetooth Low Energy (BLE)** to a web-based clinician dashboard.

## Web Dashboard

**[Click here to launch the dashboard](https://sol455.github.io/nrf_auscultation/)**


## Repository Structure
```
nrf-wearable-cardiac-auscultation/
├── firmware/             # nRF5340 embedded firmware and set up instructions
├── docs/                 # Progressive-Web-App Clinician facing dashboard, hosted on github docs
├── python_dsp/           # Python implementation of the embedded DSP chain, used for validation, testing and plotting
├── hardware              # TO-DO PCB design files etc..
└── README.md             # This file
```

## Quick Links

- **[Firmware Documentation](firmware/README.md)** - Setup and flashing instructions
- **[Web App Dashboard](https://sol455.github.io/nrf_auscultation/)** - Progressive Web App

## Supporting Hardware

- **Stethoscope Diaphragm**: [Littmann 28mm Infant Stethoscope Diaphragm](https://www.medisave.co.uk/products/3m-littmann-stethoscope-spare-parts-kit-classic-ii-infant-assembly)
- **Battery**: [2.7V 40mAh Li-Po Battery - Micron Radio Control](https://micronrc.co.uk/lipo_1s.html)
- **3D-printed enclosure and acoustic pressure amplifier**

## Getting Started

1. **Hardware Setup**: See [firmware documentation](firmware/README.md) for build and flash instructions
2. **Launch Dashboard**: Click the dashboard link above
3. **Pair Device**: Follow pairing instructions in firmware documentation
4. **Start Monitoring**: Place device on patient and begin data collection