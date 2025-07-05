#ifndef _CIRCULAR_BLOCK_BUFFER_H_
#define _CIRCULAR_BLOCK_BUFFER_H_

#include "../../macros.h"
#include <zephyr/kernel.h>

typedef struct {
    float buffer[CB_NUM_BLOCKS][BLOCK_SIZE_SAMPLES];
    uint32_t num_blocks;
    uint32_t block_size;
    uint32_t write_index;
    uint32_t absolute_sample_index;
} CircularBlockBuffer;

//Init the buffer
void cbb_init(CircularBlockBuffer *buf, uint32_t num_blocks, uint32_t block_size);

//Get pointer to next writable block
float* cbb_get_write_block(CircularBlockBuffer *buf);

//Advance write index 
void cbb_advance_write_index(CircularBlockBuffer *buf);

//Get absolute sample index of latest samples written
uint32_t cbb_get_absolute_sample_index(const CircularBlockBuffer *buf);

//Get the size of the block for this buffer
uint32_t cbb_get_block_size(const CircularBlockBuffer *buf);

//Extract window [start_abs_idx, end_abs_idx] into out_window
int cbb_extract_window(const CircularBlockBuffer *buf, uint32_t start_idx, uint32_t end_idx, int32_t pre_samples, int32_t post_samples, float *out_window);

#endif