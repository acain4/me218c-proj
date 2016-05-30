// This is a guard condition so that contents of this file are not included
// more than once.  
#ifndef MC_H
#define	MC_H

#include <xc.h>
#include "ES_Events.h"
#include "ES_Types.h"

// Public Function Prototypes
bool InitMC ( uint8_t Priority );
bool PostMC( ES_Event ThisEvent );
ES_Event RunMC( ES_Event CurrentEvent );

#endif	/* MC_H */


