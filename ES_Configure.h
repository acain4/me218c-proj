/****************************************************************************
 Module
     ES_Configure.h
 Description
     This file contains macro definitions that are edited by the user to
     adapt the Events and Services framework to a particular application.
 Notes
     
 History
 When           Who     What/Why
 -------------- ---     --------
 10/21/13 20:54 jec      lots of added entries to bring the number of timers
                         and services up to 16 each
 08/06/13 14:10 jec      removed PostKeyFunc stuff since we are moving that
                         functionality out of the framework and putting it
                         explicitly into the event checking functions
 01/15/12 10:03 jec      started coding
*****************************************************************************/

#ifndef CONFIGURE_H
#define CONFIGURE_H

/****************************************************************************/
// The maximum number of services sets an upper bound on the number of 
// services that the framework will handle. Reasonable values are 8 and 16
// corresponding to an 8-bit(uint8_t) and 16-bit(uint16_t) Ready variable size
#define MAX_NUM_SERVICES 8

/****************************************************************************/
// This macro determines that nuber of services that are *actually* used in
// a particular application. It will vary in value from 1 to MAX_NUM_SERVICES
#define NUM_SERVICES 5

/****************************************************************************/
// These are the definitions for Service 0, the lowest priority service.
// Every Events and Services application must have a Service 0. Further 
// services are added in numeric sequence (1,2,3,...) with increasing 
// priorities
// the header file with the public function prototypes
#define SERV_0_HEADER "Blink.h" // this is a debugging service
// the name of the Init function
#define SERV_0_INIT InitBlink
// the name of the run function
#define SERV_0_RUN RunBlink
// How big should this services Queue be?
#define SERV_0_QUEUE_SIZE 2

/****************************************************************************/
// The following sections are used to define the parameters for each of the
// services. You only need to fill out as many as the number of services 
// defined by NUM_SERVICES
/****************************************************************************/
// These are the definitions for Service 1
#if NUM_SERVICES > 1
// the header file with the public function prototypes
#define SERV_1_HEADER "CommService.h"
// the name of the Init function
#define SERV_1_INIT InitCommService
// the name of the run function
#define SERV_1_RUN RunCommService
// How big should this services Queue be?
#define SERV_1_QUEUE_SIZE 5
#endif

/****************************************************************************/
// These are the definitions for Service 2
#if NUM_SERVICES > 2
// the header file with the public function prototypes
#define SERV_2_HEADER "ButtonDebounce.h" // this is a debugging-aid service
// the name of the Init function
#define SERV_2_INIT InitButtonDB
// the name of the run function
#define SERV_2_RUN RunButtonDB
// How big should this services Queue be?
#define SERV_2_QUEUE_SIZE 5
#endif

/****************************************************************************/
// These are the definitions for Service 3
#if NUM_SERVICES > 3
// the header file with the public function prototypes
#define SERV_3_HEADER "MotorControl.h"
// the name of the Init function
#define SERV_3_INIT InitMC
// the name of the run function
#define SERV_3_RUN RunMC
// How big should this services Queue be?
#define SERV_3_QUEUE_SIZE 5
#endif

/****************************************************************************/
// These are the definitions for Service 4
#if NUM_SERVICES > 4
// the header file with the public function prototypes
#define SERV_4_HEADER "PairingSM.h"
// the name of the Init function
#define SERV_4_INIT InitPairingSM
// the name of the run function
#define SERV_4_RUN RunPairingSM
// How big should this services Queue be?
#define SERV_4_QUEUE_SIZE 5
#endif

/****************************************************************************/
// These are the definitions for Service 5
#if NUM_SERVICES > 5
// the header file with the public function prototypes
#define SERV_5_HEADER "Service5.h"
// the name of the Init function
#define SERV_5_INIT InitService5
// the name of the run function
#define SERV_5_RUN RunService5
// How big should this services Queue be?
#define SERV_5_QUEUE_SIZE 5
#endif

/****************************************************************************/
// These are the definitions for Service 6
#if NUM_SERVICES > 6
// the header file with the public function prototypes
#define SERV_6_HEADER "Service6.h"
// the name of the Init function
#define SERV_6_INIT InitService6
// the name of the run function
#define SERV_6_RUN RunService6
// How big should this services Queue be?
#define SERV_6_QUEUE_SIZE 3
#endif

