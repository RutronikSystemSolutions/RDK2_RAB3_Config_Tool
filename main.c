/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the RDK2 RAB3 NJR4652 Configuration Tool
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*  Created on: 2022-10-28
*  Company: Rutronik Elektronische Bauelemente GmbH
*  Address: Jonavos g. 30, Kaunas 44262, Lithuania
*  Author: Gintaras Drukteinis
*
*******************************************************************************
* (c) 2019-2021, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*
* Rutronik Elektronische Bauelemente GmbH Disclaimer: The evaluation board
* including the software is for testing purposes only and,
* because it has limited functions and limited resilience, is not suitable
* for permanent use under real conditions. If the evaluation board is
* nevertheless used under real conditions, this is done at one’s responsibility;
* any liability of Rutronik is insofar excluded
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "usb_serial.h"

#define DATA_BITS_8     		8
#define STOP_BITS_1     		1
#define BAUD_RATE       		115200
#define ARDUINO_INT_PRIORITY    2
#define USB_RX_BUF_SIZE			1024
#define ARDUINO_RX_BUF_SIZE		1024
#define LED_INDICATION_TIME		10000

/*Global Variables for USB CDC/ARDUINO UART Bridge*/
cyhal_uart_t arduino_uart_obj;
uint8_t arduino_rx_buf[ARDUINO_RX_BUF_SIZE] = {0};
uint8_t usb_rx_buf[USB_RX_BUF_SIZE] = {0};
uint32_t usb_rx_led_activate = 0;
uint32_t arduino_rx_led_activate = 0;

/* Initialize the KITPROG UART configuration structure */
const cyhal_uart_cfg_t kitprog_uart_config =
{
    .data_bits = DATA_BITS_8,
    .stop_bits = STOP_BITS_1,
    .parity = CYHAL_UART_PARITY_NONE,
    .rx_buffer = NULL,
    .rx_buffer_size = 0
};

/* Initialize the ARDUINO UART configuration structure */
const cyhal_uart_cfg_t arduino_uart_config =
{
    .data_bits = DATA_BITS_8,
    .stop_bits = STOP_BITS_1,
    .parity = CYHAL_UART_PARITY_NONE,
    .rx_buffer = NULL,
    .rx_buffer_size = 0
};

int main(void)
{
    cy_rslt_t result;
    uint32_t count = 0;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }

    __enable_irq();

    /*Initialize LEDs*/
    result = cyhal_gpio_init( LED1, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}
    result = cyhal_gpio_init( LED2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Initialize BGT60TR13C Power Control pin*/
    result = cyhal_gpio_init(ARDU_IO3, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, true); /*Keep it OFF*/
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Initialize NJR4652F2S2 RESET pin*/
    result = cyhal_gpio_init(ARDU_IO6, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_OPENDRAINDRIVESLOW, false); /*Keep it OFF*/
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}
    CyDelay(100);

    /* Initialize the ARDUINO UART Block */
    result = cyhal_uart_init(&arduino_uart_obj, ARDU_TX, ARDU_RX, NC, NC, NULL, &arduino_uart_config);
    CY_ASSERT(CY_RSLT_SUCCESS == result);

    if(!usb_serial_init())
    {
    	CY_ASSERT(0);
    }

    for (;;)
    {
    	/*Check if we got some data on USB*/
		count = Cy_USB_Dev_CDC_GetCount(USB_SERIAL_PORT, &usb_cdcContext);
		if(count > 0)
		{
			count = Cy_USB_Dev_CDC_GetData(USB_SERIAL_PORT, usb_rx_buf, count, &usb_cdcContext);
    		if(count > 0)
    		{
    			usb_rx_led_activate = LED_INDICATION_TIME;
    			(void)cyhal_uart_write (&arduino_uart_obj, usb_rx_buf, (size_t*)&count);
    		}
		}

		/*Check if we got some data on Arduino UART*/
		count = cyhal_uart_readable(&arduino_uart_obj);
    	if(count)
    	{
    		result = cyhal_uart_read (&arduino_uart_obj, arduino_rx_buf, (size_t*)&count);
    		if((result == CY_RSLT_SUCCESS) && (count > 0))
    		{
    			arduino_rx_led_activate = LED_INDICATION_TIME;
    			/* Wait until component is ready to send data to host */
    			 while (0u == Cy_USB_Dev_CDC_IsReady(USB_SERIAL_PORT, &usb_cdcContext))
    			 {}

    			/*Send Data*/
    			Cy_USB_Dev_CDC_PutData(USB_SERIAL_PORT, arduino_rx_buf, count, &usb_cdcContext);
    		}
    	}

    	/*Blink the LED for USB RX*/
    	if(usb_rx_led_activate)
    	{
    		usb_rx_led_activate--;
    		cyhal_gpio_write(LED1, CYBSP_LED_STATE_ON);
    	}
    	else
    	{
    		cyhal_gpio_write(LED1, CYBSP_LED_STATE_OFF);
    	}

    	/*Blink the LED for ARDUINO RX*/
    	if(arduino_rx_led_activate)
    	{
    		arduino_rx_led_activate--;
    		cyhal_gpio_write(LED2, CYBSP_LED_STATE_ON);
    	}
    	else
    	{
    		cyhal_gpio_write(LED2, CYBSP_LED_STATE_OFF);
    	}
    }
}

/* [] END OF FILE */
