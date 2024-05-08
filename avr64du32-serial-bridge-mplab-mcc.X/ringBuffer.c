#include "ringBuffer.h"

#include <stdbool.h>
#include <stdint.h>

//Creates a ringBuffer from a pool of memory
void ringBuffer_createBuffer(rint_buffer_t* buffer, char* memPtr, ring_buffer_size_t size)
{
    buffer->memory = memPtr;
    buffer->memSize = size;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
}

//Copies the pointers and variables of SRC to DST
//DOES NOT COPY the MEMORY CONTENTS
void ringBuffer_softCopy(rint_buffer_t* src, rint_buffer_t* dst)
{
    dst->memSize = src->memSize;
    dst->memory = src->memory;
    dst->readIndex = src->readIndex;
    dst->writeIndex = src->writeIndex;
}

//Copies the memory contents AND the variables of SRC to DST
void ringBuffer_duplicate(rint_buffer_t* src, rint_buffer_t* dst)
{
    //No space to copy!
    if (dst->memSize == 0)
    {
        return;
    }
    
    ring_buffer_size_t dstMemSize = dst->memSize;
    
    if (dstMemSize < ringBuffer_charsToRead(src))
    {
        //Smaller than source, therefore copy 1 byte less so it doesn't overflow
        dstMemSize--;
    }
    
    //Raw Buffer Copy
    dst->writeIndex = ringBuffer_copyRawBuffer(src, dst->memory, dstMemSize);    
    dst->readIndex = 0;
}


//Clears the contents of a buffer with '\0' and resets indexes
void ringBuffer_reset(rint_buffer_t* buffer)
{
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
    for (ring_buffer_size_t i = 0; i < buffer->memSize; i++)
    {
        buffer->memory[i] = '\0';
    }
}

//Returns true if the buffer is empty
bool ringBuffer_isEmpty(rint_buffer_t* buffer)
{
    return (buffer->readIndex == buffer->writeIndex);
}

/* 
 * Advance the readIndex by 1 position
 * If readIndex exceeds writeIndex, readIndex is set to writeIndex
 */
void ringBuffer_incrementReadIndex(rint_buffer_t* buffer)
{
    if (buffer->readIndex != buffer->writeIndex)
    {
        buffer->readIndex++;
        
        //If past the last memory position
        if (buffer->readIndex >= buffer->memSize)
        {
            buffer->readIndex = 0;
        }
    }
}

/* 
 * Advance the readIndex by LEN positions
 * If readIndex exceeds writeIndex, readIndex is set to writeIndex
 */
void ringBuffer_advanceReadIndex(rint_buffer_t* buffer, ring_buffer_size_t len)
{
    //If empty...
    if (ringBuffer_isEmpty(buffer))
        return;
    
    ring_buffer_size_t wIndex = buffer->writeIndex;
    ring_buffer_size_t diff;
    
    if (wIndex > buffer->readIndex)
    {
        diff = wIndex - buffer->readIndex;
        if (diff >= len)
        {
            //Does not exceed wIndex
            buffer->readIndex += len;
        }
        else
        {
            //Exceeds wIndex
            buffer->readIndex = wIndex;
        }
    }
    else
    {
        //An overflow might occur
        
        //First, how many positions until overflow...
        diff = buffer->memSize - buffer->readIndex;
        
        if (diff > len)
        {
            //Overflow didn't occur
            buffer->readIndex += len;
        }
        else
        {
            //Overflow will occur...
            len -= diff;
            
            if (len > wIndex)
            {
                //Bigger than wIndex
                buffer->readIndex = wIndex;
            }
            else
            {
                //Smaller than wIndex
                buffer->readIndex = len;
            }
        }
    }
}

