#include "audio_stream.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(audio_stream);

int pdm_config(AudioStream * audio_stream) {
    if (!device_is_ready(audio_stream->dmic_ctx)) {
        LOG_ERR("%s is not ready", audio_stream->dmic_ctx);
        return 0;
    }

    struct pcm_stream_cfg stream = {
        .pcm_width = SAMPLE_BIT_WIDTH,
        .mem_slab  = audio_stream->mem_slab,
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
    cfg.channel.req_chan_map_lo =
        dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
    cfg.streams[0].pcm_rate = MAX_SAMPLE_RATE;
    cfg.streams[0].block_size = BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);

    int ret = dmic_configure(audio_stream->dmic_ctx, &cfg);
    if (ret < 0) {
        LOG_ERR("Failed to configure the driver: %d", ret);
        return ret;
    }
    nrf_pdm_gain_set(NRF_PDM0_S, audio_stream->pdm_gain, audio_stream->pdm_gain);
    uint8_t l_gain, r_gain;
    nrf_pdm_gain_get(NRF_PDM0_S, &l_gain, &r_gain);
    LOG_INF("LEFT GAIN: %d, RIGHT GAIN: %d", l_gain, r_gain);
    return 0;
}