#include "audio_stream.h"
#include <zephyr/logging/log.h>
#include <stdio.h>

#define MEM_SLAB_BLOCK_COUNT 8
#define AUDIO_BUF_TOTAL_SIZE WAV_LENGTH_BLOCKS * MAX_BLOCK_SIZE

LOG_MODULE_REGISTER(audio_stream);
K_MSGQ_DEFINE(device_message_queue, sizeof(struct save_wave_msg), 8, 4);

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, MEM_SLAB_BLOCK_COUNT, 4); //align mem slab to 4 bytes

uint8_t writing = 0;

static int16_t audio_buf[AUDIO_BUF_TOTAL_SIZE];
static size_t audio_buf_offset = 0;

const int16_t *get_audio_buffer(void)
{
	return audio_buf;
}

size_t get_audio_buffer_length(void)
{
	return audio_buf_offset;
}

void write_to_buffer(const struct save_wave_msg *msg)
{
    size_t num_samples = msg->size / sizeof(int16_t);
    const int16_t *src = (const int16_t *)msg->buffer;

    if ((audio_buf_offset + num_samples) > AUDIO_BUF_TOTAL_SIZE) {
        LOG_ERR("Audio buffer overflow — dropping %u bytes", msg->size);
        return;
    }

    memcpy(&audio_buf[audio_buf_offset], src, msg->size);
    audio_buf_offset += num_samples;

    LOG_INF("Wrote %u int16_t samples to audio_buf (offset now %u)", num_samples, audio_buf_offset);
}

int pdm_init(AudioStream * audio_stream) {
    if (!device_is_ready(audio_stream->dmic_ctx)) {
        LOG_ERR("%s is not ready", audio_stream->dmic_ctx);
        return 0;
    }

    struct pcm_stream_cfg stream = {
        .pcm_width = SAMPLE_BIT_WIDTH,
        .mem_slab  = &mem_slab,
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

void process_audio() {
    struct save_wave_msg msg;

    while(1) {
        if (k_msgq_get(&device_message_queue, &msg, K_FOREVER) == 0) {
            LOG_INF("Consumer: Received data %p\n", msg.buffer);
            int ret = 0;
            if(ret!=0) {
                LOG_ERR("Failed to write wav header");
            }

            write_to_buffer(&msg);
            ret = write_wav_data(msg.audio_file, msg.buffer, msg.size);
            //PROCESS AUDIO / DSP HERE:
            //

            k_mem_slab_free(&mem_slab, msg.buffer);
            if (ret != 0) {
                LOG_ERR("Failed to write to file, rc=%d", ret);
                return;
            }
            LOG_INF("Written Data sucessfully");
            writing = 0;

        }
    }
}

void stop_capture(struct device *dmic_dev, struct fs_file_t *audio_file) {
    int ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
    if (ret < 0) {
        LOG_ERR("STOP trigger failed: %d", ret);
    }
    sd_card_close(audio_file);
}

//main thread
int capture_audio(AudioStream *audio_stream)
{
int ret;
struct save_wave_msg msg;

ret = dmic_trigger(audio_stream->dmic_ctx, DMIC_TRIGGER_START);
    if (ret < 0) {
        LOG_ERR("START trigger failed: %d", ret);
        return ret;
    }

for (int  i = 0; i < WAV_LENGTH_BLOCKS; i ++) {
    ret = dmic_read(audio_stream->dmic_ctx, 0, &msg.buffer, &msg.size, READ_TIMEOUT);
    if (ret < 0) {
        LOG_ERR("%d - read failed: %d", i, ret);
        return ret;
    }
    msg.audio_file = audio_stream->wav_config.wav_file;
    LOG_INF("%d - got buffer %p of %u bytes", i, msg.buffer, msg.size);
    writing = 1;
    ret = k_msgq_put(&device_message_queue, &msg, K_NO_WAIT);

    if (ret < 0) {
    LOG_ERR("Failed to enqueue message: %d", ret);
    return ret;
    }
}

//@TO-DO, get rid of this, remove global writing flag
while (1) {
    if (writing == 0){
        stop_capture(audio_stream->dmic_ctx, audio_stream->wav_config.wav_file);
        break;
    } else  {
        k_sleep(K_MSEC(100));
    }
}

LOG_INF("Audio Capture Finished");

return ret;
}

void generate_filename(char *filename_out, size_t max_len) {
    static uint32_t file_counter = 0;
    snprintf(filename_out, max_len, "%08u.wav", file_counter++);}

K_THREAD_DEFINE(subscriber_task_id, CONFIG_MAIN_STACK_SIZE, process_audio, NULL, NULL, NULL, 3, 0, 0);