//Advances the ringIndex to the END of the 1st instance of STR in the buffer
//If the string could not be found, this function returns false
bool ringBuffer_advanceToString(rint_buffer_t* buffer, const char* str)
{    
    //Search Index
    ring_buffer_size_t sIndex = 0;
    
    while ((!ringBuffer_isEmpty(buffer)) && (str[sIndex] != '\0'))
    {
        //Get the current char
        if (ringBuffer_peekChar(buffer) == str[sIndex])
        {
            //Matches current position, increment
            sIndex++;
        }
        else if (ringBuffer_peekChar(buffer) == str[0])
        {
            //Matches position 1
            sIndex = 1;
        }
        else
        {
            //Does not match anything
            sIndex = 0;
        }
        
        //Advance Read Index
        ringBuffer_incrementReadIndex(buffer);
    }
    
    if (str[sIndex] == '\0')
    {
        //String was matched
        return true;
    }
    
    return false;
}

//Searches for STR in the buffer
//Returns true if found, false if not.
bool ringBuffer_find(rint_buffer_t* buffer, const char* str)
{
    if (ringBuffer_isEmpty(buffer))
    {
        return false;
    }
    
    ring_buffer_size_t wIndex, rIndex, sIndex;
    wIndex = buffer->writeIndex;
    rIndex = buffer->readIndex;
    sIndex = 0;
    
    while (rIndex != wIndex)
    {
        if (buffer->memory[rIndex] == str[sIndex])
        {
            //Matched current character
            sIndex++;
        }
        else if (buffer->memory[rIndex] == str[0])
        {
            //Matches position 0
            sIndex = 1;
        }
        else
        {
            //Didn't match anything
            sIndex = 0;
        }
        
        //If the string has been fully matched, return true
        if (str[sIndex] == '\0')
        {
            return true;
        }
        
        rIndex++;
        
        if (rIndex == buffer->memSize)
        {
            rIndex = 0;
        }
    }
    
    return false;
}

//Counts the number of characters matching SEARCH
ring_buffer_size_t ringBuffer_count(rint_buffer_t* buffer, char search)
{
    //If empty...
    if (ringBuffer_isEmpty(buffer))
    {
        return 0;
    }
    
    ring_buffer_size_t count = 0;
    
    ring_buffer_size_t wIndex, rIndex;
    wIndex = buffer->writeIndex;
    rIndex = buffer->readIndex;
    
    while (wIndex != rIndex)
    {
        if (buffer->memory[rIndex] == search)
        {
            //If matching, increment counter
            count++;
        }
        
        //Move index
        rIndex++;
        
        if (rIndex >= buffer->memSize)
        {
            rIndex = 0;
        }
    }
    
    return count;
}

//Returns the number of characters that can be read
ring_buffer_size_t ringBuffer_charsToRead(rint_buffer_t* buffer)
{
    ring_buffer_size_t count = 0;
    
    if (buffer->writeIndex >= buffer->readIndex)
    {
        //No rollover has occurred
        count = buffer->writeIndex - buffer->readIndex;
    }
    else
    {
        //Rollover has occurred
        count = (buffer->memSize - buffer->readIndex) + buffer->writeIndex;
    }
    
    return count;
}


//Clears the buffer by advancing readIndex to writeIndex
void ringBuffer_flushReadBuffer(rint_buffer_t* buffer)
{
    buffer->readIndex = buffer->writeIndex;
}

//Advance the writeIndex by 1 Position
//This function is called by the LOAD functions
void ringBuffer_incrementWriteIndex(rint_buffer_t* buffer)
{
    buffer->writeIndex++;
    
    if (buffer->writeIndex == buffer->memSize)
    {
        buffer->writeIndex = 0;
    }
}

//Loads a character into a ring buffer
//Returns FALSE if the ringBuffer overflows (operation will still complete, however)
bool ringBuffer_loadCharacter(rint_buffer_t* buffer, char input)
{
    buffer->memory[buffer->writeIndex] = input;
    ringBuffer_incrementWriteIndex(buffer);
    
    return (buffer->writeIndex == buffer->readIndex);
}

