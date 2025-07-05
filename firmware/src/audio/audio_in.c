#include "audio_in.h"
#include <zephyr/logging/log.h>
#include <stdio.h>
#include "../modules/sd_card.h"

#define PDM_MEM_SLAB_BLOCK_COUNT 8

LOG_MODULE_REGISTER(audio_in);

K_MEM_SLAB_DEFINE(pdm_mem_slab, MAX_BLOCK_SIZE, PDM_MEM_SLAB_BLOCK_COUNT, 4); //align mem slab to 4 bytes
int16_t _wav_input_buffer[BLOCK_SIZE_SAMPLES];

static AudioInConfig _audio_in_config; 

struct k_mem_slab *audio_in_get_mem_slab(void) {
    return &pdm_mem_slab;
}

int pdm_init() {
    if (!device_is_ready(_audio_in_config.dmic_ctx)) {
        LOG_ERR("%s is not ready", _audio_in_config.dmic_ctx->name);
        return 0;
    }

    struct pcm_stream_cfg stream = {
        .pcm_width = SAMPLE_BIT_WIDTH,
        .mem_slab  = &pdm_mem_slab,
    };

    struct dmic_cfg cfg = {
        .io = {
            .min_pdm_clk_freq = 1000000,
            .max_pdm_clk_freq = 3500000,
            .min_pdm_clk_dc   = 40,
            .max_pdm_clk_dc   = 60,
        },
        .streams = &stream,
        .channel = {
            .req_num_streams = 1,
        },
    };

    cfg.channel.req_num_chan = 1;
    cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
    cfg.streams[0].pcm_rate = MAX_SAMPLE_RATE;
    cfg.streams[0].block_size = BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);

    int ret = dmic_configure(_audio_in_config.dmic_ctx, &cfg);
    if (ret < 0) {
        LOG_ERR("Failed to configure the driver: %d", ret);
        return ret;
    }
    nrf_pdm_gain_set(NRF_PDM0_S, _audio_in_config.pdm_gain, _audio_in_config.pdm_gain);
    uint8_t l_gain, r_gain;
    nrf_pdm_gain_get(NRF_PDM0_S, &l_gain, &r_gain);
    LOG_INF("LEFT GAIN: %d, RIGHT GAIN: %d", l_gain, r_gain);
    return 0;
}

int pdm_stop() {
    int ret = dmic_trigger(_audio_in_config.dmic_ctx, DMIC_TRIGGER_STOP);
    if (ret < 0) {
        LOG_ERR("STOP trigger failed: %d", ret);
    }
    return ret;
}

int pdm_capture_audio() {
    int ret;
    audio_slab_msg msg;

    ret = dmic_trigger(_audio_in_config.dmic_ctx, DMIC_TRIGGER_START);
        if (ret < 0) {
            LOG_ERR("START trigger failed: %d", ret);
            return ret;
        }

    for (int  i = 0; i < WAV_LENGTH_BLOCKS; i ++) {
        ret = dmic_read(_audio_in_config.dmic_ctx, 0, &msg.buffer, &msg.size, READ_TIMEOUT);
        if (_audio_in_config.audio_input_type==AUDIO_INPUT_TYPE_PDM_TO_WAV) {
            msg.audio_output_file = _audio_in_config.output_wav_config.wav_file;
        }
        if (ret < 0) {
            LOG_ERR("%d - read failed: %d", i, ret);
            return ret;
        }
        msg.msg_type = AUDIO_BLOCK_TYPE_DATA;
        LOG_INF("%d - got buffer %p of %u bytes", i, msg.buffer, msg.size);
        ret = k_msgq_put(_audio_in_config.msgq, &msg, K_NO_WAIT);
        if (ret < 0) {
        LOG_ERR("Failed to enqueue message: %d", ret);
        return ret;
        }
        
    }
    msg.msg_type = AUDIO_BLOCK_TYPE_STOP;
    ret = k_msgq_put(_audio_in_config.msgq, &msg, K_FOREVER);
    if (ret < 0) {
        LOG_ERR("Failed to stop message: %d", ret);
        return ret;
    }
    LOG_INF("Sent stop message: %d", ret);

    return ret;

}
void wav_file_capture_audio() {
    int block_count = 0, total_samples = 0, samples_read = 0, ret = 0;
    audio_slab_msg msg;

    while ((samples_read = read_wav_block(&_audio_in_config.input_wav_config,
                                          _wav_input_buffer, BLOCK_SIZE_SAMPLES)) > 0) {
        msg.msg_type = AUDIO_BLOCK_TYPE_DATA;
        msg.buffer = _wav_input_buffer;
        msg.size = samples_read * sizeof(int16_t);

        ret = k_msgq_put(_audio_in_config.msgq, &msg, K_FOREVER);
        block_count++;
        total_samples += samples_read;
        //LOG_INF("Block %d: %d samples read, total: %d", block_count, samples_read, total_samples);
        uint32_t block_ms = (samples_read * 1000) / _audio_in_config.input_wav_config.sample_rate;
        k_msleep(block_ms);
    }
    // Send STOP message
    audio_slab_msg stop_msg = { .msg_type = AUDIO_BLOCK_TYPE_STOP };
    k_msgq_put(_audio_in_config.msgq, &stop_msg, K_FOREVER);
    LOG_INF("Processed %d blocks, %d samples total from WAV file", block_count, total_samples);
}

void generate_wav_filename() {
    static uint32_t file_counter = 0;
    snprintf(_audio_in_config.output_wav_config.file_name, 
             sizeof(_audio_in_config.output_wav_config.file_name),
             "%08u.wav", file_counter++);
}

int audio_in_init(AudioInConfig audio_in_config) {
    _audio_in_config = audio_in_config;
    switch (_audio_in_config.audio_input_type) {
        
        case AUDIO_INPUT_TYPE_PDM:
            return pdm_init();
        case AUDIO_INPUT_TYPE_PDM_TO_WAV:
            pdm_init();
            return -1;
        case AUDIO_INPUT_TYPE_WAV:
            return 0;
        default:
            LOG_ERR("Unknown audio input type!");
            return -1;
    }
    LOG_INF("Init audio input");

}

int audio_in_start() {
    int ret;
    switch (_audio_in_config.audio_input_type) {
        case AUDIO_INPUT_TYPE_PDM :
            ret = pdm_capture_audio();
            break;
        case AUDIO_INPUT_TYPE_PDM_TO_WAV:
            generate_wav_filename();
            open_wav_for_write(&_audio_in_config.output_wav_config);
            ret = pdm_capture_audio();
            break;
        case AUDIO_INPUT_TYPE_WAV:
            ret = open_wav_for_read(&_audio_in_config.input_wav_config);
            wav_file_capture_audio();
            break;
        default:
            LOG_ERR("Unknown audio input type!");
            return -1;
    }
    return ret;
}

int audio_in_stop() {
    int ret = 0;
    switch (_audio_in_config.audio_input_type) {
        case AUDIO_INPUT_TYPE_PDM:
            ret = pdm_stop();
            return ret;
        case AUDIO_INPUT_TYPE_PDM_TO_WAV:
            ret = pdm_stop();
            ret = sd_card_close(_audio_in_config.output_wav_config.wav_file);
            if (ret < 0) {
                LOG_ERR("SD failed to close: %d", ret);
            }
            return ret;
        case AUDIO_INPUT_TYPE_WAV:
            ret = sd_card_close(_audio_in_config.input_wav_config.wav_file);
            if (ret < 0) {
                LOG_ERR("SD failed to close: %d", ret);
            }
            return ret;
        default:
            LOG_ERR("Unknown audio input type!");
            return -1;
    }
}

