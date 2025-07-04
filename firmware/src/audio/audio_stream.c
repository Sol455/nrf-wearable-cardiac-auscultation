#include "audio_stream.h"
#include "audio_in.h"
#include <zephyr/logging/log.h>
#include <stdio.h>

#define MEM_SLAB_BLOCK_COUNT 8
#define AUDIO_BUF_TOTAL_SIZE WAV_LENGTH_BLOCKS * MAX_BLOCK_SIZE

LOG_MODULE_REGISTER(audio_stream);
K_MSGQ_DEFINE(audio_input_message_queue, sizeof(audio_slab_msg), 8, 4);

struct k_msgq *audio_stream_get_msgq(void) {
    return &audio_input_message_queue;
}

//Debug audio transmission config via BLE
#if !IS_ENABLED(CONFIG_HEART_PATCH_DSP_MODE)
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

void write_to_buffer(const audio_slab_msg *msg)
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
#endif

void consume_audio() {
    audio_slab_msg msg;
    int ret = 0;

    while(1) {
        if (k_msgq_get(&audio_input_message_queue, &msg, K_FOREVER) == 0) {
            if (msg.msg_type == AUDIO_BLOCK_TYPE_DATA) {
                LOG_INF("Consumer: Received data %p\n", msg.buffer);
                #if !IS_ENABLED(CONFIG_HEART_PATCH_DSP_MODE)
                write_to_buffer(&msg);
                #endif
                ret = write_wav_data(msg.audio_output_file, msg.buffer, msg.size);
                //PROCESS AUDIO / DSP HERE:
                k_mem_slab_free(audio_in_get_mem_slab(), msg.buffer);
                if (ret != 0) {
                    LOG_ERR("Failed to write to file, rc=%d", ret);
                    return;
                }
            } else if (msg.msg_type == AUDIO_BLOCK_TYPE_STOP) {
                audio_in_stop();
            }

            LOG_INF("Written Data sucessfully");
            }
    }   
}

K_THREAD_DEFINE(subscriber_task_id, CONFIG_MAIN_STACK_SIZE, consume_audio, NULL, NULL, NULL, 3, 0, 0);