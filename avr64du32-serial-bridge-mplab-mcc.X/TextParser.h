#ifndef PARSER_H
#define	PARSER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdbool.h>
    
#define PARSER_BUFFER_SIZE 128
    
#define MAX_SERIAL_PARAMETERS 32
        
    //Initialize the text parser
    void TextParser_Initialize(void);
    
    //Load and handle text from the USB Stack
    void TextParser_Handle(void);
    
#ifdef	__cplusplus
}
#endif

#endif	/* PARSER_H */

