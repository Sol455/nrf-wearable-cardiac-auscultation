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
#define BLOCK_COUNT 10
#define WAV_LENGTH_BLOCKS 100

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 32); //align to 32 bytes in memory
void *mem_blocks[BLOCK_COUNT];  // Array to store pointers to each block for initial SD tests

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

    size_t buffer_size = 1024;
 	char buffer[buffer_size];   //Buffer to store file list
 	// ret = sd_card_list_files(NULL, buffer, &buffer_size);
    // if(ret!=0) {
	// 	LOG_ERR("SD Failed to list files");
	// } else {
    //     LOG_INF("Configured SD Card sucessfully");

    // }

    const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
	if (!device_is_ready(i2s_dev)) {
	    LOG_WRN("%s is not ready\n", i2s_dev->name);
	}

    config_i2s_stream(i2s_dev);

    void *buffer_pointer;
    size_t bytes_read = 0;

    ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
    if (ret < 0) {
        LOG_WRN("Failed to start the transmission: %d\n", ret);
    }

    k_msleep(100); 

    for(int i = 0; i < 200; i ++) {

       // memset(buffer_pointer, 0, MAX_BLOCK_SIZE);

        ret = i2s_read(i2s_dev, &buffer_pointer, &bytes_read);
        if (ret == 0) {
            //LOG_INF("Successfully read %d bytes", bytes_read);
        } else {
            LOG_ERR("Failed to read I2S data: %d", ret);
        }


        // if (i > 100) {
        //     LOG_INF("First byte: 0x%02x", ((uint8_t*)buffer_pointer)[0]);
        //     LOG_INF("Second byte: 0x%02x", ((uint8_t*)buffer_pointer)[1]);
        //     LOG_INF("Third byte: 0x%02x", ((uint8_t*)buffer_pointer)[2]);
        //     LOG_INF("Fourth byte: 0x%02x", ((uint8_t*)buffer_pointer)[3]);
        // }
        memset(buffer_pointer, 0, MAX_BLOCK_SIZE);
        k_mem_slab_free(&mem_slab, &buffer_pointer);

    }
    //Dump the contents of the final buffer
    LOG_HEXDUMP_INF(buffer_pointer, MAX_BLOCK_SIZE, "AUDIO DATA p");
    while (1) {
        k_msleep(200);
    }
}

