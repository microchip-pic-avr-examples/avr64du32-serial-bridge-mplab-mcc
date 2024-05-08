#include "text_queue.h"

#include "ringBuffer.h"
#include "usb_cdc_virtual_serial_port.h"
#include "circular_buffer.h"

static char buffer[TEXT_QUEUE_SIZE];
static rint_buffer_t ringBuffer;

//Initializes the Text Queue
void TextQueue_Initialize(void)
{
    ringBuffer_createBuffer(&ringBuffer, buffer, TEXT_QUEUE_SIZE);
}

//Adds text to the Transmit Queue
void TextQueue_AddText(const char* text)
{
    ringBuffer_loadString(&ringBuffer, text);
}

//Loads text from the internal queue into the Tx Buffer
void TextQueue_LoadTransmitBuffer(void)
{
    while (!ringBuffer_isEmpty(&ringBuffer))
    {
        if (USB_CDCWrite(ringBuffer_peekChar(&ringBuffer)) == CDC_SUCCESS)
        {
            //Advance to next character
            ringBuffer_incrementReadIndex(&ringBuffer);
        }
        else
        {
            //Buffer error - exit and try again later
            return;
        }
    }
}

