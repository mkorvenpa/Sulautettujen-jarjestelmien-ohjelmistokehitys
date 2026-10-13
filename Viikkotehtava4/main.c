/*
Viikkotehtävä 4. RTOS-ohjelman debuggaus
Tekijä: Maria Korvenpää
Arvosanatavoite tehtävälle: 3p/5p

- taskien ajoitusten mittaus
- debuggaus FIFO:n avulla debug taskissä
- debuggaus voidaan laittaa päälle ja pois päältä kirjoittamalla D UART:iin

*/


#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>

/****************************
 * Remember to add line:
 * CONFIG_HEAP_MEM_POOL_SIZE=1024
 * CONFIG_TIMING_FUNCTIONS=y
 * to prj.conf
 ****************************/


// Condition Variables
K_MUTEX_DEFINE(red_mutex);
K_CONDVAR_DEFINE(red_signal);
K_MUTEX_DEFINE(green_mutex);
K_CONDVAR_DEFINE(green_signal);
K_MUTEX_DEFINE(yellow_mutex);
K_CONDVAR_DEFINE(yellow_signal);
K_MUTEX_DEFINE(dispatcher_mutex);
K_CONDVAR_DEFINE(dispatcher_signal);

// Thread initializations
#define STACKSIZE 1024
#define PRIORITY 5

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// Configure buttons
#define BUTTON_0 DT_ALIAS(sw0)
#define BUTTON_1 DT_ALIAS(sw1)
#define BUTTON_2 DT_ALIAS(sw2)
#define BUTTON_3 DT_ALIAS(sw3)
#define BUTTON_4 DT_ALIAS(sw4)

static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;
static const struct gpio_dt_spec button_1 = GPIO_DT_SPEC_GET_OR(BUTTON_1, gpios, {0});
static struct gpio_callback button_1_data;
static const struct gpio_dt_spec button_2 = GPIO_DT_SPEC_GET_OR(BUTTON_2, gpios, {0});
static struct gpio_callback button_2_data;
static const struct gpio_dt_spec button_3 = GPIO_DT_SPEC_GET_OR(BUTTON_3, gpios, {0});
static struct gpio_callback button_3_data;
static const struct gpio_dt_spec button_4 = GPIO_DT_SPEC_GET_OR(BUTTON_4, gpios, {0});
static struct gpio_callback button_4_data;

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

void red_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);

void dispatcher_task(void *, void *, void*);
void uart_task(void *, void *, void*);

void debug_task(void *, void *, void*);

K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(debug_thread,STACKSIZE,debug_task,NULL,NULL,NULL,3,0,0);

bool debug = true;

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);

K_FIFO_DEFINE(data_fifo);

// FIFO dispatcher data type
struct data_t {
	/*************************
	// Add fifo_reserved below
	*************************/
	void *fifo_reserved;
	char msg[20];
};

struct data_t2 {
	void *fifo_reserved;
	uint64_t time;
	char message[50];
};

/********************
 * init UART
 */
int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
	return 0;
}

// Initialize leds
int  init_led() {

	// Led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: Red led configure failed");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return ret;
	}
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: Green led configure failed");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return ret;
	}
	// set led off
	gpio_pin_set_dt(&red,0);
	gpio_pin_set_dt(&green,0);

	if (debug==true) {
		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf != NULL) {
			snprintf(buf->message, 50, "%s", "Led initialized ok");
			buf->time = 0;
			k_fifo_put(&data_fifo, buf);
		}
	}
	
	return 0;
}

/********************
 * Main task
 */
int main(void)
{
	int ret = init_uart();
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: UART initialization failed");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return ret;
	}

	ret = init_button();
	if (ret < 0) {
		return 0;
	}

	init_led();

	timing_init();

	while (true) {
		k_msleep(100);
	}

	return 0;
}

/********************
 * UART task
 */
void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
				if (debug==true) {
					struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
					if (buf != NULL) {
						snprintf(buf->message, 50, "%s", "UART msg: %s", uart_msg);
						buf->time = 0;
						k_fifo_put(&data_fifo, buf);
					}
				}
                
				struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", uart_msg);

				// You need to:
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);

				// Clear UART receive buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);
			}
		}
		k_msleep(10);
	}
	return 0;
}

/********************
 * Dispatcher task
 */
