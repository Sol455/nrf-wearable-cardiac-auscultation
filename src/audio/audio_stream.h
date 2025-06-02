#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include "wav_file.h"
#include <nrfx_pdm.h>
#include "../macros.h"

typedef struct {
    WavConfig wav_config;
    const struct device *const dmic_ctx;
    struct k_mem_slab *mem_slab; 
    nrf_pdm_gain_t pdm_gain;
} AudioStream;

int pdm_config(AudioStream * audio_stream);

#endif