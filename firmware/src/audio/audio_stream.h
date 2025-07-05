#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include "../modules/sd_card.h"
#include "wav_file.h"
#include <nrfx_pdm.h>
#include "../macros.h"
#include "dsp/rt_peak_detector.h"

typedef struct {
    struct k_mem_slab *mem_slab; 
    RTPeakConfig rt_peak_config;
} AudioStreamConfig;

void init_audio_stream(AudioStreamConfig audio_stream_config);

struct k_msgq *audio_stream_get_msgq();
void consume_audio();

//Audio Transmission Functions
const int16_t *get_audio_buffer();          
size_t get_audio_buffer_length();

#endif