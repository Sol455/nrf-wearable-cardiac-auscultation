#include "circular_block_buffer.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(circ_buffer);

void cbb_init(CircularBlockBuffer *buf, uint32_t num_blocks, uint32_t block_size) {
    buf->num_blocks = num_blocks;
    buf->block_size = block_size;
    buf->write_index = 0;
    buf->absolute_sample_index = 0;
    memset(buf->buffer, 0, sizeof(buf->buffer));
}

float* cbb_get_write_block(CircularBlockBuffer *buf) {
    return buf->buffer[buf->write_index];
}

void cbb_advance_write_index(CircularBlockBuffer *buf) {
    buf->write_index = (buf->write_index + 1) % buf->num_blocks;
    buf->absolute_sample_index += buf->block_size;
}

size_t cbb_get_absolute_sample_index(const CircularBlockBuffer *buf) {
    return buf->absolute_sample_index;
}

size_t cbb_get_block_size(const CircularBlockBuffer *buf) {
    return buf->block_size;
}

int cbb_extract_window(const CircularBlockBuffer *buf, uint32_t start_idx, uint32_t end_idx, int32_t pre_samples, int32_t post_samples, float *out_window) {
    int32_t start = (int32_t)start_idx - pre_samples;
    int32_t end = (int32_t)end_idx + post_samples;
    if (start < 0 || end < 0) {
        LOG_ERR("Negative start or end indexes start: %d, end: %d", start, end);
        return -1;
    }
    int32_t window_len = end - start;
    if (window_len <= 0) {
        LOG_ERR("Attempting to extract 0 length or negative window");
        return -1;
    }
    uint32_t capacity = buf->num_blocks * buf->block_size;
    uint32_t latest = buf->absolute_sample_index;
    if ((latest - start > capacity) || (latest - end > capacity)) {
        LOG_ERR("exceeds capacity latest - start: %d, latest - end: %d ", latest-start, latest- end);
        return -1;
    }

    for (int i = 0; i < window_len; ) {
        uint32_t abs_idx = start + i;
        uint32_t rel_idx = abs_idx % capacity;
        uint32_t block_idx = rel_idx / buf->block_size;
        uint32_t sample_idx = rel_idx % buf->block_size;

        uint32_t samples_left_in_block = buf->block_size - sample_idx;
        uint32_t samples_left_in_window = window_len - i;
        //Copy out either samples to end of block or samples left in window
        uint32_t n_to_copy = (samples_left_in_block < samples_left_in_window) ? samples_left_in_block: samples_left_in_window;

        memcpy(&out_window[i], &buf->buffer[block_idx][sample_idx], n_to_copy * sizeof(float));
        i += n_to_copy;
    }
    return 0; 
}

