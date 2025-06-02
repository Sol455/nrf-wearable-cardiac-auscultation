#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <zephyr/kernel.h>

typedef struct {
    const struct device *dmic_dev;
    struct fs_file_t *audio_file;
    struct k_sem write_complete;
    size_t total_blocks;
} AudioSession;

#endif