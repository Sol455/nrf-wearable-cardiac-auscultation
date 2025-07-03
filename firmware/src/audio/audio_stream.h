#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include "../modules/sd_card.h"
#include "wav_file.h"
#include <nrfx_pdm.h>
#include "../macros.h"

typedef struct {
    struct k_mem_slab *mem_slab; 
} AudioStreamConfig;

// struct save_wave_msg {
// 	void *buffer;
// 	size_t size;
// 	struct fs_file_t *audio_file;
// };

struct k_msgq *audio_stream_get_msgq(void);
//int pdm_init(AudioStream * audio_stream);

//int capture_audio(AudioStream *audio_stream);

void consume_audio();

//void producer_capture_audio_from_wav(WavConfig *wav_config);

//Audio Transmission Functions
const int16_t *get_audio_buffer(void);          
size_t get_audio_buffer_length(void);

#endif