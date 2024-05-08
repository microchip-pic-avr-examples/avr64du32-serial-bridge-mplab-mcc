 /*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.0
*/

/*
© [2024] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/

#include <stdint.h>
#include <stdbool.h>

#include "mcc_generated_files/system/system.h"
#include "usb_core.h"
#include "usb_cdc_virtual_serial_port.h"

#include "text_queue.h"
#include "text_parser.h"

#define USB_MAX_RETRIES 10

typedef enum {
    USB_ERROR = -1, USB_DISCONNECTED = 0, USB_READY
} usb_state_t;

int main(void)
{
    SYSTEM_Initialize();
    
    //Init Text Queue
    TextQueue_Initialize();
    
    //Init Text Processor
    TextParser_Initialize();
    
    //Board configuration
    SPI0_Open(BOARD_CONFIG);

    usb_state_t usbState = USB_DISCONNECTED;    
    uint8_t retries = 0;
    
    if (AC0_Read())
    {
        USB_Stop();
    }
    
    //Enable Interrupts
    sei();
    
    while(1)
    {
        switch (usbState)
        {
            case USB_DISCONNECTED:
            {
                NANO_LED0_SetLow();
                if (AC0_Read())
                {
                    //VBUS
                    if (USB_Start() == SUCCESS)
                    {
                        //USB is Ready
                        usbState = USB_READY;
                        retries = 0;
                    }
                    else if (retries == USB_MAX_RETRIES)
                    {
                        usbState = USB_ERROR;
                    }
                    else
                    {
                        retries++;
                    }
                }
                break;
            }
            case USB_READY:
            {
                if (!AC0_Read())
                {
                    //VBUS is disconnected
                    usbState = USB_DISCONNECTED;
                    
                    //Reset retries count
                    retries = 0;
                }
                else
                {
                    //VBUS is still connected
                    NANO_LED0_SetHigh();
                    
                    //Process any text received
                    TextParser_Handle();
                    
                    //Load in any text to transmit
                    TextQueue_LoadTransmitBuffer();
                    
                    //Run the CDC Class Interface
                    if (USB_CDCVirtualSerialPortHandler() != SUCCESS)
                    {
                        usbState = USB_ERROR;
                    }
                    
                    //Handle USB Traffic
                    if (USBDevice_Handle() != SUCCESS)
                    {
                        usbState = USB_ERROR;
                    }
                }
                break;
            }
            case USB_ERROR:
            {
                //USB Error
                NANO_LED0_SetLow();
                
                if (!AC0_Read())
                {
                    //VBUS is disconnected
                    usbState = USB_DISCONNECTED;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }    
}