#ifndef _MACROS_H_
#define _MACROS_H_

//Device Tree Aliases:
#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)

#define MAX_SAMPLE_RATE  16000
#define SAMPLE_BIT_WIDTH 16
#define BYTES_PER_SAMPLE 2
#define NUM_CHANNELS 1
#define READ_TIMEOUT 1000
#define WAV_LENGTH_BLOCKS 50

#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
(BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

#define MAX_BLOCK_SIZE  BLOCK_SIZE(MAX_SAMPLE_RATE, NUM_CHANNELS)
#define BLOCK_SIZE_SAMPLES MAX_BLOCK_SIZE / 2

#define MAX_FILENAME_LEN 16

#endif