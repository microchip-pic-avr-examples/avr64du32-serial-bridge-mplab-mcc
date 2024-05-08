#ifndef TEXT_QUEUE_H
#define	TEXT_QUEUE_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define TEXT_QUEUE_SIZE 128
    
    //Initializes the Text Queue
    void TextQueue_Initialize(void);

    //Adds text to the Transmit Queue
    void TextQueue_AddText(const char* text);
    
    //Loads text from the internal queue into the Tx Buffer
    void TextQueue_LoadTransmitBuffer(void);
    
#ifdef	__cplusplus
}
#endif

#endif	/* TEXT_QUEUE_H */

