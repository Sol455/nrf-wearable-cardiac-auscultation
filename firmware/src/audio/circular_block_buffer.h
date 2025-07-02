#ifndef _CIRCULAR_BLOCK_BUFFER_H_
#define _CIRCULAR_BLOCK_BUFFER_H_

#include "../macros.h"
#include <zephyr/kernel.h>

typedef struct {
    float buffer[CB_NUM_BLOCKS][MAX_BLOCK_SIZE];
    size_t num_blocks;
    size_t block_size;
    size_t write_index;
    size_t absolute_sample_index;
} CircularBlockBuffer;

//Init the buffer
void cbb_init(CircularBlockBuffer *buf, size_t num_blocks, size_t block_size);

//Get pointer to next writable block
float* cbb_get_write_block(CircularBlockBuffer *buf);

//Advance write index 
void cbb_advance_write_index(CircularBlockBuffer *buf);

//Get absolute sample index of latest samples written
size_t cbb_get_absolute_sample_index(const CircularBlockBuffer *buf);

//Get the size of the block for this buffer
size_t cbb_get_block_size(const CircularBlockBuffer *buf);

//Extract window [start_abs_idx, end_abs_idx) into out_window
int cbb_extract_window(const CircularBlockBuffer *buf, size_t start_abs_idx, size_t end_abs_idx, float *out_window);
#endif