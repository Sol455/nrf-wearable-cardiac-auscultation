#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/sd_card.h"
#include "modules/button_handler.h"
#include "audio/wav_file.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/audio/dmic.h>
#include <nrfx_pdm.h>
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
int capture_audio(const struct device *dmic_dev, size_t block_count, struct fs_file_t *audio_file)
{
	int ret;
	struct save_wave_msg msg;

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
		if (ret < 0) {
			LOG_ERR("START trigger failed: %d", ret);
			return ret;
		}

	for (int  i = 0; i < block_count; i ++) {
		ret = dmic_read(dmic_dev, 0, &msg.buffer, &msg.size, READ_TIMEOUT);
		if (ret < 0) {
			LOG_ERR("%d - read failed: %d", i, ret);
			return ret;
		}
		msg.audio_file = audio_file;
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
			stop_capture(dmic_dev, audio_file);
			break;
		} else  {
			k_sleep(K_MSEC(100));
		}
	}

	LOG_INF("Audio Capture Finished");

	return ret;
}

int pwm_config(const struct device *dmic_dev, uint8_t gain) {

		if (!device_is_ready(dmic_dev)) {
			LOG_ERR("%s is not ready", dmic_dev->name);
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

		int ret = dmic_configure(dmic_dev, &cfg);
		if (ret < 0) {
			LOG_ERR("Failed to configure the driver: %d", ret);
			return ret;
		}
		nrf_pdm_gain_set(NRF_PDM0_S, gain, gain);
		uint8_t l_gain, r_gain;
		nrf_pdm_gain_get(NRF_PDM0_S, &l_gain, &r_gain);
		LOG_INF("LEFT GAIN: %d, RIGHT GAIN: %d", l_gain, r_gain);
		return 0;
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
    ret = device_is_ready(led0.port);
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
	if(ret!=0) LOG_ERR("Buttons and LEDs failed to init");
    LOG_INF("Configured Buttons & Interrupts");


    LOG_INF("BLOCK SIZE: %d\n", MAX_BLOCK_SIZE);

    const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm0));    
	ret = pwm_config(dmic_dev, NRF_PDM_GAIN_MAXIMUM);
	if (ret != 0) LOG_ERR("CONFIG FAILED", dmic_dev->name);
	
    ret = sd_card_init();
	if(ret!=0) LOG_ERR("SD Failed to init");
	
	static struct fs_file_t wav_file;

	WavConfig wav_config = {
			.wav_file = &wav_file,
			.file_name = "test_file.wav",
			.length = WAV_LENGTH_BLOCKS * 3200,
			.sample_rate = MAX_SAMPLE_RATE,
			.bytes_per_sample = BYTES_PER_SAMPLE,
			.num_channels = NUM_CHANNELS,
	};

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
                capture_audio(dmic_dev, WAV_LENGTH_BLOCKS, &wav_file);
                audio_state = STATE_IDLE;
                break;
        }  
    }
	LOG_INF("Exiting");
	return 0;

}

K_THREAD_DEFINE(subscriber_task_id, CONFIG_MAIN_STACK_SIZE, process_audio, NULL, NULL, NULL, 3, 0, 0);