void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		char sequence[20];
		memcpy(sequence,rec_item->msg,20);
		k_free(rec_item);

		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Dispatcher: %s", sequence);
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}

		int cnt = 0;


		timing_start();
		timing_t sequence_start_time = timing_counter_get();

		while (sequence[cnt] != 0) {
			

			if(sequence[cnt] == 'R') {
				if (debug==true) {
					struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
					if (buf != NULL) {
						snprintf(buf->message, 50, "%s", "Red led task on");
						buf->time = 0;
						k_fifo_put(&data_fifo, buf);
					}
				}
				k_condvar_signal(&red_signal);
			} else if(sequence[cnt] == 'G') {
				if (debug==true) {
					struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
					if (buf != NULL) {
						snprintf(buf->message, 50, "%s", "Green led task on");
						buf->time = 0;
						k_fifo_put(&data_fifo, buf);
					}
				}
				k_condvar_signal(&green_signal);
			} else if(sequence[cnt] == 'Y') {
				if (debug==true) {
					struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
					if (buf != NULL) {
						snprintf(buf->message, 50, "%s", "Yellow led task on");
						buf->time = 0;
						k_fifo_put(&data_fifo, buf);
					}
				}
				k_condvar_signal(&yellow_signal);
			} else if(sequence[cnt] == 'D') {
				debug = !debug;
				if (debug==true) {
					struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
					if (buf != NULL) {
						snprintf(buf->message, 50, "%s", "Debug on");
						buf->time = 0;
						k_fifo_put(&data_fifo, buf);
					}
				}
			} else {
				if (debug==true) {
					struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
					if (buf != NULL) {
						snprintf(buf->message, 50, "%s", "Dispatcher: Invalid color");
						buf->time = 0;
						k_fifo_put(&data_fifo, buf);
					}
				}
			}
			cnt++;

			if (sequence[cnt-1] == 'R' || sequence[cnt-1] == 'G' || sequence[cnt-1] == 'Y') {
				k_condvar_wait(&dispatcher_signal, &dispatcher_mutex, K_FOREVER);
			}

		}

		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf == NULL) {
			return;
		}

		timing_stop();
		timing_t sequence_end_time = timing_counter_get();
		uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&sequence_start_time, &sequence_end_time));
		
		buf->time = timing_ns;
		k_fifo_put(&data_fifo, buf);
	}
}

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{

	struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", "RYG");

				// You need to:
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);
}

void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", "R");

				// You need to:
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);

}

void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", "Y");

				// You need to:
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);
}

void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", "G");

				// You need to:
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);
}

void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{

	struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", "D");

				// You need to:
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo, buf);			
	
}


// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	while (true) { 
		
		k_condvar_wait(&red_signal, &red_mutex, K_FOREVER);

		timing_start();
		timing_t red_start_time = timing_counter_get();


		gpio_pin_set_dt(&red,1);
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Red led on");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		// 2. sleep for 2 seconds
		k_sleep(K_SECONDS(1));
		// 3. set led off
		gpio_pin_set_dt(&red,0);
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Red led off");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		// 4. sleep for 2 seconds
		k_sleep(K_SECONDS(1));

		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf == NULL) {
			k_condvar_signal(&dispatcher_signal);
			return;
		}
		
		timing_t red_end_time = timing_counter_get();
		timing_stop();
    	uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&red_start_time, &red_end_time));
		
		snprintf(buf->message, 50, "%s", "Red led task time");
		buf->time = timing_ns;
		k_fifo_put(&data_fifo, buf);

		k_condvar_signal(&dispatcher_signal);
	}
}

void yellow_led_task(void *, void *, void*) {
	
	while (true) {
		k_condvar_wait(&yellow_signal, &yellow_mutex, K_FOREVER);

		timing_start();
		timing_t yellow_start_time = timing_counter_get();

		// 1. set led on 
		gpio_pin_set_dt(&red,1);
		gpio_pin_set_dt(&green,1);
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Yellow led on");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		// 2. sleep for 2 seconds
		k_sleep(K_SECONDS(1));
		// 3. set led off
		gpio_pin_set_dt(&red,0);
		gpio_pin_set_dt(&green,0);
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Yellow led off");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		// 4. sleep for 2 seconds
		k_sleep(K_SECONDS(1));

		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf == NULL) {
			k_condvar_signal(&dispatcher_signal);
			return;
		}

		timing_t yellow_end_time = timing_counter_get();
		timing_stop();
    	uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&yellow_start_time, &yellow_end_time));
		
		snprintf(buf->message, 50, "%s", "Yellow led task time");
		buf->time = timing_ns;
		k_fifo_put(&data_fifo, buf);

		k_condvar_signal(&dispatcher_signal);
	}
}

