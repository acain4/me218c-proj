#ifndef CommService_H
#define CommService_H

// Event Definitions
#include "ES_Configure.h" /* gets us event definitions */
#include "ES_Types.h"     /* gets bool type for returns */

// typedefs for the states
// State definitions for use with the query function
typedef enum { InitComm, WaitFor7E, WaitForMSB, WaitForLSB, 
               SuckUpPacket } CommServiceState_t ;


// Public Function Prototypes
bool InitCommService ( uint8_t Priority );
bool PostCommService( ES_Event ThisEvent );
ES_Event RunCommService( ES_Event ThisEvent );

uint8_t* getRecvArray(void);
uint8_t getPACAddressLSB(void);
uint8_t getPACAddressMSB(void);

//void BuildPacket(uint8_t WhichData);
//void SendPacket(uint8_t WhichData);

#endif /* CommService_H */
        

