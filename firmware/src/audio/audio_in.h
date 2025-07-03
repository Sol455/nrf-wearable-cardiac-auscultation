#ifndef _AUDIO_IN_H_
#define _AUDIO_IN_H_

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <nrfx_pdm.h>
#include <zephyr/audio/dmic.h>
#include "wav_file.h"
#include "../macros.h"

typedef enum {
    AUDIO_INPUT_TYPE_PDM,
    AUDIO_INPUT_TYPE_PDM_TO_WAV,
    AUDIO_INPUT_TYPE_WAV,
} AudioInputType;

typedef struct {
    AudioInputType audio_input_type;
    WavConfig input_wav_config;
    const struct device *dmic_ctx;
    nrf_pdm_gain_t pdm_gain;
    WavConfig output_wav_config;
    struct k_msgq *msgq;
} AudioInConfig;

typedef enum {
    AUDIO_BLOCK_TYPE_DATA,
    AUDIO_BLOCK_TYPE_STOP
} audio_block_type_t;

typedef struct {
	void *buffer;
	size_t size;
    audio_block_type_t msg_type;
    // Optional fields
    struct fs_file_t *audio_output_file; //Only needed if saving to SD
} audio_slab_msg;

struct k_mem_slab *audio_in_get_mem_slab(void);
int audio_in_init(AudioInConfig audio_in_config);
int pdm_init();
void pdm_stop();
int audio_in_start();

#endif