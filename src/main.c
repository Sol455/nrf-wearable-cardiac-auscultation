#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/sd_card.h"
#include "modules/button_handler.h"
#include "audio/wav_file.h"
#include "audio/audio_stream.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/audio/dmic.h>
#include "macros.h"

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(main);

#define MEM_SLAB_BLOCK_COUNT 8
#define WAV_LENGTH_BLOCKS 100

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, MEM_SLAB_BLOCK_COUNT, 4); //align mem slab to 4 bytes

struct save_wave_msg {
	void *buffer;
	size_t size;
	struct fs_file_t *audio_file
};

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
} AudioState;

AudioState audio_state = STATE_IDLE;
uint8_t writing = 0;

K_MSGQ_DEFINE(device_message_queue, sizeof(struct save_wave_msg), 8, 4);
volatile bool debounce_active = false;

//Thread 2
static void process_audio() {
		//wait for process message
		struct save_wave_msg msg;

		while(1) {
			if (k_msgq_get(&device_message_queue, &msg, K_FOREVER) == 0) {
				LOG_INF("Consumer: Received data %p\n", msg.buffer);
				/* Process the received data here */

				int ret = 0;
				if(ret!=0) {
					LOG_ERR("Failed to write wav header");
					//return 0;
				}
				ret = write_wav_data(msg.audio_file, msg.buffer, msg.size);

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

void sw0_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (audio_state == STATE_IDLE && !debounce_active) {
        audio_state = STATE_RECORDING;
        debounce_active = true;
    }
}

void sw1_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{

}

int main(void)
{
    int ret;
	LOG_INF("Turning on");

	ret = init_buttons(sw0_callback, sw1_callback);
	if(ret!=0) LOG_ERR("Buttons failed to init");

    ret = device_is_ready(led0.port);
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
	if(ret!=0) LOG_ERR("LEDs failed to init");
    LOG_INF("Configured Buttons & Interrupts");


	static struct fs_file_t wav_file;

	WavConfig wav_config = {
			.wav_file = &wav_file,
			.file_name = "testfile.wav",
			.length = WAV_LENGTH_BLOCKS * 3200,
			.sample_rate = MAX_SAMPLE_RATE,
			.bytes_per_sample = BYTES_PER_SAMPLE,
			.num_channels = NUM_CHANNELS,
	};

	AudioStream audio_stream = {
		.wav_config = wav_config,
		.dmic_ctx = DEVICE_DT_GET(DT_NODELABEL(pdm0)),
		.mem_slab = &mem_slab,
		.pdm_gain = NRF_PDM_GAIN_MAXIMUM
	};

    LOG_INF("BLOCK SIZE: %d\n", MAX_BLOCK_SIZE);

	ret = pdm_config(&audio_stream);
	if (ret != 0) LOG_ERR("pdm config failed", audio_stream.dmic_ctx->name);
	
    ret = sd_card_init();
	if(ret!=0) LOG_ERR("SD Failed to init");
	

    while(1) {
        switch (audio_state){
            case STATE_IDLE:
                gpio_pin_set_dt(&led0, 0);
                k_sleep(K_USEC(100));
                break;
            case STATE_RECORDING:
                debounce_active = false;
                gpio_pin_set_dt(&led0, 1);
                ret = open_wav_for_write(&wav_config);
                capture_audio(&audio_stream);
                audio_state = STATE_IDLE;
                break;
        }  
    }
	LOG_INF("Exiting");
	return 0;

}

K_THREAD_DEFINE(subscriber_task_id, CONFIG_MAIN_STACK_SIZE, process_audio, NULL, NULL, NULL, 3, 0, 0);