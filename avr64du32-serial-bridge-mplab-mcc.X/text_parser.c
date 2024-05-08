#include "text_parser.h"

#include <xc.h>
#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/usb/usb_cdc/usb_cdc_virtual_serial_port.h"
#include "text_queue.h"
#include "mcc_generated_files/timer/delay.h"

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    COMMAND_OK = 0, COMMAND_INVALID, COMMAND_ADDR_NACK, COMMAND_DATA_NACK
} command_error_t;

typedef enum {
    SERIAL_UNKNOWN = 0, SERIAL_SPI_DAC, SERIAL_SPI_EEPROM, SERIAL_SPI_USD, SERIAL_I2C_READ, SERIAL_I2C_WRITE, SERIAL_I2C_WRITE_READ
} serial_type_t;

//Text Buffer
static char buffer[PARSER_BUFFER_SIZE];
static uint8_t textLength = 0;
static uint8_t readPos = 0;

//Advances to the position after the next ' ' or EOF in the string
bool AdvanceBuffer(void)
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
bool StringContains(const char* ref)
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
bool StringMatch(const char* ref)
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
bool ConvertStringToHex(uint8_t* dst)
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

uint8_t ConvertTextToHexArray(uint8_t* dst, uint8_t maxLen)
{
    uint8_t len = 0;
    do
    {
        //Convert the chunk to a hex number
        if (!ConvertStringToHex((dst + len)))
        {
            //Failed to convert
            return 0;
        }
        len++;
        
        //Advance to next chunk
    } while (AdvanceBuffer());
    
    //Return number of hex values
    return len;
}

