#ifndef PairingSM_H
#define PairingSM_H

// Event Definitions
#include "ES_Configure.h" /* gets us event definitions */
#include "ES_Types.h"     /* gets bool type for returns */

// typedefs for the states
// State definitions for use with the query function
typedef enum { Waiting2Pair,
               Waiting4Encrypt,
               Waiting4Control } PairingState_t ;


// Public Function Prototypes
bool InitPairingSM ( uint8_t Priority );
bool PostPairingSM( ES_Event ThisEvent );
ES_Event RunPairingSM( ES_Event ThisEvent );

uint8_t getEncryptedCHKSM(void);
int8_t getTurnByte(void);
int8_t getDriveByte(void);
uint8_t getPairAddressLSB(void);
uint8_t getPairAddressMSB(void);
int8_t getDriveByte(void);
uint8_t getDecryptCounter(void);
uint8_t getSpecialByte(void);
uint8_t getTeamNumber(void);
uint8_t* getEncryptionKey(void);
uint8_t getCtrlCheckSum(void);

#endif /* PairingSM_H */
