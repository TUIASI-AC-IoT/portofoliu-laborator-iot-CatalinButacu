#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "string.h"

#define BUF_SIZE 1024

const double LOWEST_TEMP = 15;
const double HIGHEST_TEMP = 40;

const double LOWEST_VELOCITY = -3;
const double HIGHEST_VELOCITY = 3;

double temperature = 18;

volatile double velocity = 0;
volatile double acceleration = 0;

static void simulate_temperature(void *arg)
{
	while (1)
	{
		// Increment temperature based on velocity
		temperature += velocity;
		if (temperature < LOWEST_TEMP)
		{
			temperature = LOWEST_TEMP;
		}
		else if (temperature > HIGHEST_TEMP)
		{
			temperature = HIGHEST_TEMP;
		}

		// Change velocity based on acceleration
		velocity += acceleration;
		if (velocity < LOWEST_VELOCITY)
		{
			velocity = LOWEST_VELOCITY;
		}
		else if (velocity > HIGHEST_VELOCITY)
		{
			velocity = HIGHEST_VELOCITY;
		}

		// Decrease acceleration to simulate friction
		if (acceleration > 0)
		{
			acceleration = acceleration - 0.1;
			if (acceleration < 0)
			{
				acceleration = 0;
			}
		}
		else if (acceleration < 0)
		{
			acceleration = acceleration + 0.1;
			if (acceleration > 0)
			{
				acceleration = 0;
			}
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void handle_serial(char *line)
{
    // Remove any trailing newline or carriage return characters
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
        line[len-1] = '\0';
        len--;
    }

    // Print received command for debugging
    printf("Received command: '%s'\n", line);
    fflush(stdout);

    if (strcmp(line, "GET") == 0)
    {
        // Send temperature value
        printf("%.2f\n", temperature);
        // Flush the output to ensure it's sent immediately
        fflush(stdout);
    }
    else if (strcmp(line, "SET HEATER") == 0 || strcmp(line, "HEATER") == 0)
    {
        // Set velocity to a specific value
        acceleration = 1;
        velocity = 0;
        printf("OK\n");
        fflush(stdout);
    }
    else if (strcmp(line, "SET COOLER") == 0 || strcmp(line, "COOLER") == 0)
    {
        // Set acceleration to a specific value
        acceleration = -1; // Example value
        velocity = 0;
        printf("OK\n");
        fflush(stdout);
    }
    else if (strcmp(line, "STOP") == 0)
    {
        // Stop the heater or cooler
        acceleration = 0;
        velocity = 0;
        printf("OK\n");
        fflush(stdout);
    }
    else
    {
        // Unknown command
        printf("ERROR: Unknown command\n");
        fflush(stdout);
    }
}

void app_main()
{
    // Create a task to simulate temperature changes
    xTaskCreate(simulate_temperature, "simulate_temperature", 2048, NULL, 5, NULL);

    // Initialize UART for serial communication
    const uart_port_t uart_num = UART_NUM_0;
    const int uart_baud_rate = 115200;
    const int uart_buffer_size = (1024 * 2);
    const int uart_buffer_size_bytes = uart_buffer_size * sizeof(char);
    uart_config_t uart_config = {
            .baud_rate = uart_baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(uart_num, uart_buffer_size_bytes, 0, 0, NULL, 0);
    uart_param_config(uart_num, &uart_config);

    // Print startup message
    printf("ESP32 Temperature Simulator Started\n");
    printf("Available commands: GET, SET HEATER, SET COOLER, STOP\n");
    fflush(stdout);

    // Handle serial read lines
    char line[BUF_SIZE];
    size_t pos = 0;
    while (1)
    {
        uint8_t ch;
        int len = uart_read_bytes(uart_num, &ch, 1, pdMS_TO_TICKS(100));
        if (len > 0)
        {
            // Echo character for debugging
            uart_write_bytes(uart_num, (const char*)&ch, 1);
            
            if (ch == '\n' || ch == '\r')
            {
                if (pos > 0) { // Only process non-empty lines
                    line[pos] = '\0';
                    handle_serial(line);
                    pos = 0; // Reset for next line
                }
            }
            else if (pos < sizeof(line) - 1)
            {
                line[pos++] = ch;
            }
        }
    }
}