void LoadDataToOutputQueue(uint8_t* data, uint8_t len)
{
    TextQueue_AddText("> ");
    
    char buffer[4] = {'?', '?', ' ', '\0'};
    uint8_t temp;
    
    for (uint8_t i = 0; i < len; i++)
    {
        //High Nibble
        temp = data[i] >> 4;
        
        if (temp > 9)
        {
            //A-F Output
            temp -= 10;
            buffer[0] = temp + 'A';
        }
        else
        {
            buffer[0] = temp + '0';
        }
        
        //Low Nibble
        temp = data[i] & 0x0F;
        
        if (temp > 9)
        {
            //A-F Output
            temp -= 10;
            buffer[1] = temp + 'A';
        }
        else
        {
            buffer[1] = temp + '0';
        }
        
        TextQueue_AddText(buffer);
    }
    
    TextQueue_AddText("\r\n");
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
    
    command_error_t commandStatus = COMMAND_INVALID;
    serial_type_t serialType = SERIAL_UNKNOWN;
    uint8_t serialBytes[MAX_SERIAL_PARAMETERS];
    uint8_t len;
    
    if (StringContains("SPI"))
    {
        if (AdvanceBuffer())
        {
            //Advance to next parameter
            if (StringMatch("EEPROM"))
            {
                //Advance to next chunk
                AdvanceBuffer();
                
                serialType = SERIAL_SPI_EEPROM;
                
                //Convert everything else to <data> parameters
                len = ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);
                if (len == 0)
                {
                    //Nothing to convert, or conversion error
                    commandStatus = COMMAND_INVALID;
                }
                else
                {
                    //Conversion Success
                    EEPROM_CS_SetLow();
                    DELAY_microseconds(1);
                    SPI0_Host_BufferExchange(serialBytes, len);
                    EEPROM_CS_SetHigh();
                    
                    commandStatus = COMMAND_OK;
                }
            }
            else if (StringMatch("DAC"))
            {
                //Advance to next chunk
                AdvanceBuffer();
                
                serialType = SERIAL_SPI_DAC;
                
                //Convert everything else to <data> parameters
                len = ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);
                if (len == 0)
                {
                    //Nothing to convert, or conversion error
                    commandStatus = COMMAND_INVALID;
                }
                else
                {
                    //Conversion Success
                    DAC_CS_SetLow();
                    DELAY_microseconds(1);
                    SPI0_Host_BufferExchange(serialBytes, len);
                    DAC_CS_SetHigh();
                    
                    commandStatus = COMMAND_OK;
                }
            }
            else if (StringMatch("USD"))
            {
                //Advance to next chunk
                AdvanceBuffer();
                
                serialType = SERIAL_SPI_USD;
                
                //Convert everything else to <data> parameters
                len = ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);
                if (len == 0)
                {
                    //Nothing to convert, or conversion error
                    commandStatus = COMMAND_INVALID;
                }
                else
                {
                    //Conversion Success
                    uSD_CS_SetLow();
                    DELAY_microseconds(1);
                    SPI0_Host_BufferExchange(serialBytes, len);
                    uSD_CS_SetHigh();
                    
                    commandStatus = COMMAND_OK;
                }

            }
        }
    }
    else if (StringContains("I2C"))
    {
        if (AdvanceBuffer())
        {
            uint8_t addr;
            
            //Get the Address
            if (ConvertStringToHex(&addr))
            {
                //Address found
                if (AdvanceBuffer())
                {
                    //Advance to type of operation
                    if (StringMatch("R"))
                    {
                        //Read Command
                        AdvanceBuffer();
                        
                        serialType = SERIAL_I2C_READ;
                                
                        //Get # of Bytes to Read
                        if (ConvertStringToHex(&len))
                        {
                            //Length Found
                            I2C0_Host_Read(addr, serialBytes, len);
                            while (I2C0_Host_IsBusy())
                            {
                                I2C0_Host_Tasks();
                            }

                            switch (I2C0_Host_ErrorGet())
                            {
                                case I2C_ERROR_NONE:
                                {
                                    commandStatus = COMMAND_OK;
                                    break;
                                }
                                case I2C_ERROR_ADDR_NACK:
                                {
                                    commandStatus = COMMAND_ADDR_NACK;
                                    break;
                                }
                                case I2C_ERROR_DATA_NACK:
                                {
                                    commandStatus = COMMAND_DATA_NACK;
                                    break;
                                }
                                default:
                                {

                                }
                            }
                        }
                    }
                    else if (StringMatch("W"))
                    {
                        //Write Command
                        AdvanceBuffer();
                        
                        serialType = SERIAL_I2C_WRITE;
                        
                        //Get Bytes to Transmit
                        len = ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);

                        if (len > 0)
                        {
                            //Bytes found
                            I2C0_Host_Write(addr, serialBytes, len);
                            while (I2C0_Host_IsBusy())
                            {
                                I2C0_Host_Tasks();
                            }
                            
                            switch (I2C0_Host_ErrorGet())
                            {
                                case I2C_ERROR_NONE:
                                {
                                    commandStatus = COMMAND_OK;
                                    break;
                                }
                                case I2C_ERROR_ADDR_NACK:
                                {
                                    commandStatus = COMMAND_ADDR_NACK;
                                    break;
                                }
                                case I2C_ERROR_DATA_NACK:
                                {
                                    commandStatus = COMMAND_DATA_NACK;
                                    break;
                                }
                                default:
                                {

                                }
                            }
                        }
                    }
                    else if (StringMatch("WR"))
                    {
                        //Write then Read
                        AdvanceBuffer();
                        
                        serialType = SERIAL_I2C_WRITE_READ;
                        
                        //Get Bytes
                        len = ConvertTextToHexArray(serialBytes, MAX_SERIAL_PARAMETERS);

                        if (len == 2)
                        {
                            //Bytes found
                            
                            //Read Length is in byte 2
                            len = serialBytes[1];
                            
                            I2C0_Host_WriteRead(addr, serialBytes, 1, (serialBytes),len);
                            while (I2C0_Host_IsBusy())
                            {
                                I2C0_Host_Tasks();
                            }

                            switch (I2C0_Host_ErrorGet())
                            {
                                case I2C_ERROR_NONE:
                                {
                                    commandStatus = COMMAND_OK;
                                    break;
                                }
                                case I2C_ERROR_ADDR_NACK:
                                {
                                    commandStatus = COMMAND_ADDR_NACK;
                                    break;
                                }
                                case I2C_ERROR_DATA_NACK:
                                {
                                    commandStatus = COMMAND_DATA_NACK;
                                    break;
                                }
                                default:
                                {

                                }
                            }
                        }
                    }
                }
                
            }
        }
    }
    
    switch (commandStatus)
    {
        case COMMAND_OK:
        {
            //Print results
            switch (serialType)
            {
                case SERIAL_SPI_EEPROM:
                case SERIAL_SPI_DAC:
                case SERIAL_SPI_USD:
                {
                    //SPI Communication
                    LoadDataToOutputQueue(serialBytes, len);
                    break;
                }
                case SERIAL_I2C_READ:
                {
                    //I2C Read
                    LoadDataToOutputQueue(serialBytes, len);
                    break;
                }
                case SERIAL_I2C_WRITE:
                {
                    //I2C Write
                    TextQueue_AddText("> OK\r\n");
                    break;
                }
                case SERIAL_I2C_WRITE_READ:
                {
                    //I2C Write/Read
                    LoadDataToOutputQueue(serialBytes, len);
                    break;
                }
                default:
                {
                    TextQueue_AddText("Unknown communication type\r\n");
                }
            }
            break;
        }
        case COMMAND_INVALID:
        {
            TextQueue_AddText("Command parsing error\r\n");
            break;
        }
        case COMMAND_ADDR_NACK:
        {
            TextQueue_AddText("I2C NACK error\r\n");
            break;
        }
        case COMMAND_DATA_NACK:
        {
            TextQueue_AddText("I2C communication error\r\n");
            break;
        }
        default:
        {
            //Shouldn't get here
            TextQueue_AddText("Unknown error\r\n");
        }
    }
    
    //Clean-up
    textLength = 0;
}