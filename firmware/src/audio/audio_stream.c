#include <zephyr/logging/log.h>
#include <stdio.h>
#include "audio_stream.h"
#include "audio_in.h"
#include "arm_math.h"
#include "dsp/filters/bandpass_coeffs.h"
#include "dsp/filters/lowpass_coeffs.h"
#include "dsp/circular_block_buffer.h"

#define MEM_SLAB_BLOCK_COUNT 8
#define AUDIO_BUF_TOTAL_SIZE WAV_LENGTH_BLOCKS * MAX_BLOCK_SIZE

#define PEAK_PROCESSING_STACK_SIZE CONFIG_MAIN_STACK_SIZE
#define PEAK_PROCESSING_PRIORITY 5

LOG_MODULE_REGISTER(audio_stream);
K_MSGQ_DEFINE(audio_input_message_queue, sizeof(audio_slab_msg), 8, 4);
K_MSGQ_DEFINE(peak_message_queue, sizeof(RTPeakMessage), 8, 4);

AudioStreamConfig _audio_stream_config;

CircularBlockBuffer _block_buffer;
RTPeakDetector _rt_peak_detector;

struct k_msgq *audio_stream_get_msgq() {
    return &audio_input_message_queue;
}

struct k_msgq *audio_stream_get_peak_msgq() {
    return &peak_message_queue;
}

float32_t bp_state[4 * NUM_STAGES_BP];
arm_biquad_casd_df1_inst_f32 bp_inst;

float32_t lp_state[4 * NUM_STAGES_LP];
arm_biquad_casd_df1_inst_f32 lp_inst;

void init_filters() {
    arm_biquad_cascade_df1_init_f32(
        &bp_inst,
        NUM_STAGES_BP,
        bandpass_coeffs,
        bp_state
    );
    arm_biquad_cascade_df1_init_f32(
        &lp_inst,
        NUM_STAGES_LP,
        lowpass_coeffs,
        lp_state
    );
}

void init_audio_stream(AudioStreamConfig audio_stream_config) {
    _audio_stream_config = audio_stream_config;
    init_filters();
    cbb_init(&_block_buffer, CB_NUM_BLOCKS, BLOCK_SIZE_SAMPLES);
    //rt_peak_detector_init(&_rt_peak_detector, BLOCK_SIZE_SAMPLES, CB_NUM_BLOCKS, 0.0001, 2.0, 2000);
    rt_peak_detector_init(&_rt_peak_detector, &_audio_stream_config.rt_peak_config);
}

float32_t f32_buf[BLOCK_SIZE_SAMPLES];
int16_t out_q15[BLOCK_SIZE_SAMPLES];
float32_t envelope_buf[BLOCK_SIZE_SAMPLES];

int debug_peak_count = 0;

void _process_block(audio_slab_msg *msg) {

    int ret = 0;
    #if !IS_ENABLED(CONFIG_HEART_PATCH_DSP_MODE)
        write_to_buffer(&msg);
    #endif
    //1. Write filtered audio to ring buffer
    float *block_to_write = cbb_get_write_block(&_block_buffer);
    arm_q15_to_float((int16_t *)msg->buffer, f32_buf, BLOCK_SIZE_SAMPLES); //Convert to F32
    arm_biquad_cascade_df1_f32(&bp_inst, f32_buf, block_to_write, BLOCK_SIZE_SAMPLES); // Filter into slab buffer
    cbb_advance_write_index(&_block_buffer); //Advance slab buffer index for next run

    // arm_float_to_q15(f32_buf, out_q15, BLOCK_SIZE_SAMPLES); // Convert back to int for saving to file
    // ret = write_wav_data(msg->audio_output_file, (const char *)out_q15, msg->size); // write to wav file

    //2. Generate envelope
    arm_abs_f32(block_to_write, envelope_buf, BLOCK_SIZE_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp_inst, envelope_buf, envelope_buf, BLOCK_SIZE_SAMPLES);

    //3. Peak Detection
    int32_t block_absolute_start = cbb_get_absolute_sample_index(&_block_buffer) - cbb_get_block_size(&_block_buffer);
    //LOG_INF("Block start absolute idx: %d", block_absolute_start);

    for (int i = 0; i < BLOCK_SIZE_SAMPLES; i++) {

        int32_t abs_idx_of_sample = block_absolute_start + i;
        RTPeakMessage peak_msg;
        bool found = rt_peak_detector_update(&_rt_peak_detector, envelope_buf[i], abs_idx_of_sample, &peak_msg);
        if (found) {
            debug_peak_count++;
            LOG_INF("Peak at global idx %d, value %f, running peak_total: %d", peak_msg.global_index, (double)peak_msg.value, debug_peak_count);
        }
    }


    //k_mem_slab_free(audio_in_get_mem_slab(), msg->buffer);
    if (ret != 0) {
        LOG_ERR("Failed to write to file, rc=%d", ret);
        return;
    }
}

void consume_audio() {
    audio_slab_msg msg;
    int ret = 0;

    while(1) {
        if (k_msgq_get(&audio_input_message_queue, &msg, K_FOREVER) == 0) {
            if (msg.msg_type == AUDIO_BLOCK_TYPE_DATA) {
                //LOG_INF("Consumer: Received data %p\n", msg.buffer);
                _process_block(&msg);
            } else if (msg.msg_type == AUDIO_BLOCK_TYPE_STOP) {
                audio_in_stop();
            }

            //LOG_INF("Written Data sucessfully");
            }
    }   
}

void process_peaks() { 
    RTPeakMessage msg;
    int ret = 0;

    while(1) {
        if (k_msgq_get(&peak_message_queue, &msg, K_FOREVER) == 0) {
            //do something
        }
    }   
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

K_THREAD_DEFINE(subscriber_task_id, CONFIG_MAIN_STACK_SIZE, consume_audio, NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(peak_processing_thread_id, PEAK_PROCESSING_STACK_SIZE, process_peaks,  NULL, NULL, NULL, PEAK_PROCESSING_PRIORITY, 0, 0);