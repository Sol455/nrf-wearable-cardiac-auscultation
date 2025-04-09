#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/sd_card.h"
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>

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
#define SAMPLE_BIT_WIDTH 24
#define BYTES_PER_SAMPLE 3
#define READ_TIMEOUT     1000

#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
(BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

#define MAX_BLOCK_SIZE  BLOCK_SIZE(MAX_SAMPLE_RATE, 1)
#define BLOCK_COUNT 25
#define WAV_LENGTH_BLOCKS 100

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 32); //align to 32 bytes in memory

void *audio_buffers[BLOCK_COUNT];  // Array to store pointers to each block for initial SD tests
size_t buffer_sizes[BLOCK_COUNT];

void sw0_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&led0);
}

void sw1_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&led0);
}

int config_i2s_stream(const struct device *i2s_dev) {
    struct i2s_config i2s_cfg;
	i2s_cfg.word_size = SAMPLE_BIT_WIDTH; 
	i2s_cfg.channels = 1; //Mono
	i2s_cfg.format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
	i2s_cfg.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
	i2s_cfg.frame_clk_freq = MAX_SAMPLE_RATE;
	i2s_cfg.mem_slab = &mem_slab;
	i2s_cfg.block_size = MAX_BLOCK_SIZE;
	i2s_cfg.timeout = READ_TIMEOUT;

    int ret = i2s_configure(i2s_dev, I2S_DIR_RX, &i2s_cfg);
	if (ret < 0) {
		LOG_WRN("Failed to configure the I2S stream: (%d)\n", ret);
        return -1;
    }
    return 0;
}

static struct fs_file_t wav_file;
int slab_counter = 0;

void write_wav() {

	fs_file_t_init(&wav_file);

	int ret = sd_card_open_for_write("poortaa.wav", &wav_file);
		if (ret!= 0) {
			LOG_ERR("Failed to open file, rc=%d", ret);
			//return err;	
			} else {
			LOG_INF("Opened SD card sucessfully");

			ret = write_wav_header(
				&wav_file,
				slab_counter * MAX_BLOCK_SIZE,
				//16000, // hard coded header because fs_seek doesn't seem to be working.
				MAX_SAMPLE_RATE, // bugbug: somehow the lc3 decoded data is
				// returning twice as much data as expected...?
				3,// bit depth octests
				1 // 1 channel
			);
		}

		if(ret!=0) {
			LOG_ERR("Failed to write wav header");
			return 0;
		}

		//pointer to buffer, data_size...
		//for block in blocks.....
		for(int i =0; i < slab_counter; i++) {
			//LOG_HEXDUMP_INF((uint8_t *)audio_buffers[i], buffer_sizes[i], "Received audio data:");		
			int ret = write_wav_data(&wav_file, audio_buffers[i], buffer_sizes[i]);

			k_mem_slab_free(&mem_slab, &audio_buffers[i]);

		if (ret != 0) {
			LOG_ERR("Failed to write to file, rc=%d", ret);
			return;
		}
		}
		ret = sd_card_close(&wav_file);
        if (ret != 0) {
			LOG_ERR("Failed to close SD card");
			return;
		} else {
            LOG_INF("CLOSED SD CARD");
        }

        // size_t buffer_size = 1024;
 	    // char buffer[buffer_size];   //Buffer to store file list
 	    // ret = sd_card_list_files(NULL, buffer, &buffer_size);
        // if(ret!=0) {
	    // 	LOG_ERR("SD Failed to list files");
	    // } else {
        //     LOG_INF("Configured SD Card sucessfully");
        // }


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

    ret = sd_card_init();
	if(ret!=0) {
		LOG_ERR("SD Failed to init");
	}


    const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
	if (!device_is_ready(i2s_dev)) {
	    LOG_WRN("%s is not ready\n", i2s_dev->name);
	}

    config_i2s_stream(i2s_dev);


    ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
    if (ret < 0) {
        LOG_WRN("Failed to start the transmission: %d\n", ret);
    }

    k_msleep(100); 
    int available_slabs = k_mem_slab_num_free_get(&mem_slab);

    void *buffer_pointer;
    size_t size;

    while(available_slabs > 0 && slab_counter < BLOCK_COUNT) {

        ret = i2s_read(i2s_dev, &buffer_pointer, &size);
        if (ret == 0) {
            //LOG_INF("Successfully read %d bytes", size);
        } else {
            LOG_ERR("%d - read failed: %d", slab_counter, ret);
        }
        //memset(buffer_pointer, 0, MAX_BLOCK_SIZE);
        //k_mem_slab_free(&mem_slab, &buffer_pointer);
        audio_buffers[slab_counter] = buffer_pointer;
        buffer_sizes[slab_counter] = size;

        //LOG_INF("%d - got buffer %p of %u bytes", slab_counter, buffer_pointer, size);
		available_slabs = k_mem_slab_num_free_get(&mem_slab);
		slab_counter++;

    }
    //Dump the contents of the final buffer
    //LOG_HEXDUMP_INF(buffer_pointer, MAX_BLOCK_SIZE, "AUDIO DATA p");

    write_wav();
    // while (1) {
    //     k_msleep(200);
    // }
    LOG_INF("Exiting");

}

