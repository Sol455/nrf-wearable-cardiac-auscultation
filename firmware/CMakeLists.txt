cmake_minimum_required(VERSION 3.20.0)
# Define DT Overlay:
# Prototype 1: mic_patch_mk1_nrf5340_cpuapp.overlay
# Prototype 2: mic_patch_mk2_nrf5340_cpuapp.overlay
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/mic_patch_mk2_nrf5340_cpuapp.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrf_auscultation)

set(NRF5340_NETWORK_CORE_IMAGE hci_ipc)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/event_handler.c)
target_sources(app PRIVATE src/audio/wav_file.c)
target_sources(app PRIVATE src/modules/sd_card.c)
target_sources(app PRIVATE src/modules/button_handler.c)
target_sources(app PRIVATE src/modules/led_controller.c)
target_sources(app PRIVATE src/audio/audio_stream.c)
target_sources(app PRIVATE src/audio/audio_in.c)

#DSP
target_sources(app PRIVATE src/audio/dsp/circular_block_buffer.c)
target_sources(app PRIVATE src/audio/dsp/rt_peak_detector.c)
target_sources(app PRIVATE src/audio/dsp/peak_validator.c)
target_sources(app PRIVATE src/audio/dsp/peak_processor.c)
target_sources(app PRIVATE src/audio/dsp/window_analysis.c)

target_sources(app PRIVATE src/ble/ble_manager.c)
target_sources(app PRIVATE src/ble/heart_service.c)

