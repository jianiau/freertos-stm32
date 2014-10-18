/**
  ******************************************************************************
  * @file    Template/main.c 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    20-September-2013
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "draw_graph.h"
#include "move_car.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "usart.h"
#include "filesystem.h"
#include "fio.h"
#include "romfs.h"
#include "hash-djb2.h"
#include "clib.h"
#include "shell.h"
#include "host.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
void send_byte(char ch);

xQueueHandle t_queue; /* Traffic light queue. */
xQueueHandle t_mutex; /* Traffic light mutex. */

static int traffic_index = 0; 
static int button_change_traffic = 0;
static int states[] = {TRAFFIC_RED, TRAFFIC_YELLOW, TRAFFIC_GREEN, 
							TRAFFIC_YELLOW};

volatile xSemaphoreHandle serial_tx_wait_sem;
/* Add for serial input */
volatile xQueueHandle serial_rx_queue;
extern const unsigned char _sromfs;
extern uint32_t pwd_hash; // /romfs

void
prvInit()
{
	//LCD init
	LCD_Init();
	IOE_Config();
	LTDC_Cmd( ENABLE );

	LCD_LayerInit();
	LCD_SetLayer( LCD_FOREGROUND_LAYER );
	LCD_Clear( LCD_COLOR_BLACK );
	LCD_SetTextColor( LCD_COLOR_WHITE );

	//Button
	STM_EVAL_PBInit( BUTTON_USER, BUTTON_MODE_GPIO );
}

static void GetTrafficState(int change_state, int *v_state, int *h_state)
{

	switch (change_state) {
	case TRAFFIC_RED:
		*v_state = TRAFFIC_RED;
		*h_state = TRAFFIC_GREEN;
		break;
	case TRAFFIC_YELLOW:
		if (*v_state == TRAFFIC_GREEN)
			*v_state = TRAFFIC_YELLOW;
		else
			*h_state = TRAFFIC_YELLOW;
		break;
	case TRAFFIC_GREEN:
		*v_state = TRAFFIC_GREEN;
		*h_state = TRAFFIC_RED;
		break;
	default:
		ReportError("out of range");
		break;
	}
}


static void DrawGraphTask( void *pvParameters)
{
	const portTickType ticks = 100 / portTICK_RATE_MS;
	int value;
	int traffic_v_state = TRAFFIC_GREEN;
	int traffic_h_state = TRAFFIC_RED;

	portBASE_TYPE status;

	DrawBackground();

	while ( 1 ) {
		/*
		 * Check if the traffic changed event is sent to
		 * the queue. If so, we need to change the traffic
		 * light.
		 */
		status = xQueueReceive(t_queue, &value, ticks);

		if (status == pdPASS) {
			GetTrafficState(value, &traffic_v_state, 
						&traffic_h_state);
		}

		MoveCar(traffic_v_state, traffic_h_state);
	}
}

static void ChgTrafficLightTask(void *pvParameters)
{
	int num_ticks;
	int states_num = sizeof(states) / sizeof(states[0]);

	portBASE_TYPE status;
	portTickType ticks = TRAFFIC_GREEN_TICK;

	while ( 1 ) {
		ticks = (states[traffic_index] == TRAFFIC_YELLOW ? 
			TRAFFIC_YELLOW_TICK : TRAFFIC_GREEN_TICK);

		num_ticks = ticks / TRAFFIC_TICK_SLICE;

		status = xQueueSendToBack(t_queue, &states[traffic_index++], 0);
	
		if (status != pdPASS)
			ReportError("Cannot send to the queue!");

		if (traffic_index >= states_num)
			traffic_index = 0;

		while (num_ticks--) { 
			xSemaphoreTake(t_mutex, portMAX_DELAY);
			
			if (button_change_traffic) {
				button_change_traffic = 0;
				xSemaphoreGive(t_mutex);
				break;
			}

			xSemaphoreGive(t_mutex);

			vTaskDelay(TRAFFIC_TICK_SLICE);
		}
	}
}

static void ButtonEventTask(void *pvParameters)
{
	while (1) {
		if( STM_EVAL_PBGetState( BUTTON_USER ) ){

			while( STM_EVAL_PBGetState( BUTTON_USER ) );

			xSemaphoreTake(t_mutex, portMAX_DELAY);
			button_change_traffic = 1;
			xSemaphoreGive(t_mutex);
		}
	}
}

/* IRQ handler to handle USART1 interruptss (both transmit and receive
 * interrupts). */