void green_led_task(void *, void *, void*) {
	
	while (true) {
		k_condvar_wait(&green_signal, &green_mutex, K_FOREVER);

		timing_start();
		timing_t green_start_time = timing_counter_get();

		gpio_pin_set_dt(&green,1);
		
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Green led on");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		// 2. sleep for 2 seconds
		k_sleep(K_SECONDS(1));
		// 3. set led off
		gpio_pin_set_dt(&green,0);
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Green led off");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		// 4. sleep for 2 seconds
		k_sleep(K_SECONDS(1));

		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf == NULL) {
			k_condvar_signal(&dispatcher_signal);
			return;
		}

		timing_t green_end_time = timing_counter_get();
		timing_stop();
		uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&green_start_time, &green_end_time));
		
		snprintf(buf->message, 50, "%s", "Green led task time");
		buf->time = timing_ns;
		k_fifo_put(&data_fifo, buf);
		
		k_condvar_signal(&dispatcher_signal);
		
	}
}

// Button initialization
int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_0)) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: button 0 is not ready");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure interrupt on pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	if (debug==true) {
		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf != NULL) {
			snprintf(buf->message, 50, "%s", "Set up button 0 ok");
			buf->time = 0;
			k_fifo_put(&data_fifo, buf);
		}
	}

	if (!gpio_is_ready_dt(&button_1)) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: button 1 is not ready");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure interrupt on pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	gpio_init_callback(&button_1_data, button_1_handler, BIT(button_1.pin));
	gpio_add_callback(button_1.port, &button_1_data);
	if (debug==true) {
		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf != NULL) {
			snprintf(buf->message, 50, "%s", "Set up button 1 ok");
			buf->time = 0;
			k_fifo_put(&data_fifo, buf);
		}
	}

	if (!gpio_is_ready_dt(&button_2)) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: button 2 is not ready");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure interrupt on pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	gpio_init_callback(&button_2_data, button_2_handler, BIT(button_2.pin));
	gpio_add_callback(button_2.port, &button_2_data);
	if (debug==true) {
		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf != NULL) {
			snprintf(buf->message, 50, "%s", "Set up button 2 ok");
			buf->time = 0;
			k_fifo_put(&data_fifo, buf);
		}
	}

	if (!gpio_is_ready_dt(&button_3)) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: button 3 is not ready");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_3, GPIO_INPUT);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_3, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure interrupt on pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	gpio_init_callback(&button_3_data, button_3_handler, BIT(button_3.pin));
	gpio_add_callback(button_3.port, &button_3_data);
	if (debug==true) {
		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf != NULL) {
			snprintf(buf->message, 50, "%s", "Set up button 3 ok");
			buf->time = 0;
			k_fifo_put(&data_fifo, buf);
		}
	}

	if (!gpio_is_ready_dt(&button_4)) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: button 4 is not ready");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_4, GPIO_INPUT);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_4, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		if (debug==true) {
			struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
			if (buf != NULL) {
				snprintf(buf->message, 50, "%s", "Error: failed to configure interrupt on pin");
				buf->time = 0;
				k_fifo_put(&data_fifo, buf);
			}
		}
		return -1;
	}

	gpio_init_callback(&button_4_data, button_4_handler, BIT(button_4.pin));
	gpio_add_callback(button_4.port, &button_4_data);
	if (debug==true) {
		struct data_t2 *buf = k_malloc(sizeof(struct data_t2));
		if (buf != NULL) {
			snprintf(buf->message, 50, "%s", "Set up button 4 ok");
			buf->time = 0;
			k_fifo_put(&data_fifo, buf);
		}
	}

	return 0;
}

void debug_task(void *, void *, void*) {

	// Store received data
	struct data_t2 *received;

	while (true) {

		received = k_fifo_get(&data_fifo, K_FOREVER);

		printk("Debug message received: %s\n", received->message);
		if (received->time != 0) {
			printk("Debug time received: %lld ns\n", received->time);
		}
		
		k_free(received);

		k_yield();
	}
}