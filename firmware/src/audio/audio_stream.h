#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include "../modules/sd_card.h"
#include "wav_file.h"
#include <nrfx_pdm.h>
#include "../macros.h"

typedef struct {
    WavConfig wav_config;
    const struct device *const dmic_ctx;
    //struct k_mem_slab *mem_slab; 
    nrf_pdm_gain_t pdm_gain;
} AudioStream;

struct save_wave_msg {
	void *buffer;
	size_t size;
	struct fs_file_t *audio_file
};

int pdm_init(AudioStream * audio_stream);

int capture_audio(AudioStream *audio_stream);

void process_audio();

void generate_filename(char *filename_out, size_t max_len);

void capture_audio_from_wav(WavConfig *wav_config);

//Audio Transmission Functions
const int16_t *get_audio_buffer(void);          
size_t get_audio_buffer_length(void);

#endif