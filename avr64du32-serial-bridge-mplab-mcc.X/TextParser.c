#include "TextParser.h"

#include <xc.h>
#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/usb/usb_cdc/circular_buffer/circular_buffer.h"
#include "mcc_generated_files/usb/usb_cdc/usb_cdc_virtual_serial_port.h"
#include "TextQueue.h"
#include "mcc_generated_files/timer/delay.h"

#include <stdint.h>
#include <stdbool.h>

//Text Buffer
static char buffer[PARSER_BUFFER_SIZE];
static uint8_t textLength = 0;
static uint8_t readPos = 0;

//Advances to the position after the next ' ' or EOF in the string
bool Parser_Advance(void)
{
    while ((buffer[readPos] != '\0') && (buffer[readPos] != ' '))
    {
        readPos++;
    }

    //If at end of buffer
    if (buffer[readPos] == '\0')
    {
        return false;
    }

    //Advance to the text after the space
    readPos++;

    //This covers any case with multiple spaces
    while ((buffer[readPos] != '\0') && (buffer[readPos] == ' '))
    {
        readPos++;
    }
    
    //If at end of buffer
    if (buffer[readPos] == '\0')
    {
        return false;
    }
    
    return true;
}

//Checks to see if the current "chunk" of the sentence contains the reference
bool Parser_StringContains(const char* ref)
{
    char* ptr = buffer + readPos;
    uint8_t cIndex = 0;
    
    while ((*ptr != '\0') && (*ptr != ' '))
    {
        if (ref[cIndex] == '\0')
        {
            //End of reference string
            return true;
        }
        else if (*ptr == ref[cIndex])
        {
            //Characters match
            cIndex++;
        }
        else
        {
            //Not a match
            
            //See if the pattern repeated (Ex: apple in apaapple)
            if (*ptr == ref[0])
            {
                cIndex = 1;
            }
            else
            {
                cIndex = 0;
            }
        }
        
        //Advance pointer
        ptr++;
    }
    
    if (ref[cIndex] == '\0')
    {
        //End of reference string
        return true;
    }
    
    return false;
}

//Checks to see if the current "chunk" of the sentence matches the reference
bool Parser_StringMatch(const char* ref)
{
    char* ptr = buffer + readPos;
    uint8_t cIndex = 0;
    
    while ((*ptr != '\0') && (*ptr != ' '))
    {
        if (ref[cIndex] == '\0')
        {
            //End of reference string
            return false;
        }
        else if (*ptr == ref[cIndex])
        {
            //Characters match
            cIndex++;
        }
        else
        {
            //Not a match
            return false;
        }
        
        //Advance pointer
        ptr++;
    }
    
    if (ref[cIndex] == '\0')
    {
        //End of reference string
        return true;
    }
    
    return false;
}

//Tries to convert the current "chunk" of the sentence
bool Parser_ConvertStringToHex(uint8_t* dst)
{
    char* ptr = buffer + readPos;
    uint8_t result;
    
    if ((*ptr >= '0') && (*ptr <= '9'))
    {
        //Number
        result = *ptr - '0';
    }
    else if ((*ptr >= 'A') && (*ptr <= 'F'))
    {
        //Char
        result = (*ptr - 'A') + 10;
    }
    else
    {
        return false;
    }
    
    //Increment Pointer
    ptr++;
    
    //For single-digit numbers (a, 4, etc...)
    if ((*ptr == ' ') || (*ptr == '\0'))
    {
        return result;
    }
    
    result <<= 4;
    
    if ((*ptr >= '0') && (*ptr <= '9'))
    {
        //Number
        result |= (*ptr - '0');
    }
    else if ((*ptr >= 'A') && (*ptr <= 'F'))
    {
        //Char
        result |= ((*ptr - 'A') + 10);
    }
    else
    {
        return false;
    }

    //Set value
    *dst = result;
    
    return true;
}

uint8_t Parser_ConvertTextToHexArray(uint8_t* dst, uint8_t maxLen)
{
    uint8_t len = 0;
    do
    {
        //Convert the chunk to a hex number
        if (!Parser_ConvertStringToHex((dst + len)))
        {
            //Failed to convert
            return 0;
        }
        len++;
        
        //Advance to next chunk
    } while (Parser_Advance());
    
    //Return number of hex values
    return len;
}

//Initialize the text parser
void TextParser_Initialize(void)
{
    //Clear buffer
    for (uint8_t i = 0; i < PARSER_BUFFER_SIZE; i++)
    {
        buffer[i] = '\0';
    }
}