//Loads LEN characters into a ring buffer
//Returns FALSE if the ringBuffer overflows (operation will still complete, however)
bool ringBuffer_loadCharacters(rint_buffer_t* buffer, const char* input, ring_buffer_size_t len)
{
    bool overflow = false;
    
    for (ring_buffer_size_t i = 0; i < len; i++)
    {
        buffer->memory[buffer->writeIndex] = input[i];
        ringBuffer_incrementWriteIndex(buffer);
                
        //If writeIndex == readIndex, then an overflow occurred
        if (buffer->writeIndex == buffer->readIndex)
        {
            overflow = true;
        }
    }
    return overflow;
}

//Loads a NULL-terminated string into the ring buffer
//Returns FALSE if the ringBuffer overflows (operation will still complete, however)
bool ringBuffer_loadString(rint_buffer_t* buffer, const char* input)
{
    bool overflow = false;
    
    for (ring_buffer_size_t i = 0; input[i] != '\0'; i++)
    {
        buffer->memory[buffer->writeIndex] = input[i];
        ringBuffer_incrementWriteIndex(buffer);
        
        //If writeIndex == readIndex, then an overflow occurred
        if (buffer->writeIndex == buffer->readIndex)
        {
            overflow = true;
        }
    }
    return overflow;
}

//Returns the char at the current readIndex AND advances readIndex
//If no data is in the buffer, then '\0' is returned
char ringBuffer_getChar(rint_buffer_t* buffer)
{
    char c = '\0';
    
    //Cache writeIndex to protect against writes while being accessed
    ring_buffer_size_t writeIndex;
    writeIndex = buffer->writeIndex;
    
    //If not empty
    if (buffer->readIndex != writeIndex)
    {
        c = buffer->memory[buffer->readIndex];
        ringBuffer_incrementReadIndex(buffer);
    }
    
    return c;
}

//Returns the char at the current readIndex but does NOT advance readIndex
//If no data is in the buffer, then '\0' is returned
char ringBuffer_peekChar(rint_buffer_t* buffer)
{
    char c = '\0';
    
    //Cache writeIndex to protect against writes while being accessed
    ring_buffer_size_t writeIndex;
    writeIndex = buffer->writeIndex;
    
    //If not empty
    if (buffer->readIndex != writeIndex)
    {
        c = buffer->memory[buffer->readIndex];
    }
    
    return c;
}

/*
 * Copies the current ringBuffer to a destination string of LEN bytes.
 * A NULL terminator will be appended to the dest. string, if not found.
 * 
 * Returns the number of bytes (incl. terminator) written.
 * 
 * readIndex is not incremented.
 */
ring_buffer_size_t ringBuffer_copyBuffer(rint_buffer_t* buffer, char* dest, ring_buffer_size_t len)
{
    //If bigger than buffer size, reduce max count
    if (len > buffer->memSize)
    {
        len = buffer->memSize;
    }
    
    //Cached Indexes
    ring_buffer_size_t rIndex = buffer->readIndex;
    ring_buffer_size_t wIndex = buffer->writeIndex;
    ring_buffer_size_t bWritten = 0;
    
    /* 
     * While...
     * Space in Dest. String
     * RingBuffer has Data Remaining
     * RingBuffer[i] is not a terminator
     */
    while ((bWritten != len) && (rIndex != wIndex) && (buffer->memory[rIndex] != '\0'))
    {
        dest[bWritten] = buffer->memory[rIndex];
        bWritten++;
        rIndex++;
        
        //If we exceed the memory size
        if (rIndex >= buffer->memSize)
        {
            rIndex = 0;
        }
    }
    
    if (bWritten < len)
    {
        //Not at the end of the array
        dest[bWritten] = '\0';
        bWritten++;
    }
    else
    {
        //Out of memory - overwrite last element
        dest[bWritten - 1] = '\0';
    }
    
    return bWritten;
}

/*
 * Copies the current ringBuffer to a destination string of LEN bytes. 
 * A NULL terminator will NOT be appended to the dest. string.
 * 
 * Returns the number of bytes written.
 * 
 * readIndex is not incremented.
 */