/****************************************************************************/
// These are the definitions for Service 7
#if NUM_SERVICES > 7
// the header file with the public function prototypes
#define SERV_7_HEADER "Service7.h"
// the name of the Init function
#define SERV_7_INIT InitService7
// the name of the run function
#define SERV_7_RUN RunService7
// How big should this services Queue be?
#define SERV_7_QUEUE_SIZE 3
#endif

/****************************************************************************/
// Name/define the events of interest
// Universal events occupy the lowest entries, followed by user-defined events
typedef enum {  ES_NO_EVENT = 0,
                ES_ERROR,  /* used to indicate an error from the service */
                ES_INIT,   /* used to transition from initial pseudo-state */
                ES_NEW_KEY, /* signals a new key received from terminal */
                ES_TIMEOUT, /* signals that the timer has expired */
                ES_ENTRY,
                ES_ENTRY_HISTORY,
                ES_EXIT,
                /* User-defined events start here */
                ES_DEBUG1, 
                ES_DEBUG2, 
                ES_DECRYPT_ERROR, 
                ES_MANUAL_UNPAIR, 
                ES_DRIVE_COMMAND, 
                ES_STATUS1, // paired, no decrypt error
                ES_STATUS2, // paired, decrypt error
                ES_STATUS3, // unpaired, no decrypt error
                ES_STATUS4, // unpaired, decrypt error
                ES_NEW_PACKET, 
                ES_Transmit, /* command comm to transmit */
                ES_ReceivedByte, /* received a byte */
				ES_LOCK,
				ES_UNLOCK,
				ES_BUTTON_DOWN,
				ES_BUTTON_UP,
                ES_Fan1,    //Events for checkpoint 1
                ES_Fan2,    //Events for checkpoint 1
                ES_LED,     //Events for checkpoint 1
                ES_LiftFan,  //Events for checkpoint 1
                ES_ADCNewRead //Posted from ISR to PairingSM after a new ADC conversion is done
                } ES_EventTyp_t ;

/****************************************************************************/
// These are the definitions for the Distribution lists. Each definition
// should be a comma separated list of post functions to indicate which
// services are on that distribution list.
#define NUM_DIST_LISTS 0
#if NUM_DIST_LISTS > 0 
#define DIST_LIST0 PostMapKeys, PostMasterSM
#endif
#if NUM_DIST_LISTS > 1 
#define DIST_LIST1 PostTemplateFSM, TemplateFSM
#endif
#if NUM_DIST_LISTS > 2 
#define DIST_LIST2 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 3 
#define DIST_LIST3 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 4 
#define DIST_LIST4 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 5 
#define DIST_LIST5 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 6 
#define DIST_LIST6 PostTemplateFSM
#endif
#if NUM_DIST_LISTS > 7 
#define DIST_LIST7 PostTemplateFSM
#endif

/****************************************************************************/
// This are the name of the Event checking funcion header file. 
#define EVENT_CHECK_HEADER "ButtonDebounce.h"

/****************************************************************************/
// This is the list of event checking functions 
#define EVENT_CHECK_LIST CheckButtonEvents

/****************************************************************************/
// These are the definitions for the post functions to be executed when the
// corresponding timer expires. All 16 must be defined. If you are not using
// a timer, then you should use TIMER_UNUSED
// Unlike services, any combination of timers may be used and there is no
// priority in servicing them
#define TIMER_UNUSED ((pPostFunc)0)
#define TIMER0_RESP_FUNC PostBlink
#define TIMER1_RESP_FUNC PostCommService
#define TIMER2_RESP_FUNC PostButton
#define TIMER3_RESP_FUNC PostMC
#define TIMER4_RESP_FUNC PostPairingSM
#define TIMER5_RESP_FUNC PostPairingSM
#define TIMER6_RESP_FUNC PostPairingSM
#define TIMER7_RESP_FUNC TIMER_UNUSED

/****************************************************************************/
// Give the timer numbers symbolc names to make it easier to move them
// to different timers if the need arises. Keep these definitions close to the
// definitions for the response functions to make it easier to check that
// the timer number matches where the timer event will be routed
// These symbolic names should be changed to be relevant to your application 

#define BlinkTimer 0
#define CommTimer  1
#define DBTimer   2
#define MC_TIMER 3
#define PAIR_TIMER 4
#define XMIT_TIMER 5
#define ADC_TIMER 6

#endif /* CONFIGURE_H */
