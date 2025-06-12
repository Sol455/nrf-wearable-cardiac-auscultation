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

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
} AudioState;

AudioState audio_state = STATE_IDLE;

volatile bool debounce_active = false;

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
		.pdm_gain = NRF_PDM_GAIN_MAXIMUM
	};

    LOG_INF("BLOCK SIZE: %d\n", MAX_BLOCK_SIZE);

	ret = pdm_init(&audio_stream);
	if (ret != 0) LOG_ERR("pdm config failed");
	
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