void USART1_IRQHandler()
{
	static signed portBASE_TYPE xHigherPriorityTaskWoken;
	//xHigherPriorityTaskWoken = pdFALSE;
	/* If this interrupt is for a transmit... */
	if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET) {
		/* "give" the serial_tx_wait_sem semaphore to notfiy processes
		 * that the buffer has a spot free for the next byte.
		 */
		
		//xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);
		//xSemaphoreGive(serial_tx_wait_sem);
		/* Diables the transmit interrupt. */
		USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
		/* If this interrupt is for a receive... */
	}else if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET){
		char msg = USART_ReceiveData(USART1);
		/* If there is an error when queueing the received byte, freeze! */
				
		//if(!xQueueSendToBackFromISR(serial_rx_queue, &msg, &xHigherPriorityTaskWoken))
		//if(!(xQueueSendToBack(serial_rx_queue, &msg, 0))) {
		
		if(!(xQueueSendToBack(serial_rx_queue, &msg, 0))) {
			while(1)  {
				fio_printf(1, "\r\nRX IRQ 111\r\n");
			}
		} else {
		}
	}
	else {
		/* Only transmit and receive interrupts should be enabled.
		 * If this is another type of interrupt, freeze.
		 */
		while(1);
	}

	if (xHigherPriorityTaskWoken) {
		taskYIELD();
	}
}


void send_byte(char ch)
{
	/* Wait until the RS232 port can receive another byte (this semaphore
	 * is "given" by the RS232 port interrupt when the buffer has room for
	 * another byte.
	 */
//	while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

	/* Send the byte and enable the transmit interrupt (it is disabled by
	 * the interrupt).
	 */
	while( !(USART1->SR & 0x00000040) );
	USART_SendData(USART1, ch);
//	USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
}

char recv_byte()
{

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	char msg;
//fio_printf(1, "\r\nrecv_byte start\r\n");
	while(!xQueueReceive(serial_rx_queue, &msg, portMAX_DELAY));
//fio_printf(1, "\r\nrecv_byte end\r\n");
	return msg;
}


void command_prompt(void *pvParameters)
{
	char buf[128];
	char *argv[20];
    char hint[] = USER_NAME "@" USER_NAME "-STM32:~$ ";

	fio_printf(1, "\r\nWelcome to FreeRTOS Shell\r\n");

	while(1){
		fio_printf(1, "%s", hint);
		fio_read(0, buf, 127);
		int n=parse_command(buf, argv);

		/* will return pointer to the command function */
		//if (*argv[0]=='\0')
		cmdfunc *fptr=do_command(argv[0]);
		if(fptr!=NULL)
			fptr(n, argv);
		else if (*argv[0]!='\0')
			fio_printf(2, "\r\n\"%s\" command not found.\r\n", argv[0]);
		else
			fio_printf(1, "\r\n");
	}

}

//Main Function
int main(void)
{
	init_rs232();
	enable_rs232_interrupts();
	enable_rs232();
		
	
	fs_init();
	fio_init();
	
	fio_printf(1, "\r\n init RS232 \r\n");
	
	register_romfs("romfs", &_sromfs);
	pwd_hash=hash_djb2((const uint8_t *)&"",-1); // init pwd to /romfs/
	
	/* Create the queue used by the serial task.  Messages for write to
	 * the RS232. */
	//vSemaphoreCreateBinary(serial_tx_wait_sem);
	serial_tx_wait_sem = xSemaphoreCreateMutex();
	/* Add for serial input 
	 * Reference: www.freertos.org/a00116.html */
	serial_rx_queue = xQueueCreate(1, sizeof(char));
	
	if (!serial_rx_queue) {
		fio_printf(1, "\r\nqueue GG\r\n");
		while(1);	
	} else {
		fio_printf(1, "\r\nqueue OK\r\n");
	}


	t_queue = xQueueCreate(1, sizeof(int));
	if (!t_queue) {
		ReportError("Failed to create t_queue");
		while(1);
	}

	t_mutex = xSemaphoreCreateMutex();
	if (!t_mutex) {
		ReportError("Failed to create t_mutex");
		while(1);
	}

	prvInit();


	xTaskCreate(command_prompt, "CLI",512 , NULL, tskIDLE_PRIORITY + 2, NULL);
	
	xTaskCreate(ChgTrafficLightTask, "Traffic Light Task", 256, 
			( void * ) NULL, tskIDLE_PRIORITY + 4, NULL);

	xTaskCreate(ButtonEventTask, (char *) "Button Event Task", 256,
		   	NULL, tskIDLE_PRIORITY + 2, NULL);

	xTaskCreate(DrawGraphTask, (char *) "Draw Graph Task", 256,
		   	NULL, tskIDLE_PRIORITY + 3, NULL);


	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
        RNG_Cmd(ENABLE);

	//Call Scheduler
	vTaskStartScheduler();
}

