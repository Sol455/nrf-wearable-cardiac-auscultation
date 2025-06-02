#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/sd_card.h"
#include "audio/wav_file.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/audio/dmic.h>
#include <nrfx_pdm.h>

#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec sw1= GPIO_DT_SPEC_GET(SW1_NODE, gpios);

static struct gpio_callback sw0_cb;
static struct gpio_callback sw1_cb;

LOG_MODULE_REGISTER(main);

//Mek slab for I2s Audio Driver
#define MAX_SAMPLE_RATE  16000
#define SAMPLE_BIT_WIDTH 16
#define BYTES_PER_SAMPLE 2
#define NUM_CHANNELS 1
#define READ_TIMEOUT     1000

#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
(BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

#define MAX_BLOCK_SIZE  BLOCK_SIZE(MAX_SAMPLE_RATE, 2)
#define BLOCK_COUNT 8
#define WAV_LENGTH_BLOCKS 100

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 4);

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
	int write_counter = 0;
	struct save_wave_msg msg;

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
		if (ret < 0) {
			LOG_ERR("START trigger failed: %d", ret);
			return ret;
		}
	//start recording blocks ---->
	while(write_counter < block_count) {
		ret = dmic_read(dmic_dev, 0, &msg.buffer, &msg.size, READ_TIMEOUT);
		if (ret < 0) {
			LOG_ERR("%d - read failed: %d", write_counter, ret);
			return ret;
		}

		msg.audio_file = audio_file;
		LOG_INF("%d - got buffer %p of %u bytes", write_counter, msg.buffer, msg.size);
		//send message with buffers and pointers here --->

		writing = 1;
		ret = k_msgq_put(&device_message_queue, &msg, K_NO_WAIT);

		if (ret < 0) {
    	LOG_ERR("Failed to enqueue message: %d", ret);
    	return ret;
		}
		//---> pub sub send---->>
		write_counter++;
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
    ret = device_is_ready(led0.port);
    ret = device_is_ready(sw0.port);
    ret = device_is_ready(sw1.port);
    if (!ret) {
        LOG_WRN("LED or Button GPIO port not ready!\n");
        return 0;
    }
    gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
    gpio_pin_configure_dt(&sw0, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&sw0, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&sw0_cb, sw0_callback, BIT(sw0.pin));
    gpio_add_callback(sw0.port, &sw0_cb);
    gpio_pin_configure_dt(&sw1, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&sw1, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&sw1_cb, sw1_callback, BIT(sw1.pin));
    gpio_add_callback(sw1.port, &sw1_cb);

    LOG_INF("Configured Buttons & Interrupts");
    LOG_INF("BLOCK SIZE: %d\n", MAX_BLOCK_SIZE);

    const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm0));    

	ret = pwm_config(dmic_dev, NRF_PDM_GAIN_MAXIMUM);
	if (ret != 0) {
		LOG_ERR("CONFIG FAILED", dmic_dev->name);
	}

    ret = sd_card_init();
	if(ret!=0) {
		LOG_ERR("SD Failed to init");
	}

	//k_sem_init(&write_done, 0, 1);
	
	static struct fs_file_t wav_file;
	WavConfig wav_config;
	wav_config.wav_file = &wav_file;
	wav_config.file_name = "april.wav";
	wav_config.length = WAV_LENGTH_BLOCKS * 3200;
	wav_config.sample_rate = MAX_SAMPLE_RATE;
	wav_config.bytes_per_sample = BYTES_PER_SAMPLE;
	wav_config.num_channels = NUM_CHANNELS;

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