#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define GPIO_OUTPUT_IO 4
#define GPIO_OUTPUT_PIN_SEL (1ULL<<GPIO_OUTPUT_IO)

#define GPIO_INPUT_IO 2
#define GPIO_INPUT_PIN_SEL (1ULL<<GPIO_INPUT_IO)

static QueueHandle_t queue = NULL;
static unsigned int count = 0;
static unsigned int last = 0;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(queue, &gpio_num, NULL);
}

static void task1(void *arg)
{
    uint32_t io_num;
    for(;;)
    {
        if(xQueueReceive(queue, &io_num, portMAX_DELAY))
        {
            uint32_t level = gpio_get_level(io_num);
            if (level != last)
            {
                last = level;
                count++;
                printf("\ncount: %d", count);
            }
        }
    }
}

void app_main() 
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    gpio_set_intr_type(GPIO_INPUT_IO, GPIO_INTR_ANYEDGE);
    
    queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(task1, "task1", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_INPUT_IO, gpio_isr_handler, (void *)GPIO_INPUT_IO);
    gpio_isr_handler_remove(GPIO_INPUT_IO);

    uint32_t cnt = 0;
    for(;;)
    {
        gpio_set_level(GPIO_OUTPUT_IO, 0 | cnt % 2);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_OUTPUT_IO, 1);
        vTaskDelay(750 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_OUTPUT_IO, 0 | cnt % 2);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_OUTPUT_IO, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}