//Load and handle text from the USB Stack
void TextParser_Handle(void)
{
    //Unload any received characters
    char c;
    bool cmdReady = false;
    
    //Load characters
    while (USB_CDCRead(&c) == CDC_SUCCESS)
    {
        if (c == '\n')
        {
            cmdReady = true;
            buffer[textLength] = '\0';
        }
        else if (c == '\r')
        {
            //Don't load any RETURN characters
            continue;
        }
        
        //If no command is waiting and space is available
        if ((!cmdReady) && (textLength < (PARSER_BUFFER_SIZE - 1)))
        {
            //Convert lowercase to uppercase
            if ((c >= 'a') && (c <= 'z'))
            {
                c = (c - 'a') + 'A';
            }
            
            buffer[textLength] = c;
            textLength++;
        }
    }

    //No command is ready to be processed
    if (!cmdReady)
    {
        return;
    }
    
    //Reset read position
    readPos = 0;
    
    /* Commands:
     * SPI EEPROM <DATA>
     * SPI DAC <DATA>
     * 
     * I2C W <ADDR> <DATA>
     * I2C RW <ADDR> <REG ADDR (1 Byte)> <LEN>
     */
    
    bool isGood = true;
    uint8_t serialBytes[MAX_SERIAL_PARAMETERS];
    
    if (Parser_StringContains("SPI"))
    {
        TextQueue_AddText("SPI Command\r\n");
        if (Parser_Advance())
        {
            //Advance to next parameter
            if (Parser_StringContains("EEPROM"))
            {
                //Advance to next chunk
                Parser_Advance();
                
                //Convert everything else to <data> parameters
                uint8_t parms = Parser_ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);
                if (parms == 0)
                {
                    //Nothing to convert, or conversion error
                    isGood = false;
                }
                else
                {
                    //Conversion Success
                    EEPROM_CS_SetLow();
                    DELAY_microseconds(1);
                    SPI0_Host_BufferExchange(serialBytes, parms);
                    EEPROM_CS_SetHigh();
                }
            }
            else if (Parser_StringContains("DAC"))
            {
                //Advance to next chunk
                Parser_Advance();
                
                //Convert everything else to <data> parameters
                uint8_t parms = Parser_ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);
                if (parms == 0)
                {
                    //Nothing to convert, or conversion error
                    isGood = false;
                }
                else
                {
                    //Conversion Success
                    DAC_CS_SetLow();
                    DELAY_microseconds(1);
                    SPI0_Host_BufferExchange(serialBytes, parms);
                    DAC_CS_SetHigh();
                }
            }
            else
            {
                isGood = false;
            }
        }
        else
        {
            isGood = false;
        }
    }
    else if (Parser_StringContains("I2C"))
    {
        if (Parser_Advance())
        {
            uint8_t addr;
            uint8_t len;
            
            //Get the Address
            if (Parser_ConvertStringToHex(&addr))
            {
                //Address found
                if (Parser_Advance())
                {
                    //Advance to type of operation
                    if (Parser_StringMatch("R"))
                    {
                        //Read Command
                        if (Parser_Advance())
                        {
                            //Get # of Bytes to Read
                            if (Parser_ConvertStringToHex(&len))
                            {
                                //Length Found
                                I2C0_Host_Read(addr, serialBytes, len);
                                while (I2C0_Host_IsBusy())
                                {
                                    I2C0_Host_Tasks();
                                }
                            }
                            else
                            {
                                isGood = false;
                            }
                        }
                        else
                        {
                            isGood = false;
                        }
                    }
                    else if (Parser_StringMatch("W"))
                    {
                        //Write Command
                        
                        if (Parser_Advance())
                        {
                            //Get Bytes to Transmit
                            len = Parser_ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);
                        
                            if (len > 0)
                            {
                                //Bytes found
                                I2C0_Host_Write(addr, serialBytes, len);
                                while (I2C0_Host_IsBusy())
                                {
                                    I2C0_Host_Tasks();
                                }
                            }
                            else
                            {
                                isGood = false;
                            }
                        }
                        else
                        {
                            isGood = false;
                        }
                    }
                    else if (Parser_StringMatch("WR"))
                    {
                        //Write then Read
                        
                        if (Parser_Advance())
                        {
                            //Get Bytes
                            len = Parser_ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);

                            if (len == 2)
                            {
                                //Bytes found
                                I2C0_Host_WriteRead(addr, serialBytes, 1, (serialBytes), serialBytes[1]);
                                while (I2C0_Host_IsBusy())
                                {
                                    I2C0_Host_Tasks();
                                }
                            }
                            else
                            {
                                isGood = false;
                            }
                        }
                        else
                        {
                            isGood = false;
                        }
                    }
                    else
                    {
                        isGood = false;
                    }
                }
                
            }
            else
            {
                isGood = false;
            }
        }
        else
        {
            isGood = false;
        }
    }
    else
    {
        isGood = false;
    }
    
    if (!isGood)
    {
        TextQueue_AddText("Command Error\r\n");
    }
    
    //Clean-up
    textLength = 0;
}