ring_buffer_size_t ringBuffer_copyRawBuffer(rint_buffer_t* buffer, char* dest, ring_buffer_size_t len)
{
    //If bigger than buffer size, reduce max count
    if (len > buffer->memSize)
    {
        len = buffer->memSize;
    }
    
    //Cached Indexes
    ring_buffer_size_t rIndex = buffer->readIndex;
    ring_buffer_size_t wIndex = buffer->writeIndex;
    ring_buffer_size_t bWritten = 0;
    
    /* 
     * While...
     * Space in Dest. String
     * RingBuffer has Data Remaining
     * RingBuffer[i] is not a terminator
     */
    while ((bWritten != len) && (rIndex != wIndex) && (buffer->memory[rIndex] != '\0'))
    {
        dest[bWritten] = buffer->memory[rIndex];
        bWritten++;
        rIndex++;
        
        //If we exceed the memory size
        if (rIndex >= buffer->memSize)
        {
            rIndex = 0;
        }
    }
    
    return bWritten;
}

/*
 * Copies the current ringBuffer until DELIM or LEN bytes have been copied. 
 * A NULL terminator will be appended to the dest. string.
 * 
 * Returns the number of bytes (incl. terminator) written.
 * 
 * readIndex is not incremented.
 */
ring_buffer_size_t ringBuffer_copyBufferUntil(rint_buffer_t* buffer, char* dest, char delim, ring_buffer_size_t len)
{
    //If bigger than buffer size, reduce max count
    if (len > buffer->memSize)
    {
        len = buffer->memSize;
    }
    
    //Cached Indexes
    ring_buffer_size_t rIndex = buffer->readIndex;
    ring_buffer_size_t wIndex = buffer->writeIndex;
    ring_buffer_size_t bWritten = 0;
    
    /* 
     * While...
     * Space in Dest. String
     * RingBuffer has Data Remaining
     * RingBuffer[i] is not the deliminator
     */
    while ((bWritten != len) && (rIndex != wIndex) && (buffer->memory[rIndex] != delim))
    {
        dest[bWritten] = buffer->memory[rIndex];
        bWritten++;
        rIndex++;
        
        //If we exceed the memory size
        if (rIndex >= buffer->memSize)
        {
            rIndex = 0;
        }
    }
    
    if (bWritten < len)
    {
        //Not at the end of the array
        dest[bWritten] = '\0';
        bWritten++;
    }
    else
    {
        //Out of memory - overwrite last element
        dest[bWritten - 1] = '\0';
    }
    
    return bWritten;
}

/*
 * Copies the current ringBuffer between START and STOP, or until LEN bytes have been copied. 
 * A NULL terminator will be appended to the dest. string.
 * 
 * Returns the number of bytes (incl. terminator) written.
 * 
 * readIndex is not incremented.
 */
ring_buffer_size_t ringBuffer_copyAndChop(rint_buffer_t* buffer, char* dest, char start, char stop, ring_buffer_size_t len)
{
    //If bigger than buffer size, reduce max count
    if (len > buffer->memSize)
    {
        len = buffer->memSize;
    }
    
    //Cached Indexes
    ring_buffer_size_t rIndex = buffer->readIndex;
    ring_buffer_size_t wIndex = buffer->writeIndex;
    ring_buffer_size_t bWritten = 0;
    
    bool hasStarted = false;
    
    /* 
     * While...
     * Space in Dest. String
     * RingBuffer has Data Remaining
     * RingBuffer[i] is not the deliminator
     */
    while ((bWritten != len) && (rIndex != wIndex) && 
            (!((hasStarted) && (buffer->memory[rIndex] == stop))))
    {
        if (hasStarted)
        {
            //Start character passed
            dest[bWritten] = buffer->memory[rIndex];
            bWritten++;
        }
        else if (buffer->memory[rIndex] == start)
        {
            //Matching start character
            hasStarted = true;
        }
        
        rIndex++;
        
        //If we exceed the memory size
        if (rIndex >= buffer->memSize)
        {
            rIndex = 0;
        }
    }
    
    if (bWritten < len)
    {
        //Not at the end of the array
        dest[bWritten] = '\0';
        bWritten++;
    }
    else
    {
        //Out of memory - overwrite last element
        dest[bWritten - 1] = '\0';
    }
    
    return bWritten;
}