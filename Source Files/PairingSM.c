/****************************************************************************
 Module
   PairingSM.c

 Description
   State machine to control process of pairing with a controller.

 Notes

****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Timers.h"
#include "bitdefs.h"
#include "CommService.h"
#include "PIC16F1788.h"
#include "PairingSM.h"
#include "MotorControl.h"

/*----------------------------- Module Defines ----------------------------*/
#define PAIR_TIMEOUT 45000 // amount of time before pairing times out (in ms)
#define XMIT_TIMEOUT 2000

#define UNPAIRED 0x00
#define RED_TEAM 0x01
#define BLUE_TEAM 0x02
#define EBRAKE 0x03

#define TEAM_NUMBER_MASK BIT7LO

#define FORWARD = 0x01
#define BACKWARD = 0x00

/*---------------------------- Module Prototypes ---------------------------*/
static void IncrementCounter(void);
static void UpdateSmallPIC(uint8_t PairStatus);
static void InitADC(void);
static void GetADC(void);

/*---------------------------- Module Variables ---------------------------*/
static ES_Event ThatEvent;
static PairingState_t CurrentState;
static uint8_t MyPriority;

static uint8_t DataLength;
static uint16_t PairAddressMSB;
static uint8_t PairAddressLSB;
static uint8_t LastPairMSB;
static uint8_t LastPairLSB;

static uint8_t currTeam = 0;

static int8_t DriveByte;
static int8_t TurnByte;
static uint8_t SpecialByte;
static int16_t DriveCommand;

static int8_t DriveLeft;
static int8_t DriveRight;

static uint8_t EncryptionKey[32];
static uint8_t DecryptCounter;
static uint8_t ControlSum = 0;
static uint8_t EncryptedCHKSM;

static bool isLiftFanOn = false;

static uint8_t RawADCValue = 0; 
static uint8_t TeamNumber = 6; // start off as team 6 (arbitrary, for debugging)

// initialize a pointer to ReceiveArray's location in memory
static uint8_t *recvPointer;

/*------------------------------ Framework Code ------------------------------*/
/***********************************
            Init function
 ***********************************/
bool InitPairingSM ( uint8_t Priority )
{
    MyPriority = Priority;
    // put us into Waiting2Pair
    CurrentState = Waiting2Pair;
    // change output pins to PIC12F752 to reflect unpaired state
    UpdateSmallPIC(UNPAIRED);
    // get into state machine
    ES_Event ThisEvent;
    ThisEvent.EventType = ES_INIT;
    PostPairingSM(ThisEvent);
    // init ADC module
    InitADC();
    // start ADC timer to timeout after acquisition delay (1 ms)
    ES_Timer_InitTimer(ADC_TIMER, 1);
    return true;
}
/***********************************
            Post function
 ***********************************/
bool PostPairingSM( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}
/***********************************
            Run function
 ***********************************/
ES_Event RunPairingSM( ES_Event ThisEvent )
{
    ES_Event ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
    // pull in pointer to receive array
    recvPointer = getRecvArray();
    // enter state machine
    switch ( CurrentState )
    {
        case Waiting2Pair:
            if (ThisEvent.EventType == ES_INIT){
                // turn off drive propeller motors
                DriveLeft = 0;
                DriveRight = 0;
                ThatEvent.EventType = ES_DRIVE_COMMAND;
                ThatEvent.EventParam = 0x01; // FORWARD
                PostMC(ThatEvent);
            }
            else if((ThisEvent.EventType == ES_TIMEOUT)&&(ThisEvent.EventParam == ADC_TIMER)){
                GetADC(); // Start the process to read the pin to determine TeamNumber
                ES_Timer_InitTimer(ADC_TIMER,500); //Set a timer for when we should read the pin again
            }
            else if(ThisEvent.EventType == ES_ADCNewRead){
                RawADCValue = ThisEvent.EventParam;
                if ((RawADCValue > 70)&&(RawADCValue < 100)){ //2% tolerance on the expected value of 85.33 //83 to 88
                    TeamNumber = 0; //Team 1
                }
                else if ((RawADCValue > 101)&&(RawADCValue < 139)){ //2% tolerance on the expected value of 125 //125 to 131
                    TeamNumber = 1; //Team 2
                }
                else if ((RawADCValue > 140)&&(RawADCValue < 162)){ //2% tolerance on the expected value of 153.6 //150  to 157
                    TeamNumber = 2; //Team 3
                }
                else if ((RawADCValue > 163)&&(RawADCValue < 190)){ //2% tolerance on the expected value of 170.66 //167 to 174
                    TeamNumber = 3; // Team 4
                }
                else{
                    TeamNumber = 6; // no assigned team
                }
            }
            // else if we just got a pairing request
            else if( ThisEvent.EventType == ES_NEW_PACKET 
                    && *(recvPointer + 5) == 0x00 
                    && *(recvPointer+1) != LastPairMSB
                    && *(recvPointer+2) != LastPairLSB ){
                //check that they are trying to pair with my number
                if (((*(recvPointer + 6))&TEAM_NUMBER_MASK) != TeamNumber){
                    break;
                }
                // update Small PIC to display pairing status
                if (((*(recvPointer + 6))&BIT7HI) == BIT7HI){
                    UpdateSmallPIC(BLUE_TEAM);
                    currTeam = BLUE_TEAM;
                } else if (((*(recvPointer + 6))&BIT7HI) == 0x00){
                    UpdateSmallPIC(RED_TEAM);
                    currTeam = RED_TEAM;
                }
                // change states
                CurrentState = Waiting4Encrypt;
                // pull in length of RF data
                DataLength = ThisEvent.EventParam;
                // start a 45 s timer
                ES_Timer_InitTimer(PAIR_TIMER,PAIR_TIMEOUT);
                // start a 1 s timer
                ES_Timer_InitTimer(XMIT_TIMER,XMIT_TIMEOUT);
                // start lift fan
                ThisEvent.EventType = ES_LiftFan;
                ThisEvent.EventParam = 0x01;
                PostMC(ThisEvent);
                // Turn on LED to indicate pairing success
                //LATA1 = 1;
                // Transmit status message back to PAC (STATUS1)
                PairAddressMSB = *(recvPointer+1);
                PairAddressLSB = *(recvPointer+2);
                LastPairMSB = PairAddressMSB;
                LastPairLSB = PairAddressLSB;
                ThisEvent.EventType = ES_STATUS1;
                ThisEvent.EventParam = ((PairAddressMSB<<8) & 0xff00) + (PairAddressLSB & 0xff); // pass address of paired PAC as event parameter
                PostCommService(ThisEvent);
            }
        break;
           
        case Waiting4Encrypt:
            // if we got a transmit timeout
            if (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == XMIT_TIMER){
                // change states
                CurrentState = Waiting2Pair;
                //start ADC timer
                ES_Timer_InitTimer(ADC_TIMER,500);
                // update small PIC to display unpaired status
                UpdateSmallPIC(UNPAIRED);
                // deactivate lift fan
                ThisEvent.EventType = ES_LiftFan;
                ThisEvent.EventParam = 0x00;
                PostMC(ThisEvent);
                // turn off pairing LED
                //LATA1 = 0;
                // Stop pairing timer
                ES_Timer_StopTimer(PAIR_TIMER);
                // Stop xmit timer
                ES_Timer_StopTimer(XMIT_TIMER);
                // Transmit status back to PAC (STATUS3)
                ThisEvent.EventType = ES_STATUS3;
                ThisEvent.EventParam = ((PairAddressMSB<<8) & 0xff00) + (PairAddressLSB & 0xff); // pass address of paired PAC as event parameter
                PostCommService(ThisEvent);
            } 
            // else if we received an encryption key
            else if ((ThisEvent.EventType == ES_NEW_PACKET && *(recvPointer + 5) == 0x01)
                    && PairAddressLSB == getPACAddressLSB()
                    && PairAddressMSB == getPACAddressMSB() ){
                // change states
                CurrentState = Waiting4Control;
                // set decryption counter to 0
                DecryptCounter = 0;
                // save encryption key
                for(int i=0; i<32; i++){
                    EncryptionKey[i] = *(recvPointer+(i+6));
                }
                // restart 1s transmit timer
                ES_Timer_InitTimer(XMIT_TIMER,XMIT_TIMEOUT);
                // transmit status back to PAC (STATUS1)
                //ThisEvent.EventType = ES_STATUS1;
                ThisEvent.EventType = ES_DEBUG1;
                ThisEvent.EventParam = ((PairAddressMSB<<8) & 0xff00) + (PairAddressLSB & 0xff); // pass address of paired PAC as event parameter
                PostCommService(ThisEvent);
            }
        break;
        
        case Waiting4Control:
            // if we received a new command
            if(((ThisEvent.EventType == ES_NEW_PACKET) && ((*(recvPointer + 5)) ^ EncryptionKey[DecryptCounter]) == 0x02)
                    && (PairAddressLSB == getPACAddressLSB())
                    && (PairAddressMSB == getPACAddressMSB()) ){

                // store encrypted checksum value
                EncryptedCHKSM = *(recvPointer + 9);
                // restart xmit timer
                ES_Timer_InitTimer(XMIT_TIMER,XMIT_TIMEOUT);
                // transmit status back to PAC (STATUS1)
                ThisEvent.EventType = ES_STATUS1;
                ThisEvent.EventParam = ((PairAddressMSB<<8) & 0xff00) + (PairAddressLSB & 0xff); // pass address of paired PAC as event parameter
                PostCommService(ThisEvent);
                // decrypt data and compare checksum
                ControlSum = 0;
                for (int i=5; i<9; i++){
                    if (i==6){
                        DriveByte = (*(recvPointer + 6)) ^ EncryptionKey[DecryptCounter];
                    }
                    if (i==7){
                        TurnByte = (*(recvPointer + 7)) ^ EncryptionKey[DecryptCounter];
                    }
                    if (i==8){
                        SpecialByte = (*(recvPointer + 8)) ^ EncryptionKey[DecryptCounter];
                    }
                    ControlSum += (*(recvPointer + i)) ^ EncryptionKey[DecryptCounter];
                    IncrementCounter();
                }
                // if checksum is bad, post ES_DECRYPT_ERROR to self
                if ((ControlSum) != ((*(recvPointer + 9)) ^ EncryptionKey[DecryptCounter])){
                //if ((0xff - ControlSum) != ((*(recvPointer + 9)) ^ EncryptionKey[DecryptCounter])){
                    ThisEvent.EventType = ES_DECRYPT_ERROR;
                    PostPairingSM(ThisEvent);
                }
                IncrementCounter();
                /* execute commands:
                    - motor commands
                    - other special actions
                 */
                /* motor commands: map received drive and turn bytes to values that 
				 * we can use to generate intuitive PWM duty cycles
				 */
                DriveRight = DriveByte;
                DriveLeft = DriveByte;
                if (DriveByte>0){
                    if (TurnByte < 0){
                        DriveRight -= TurnByte;
                        if (DriveRight < 0){
                            DriveRight = 0;
                        }
                    } else {
                        DriveLeft += TurnByte;
                        if (DriveLeft < 0){
                            DriveLeft = 0;
                        }
                    }
                }else{
                    if (TurnByte > 0){
                        DriveLeft += TurnByte;
                        if (DriveLeft > 0){
                            DriveLeft = 0;
                        } else {
                            DriveLeft *= -1;
                        }
                        DriveRight *= -1;
                    } else {
                        DriveRight -= TurnByte;
                        if (DriveRight > 0){
                            DriveRight = 0;
                        } else {
                            DriveRight *= -1;
                        }
                        DriveLeft *= -1;
                    }
                }
                ThisEvent.EventType = ES_DRIVE_COMMAND;
                PostMC(ThisEvent);
                // Special actions: e-brake and unpair  
                if ((SpecialByte & 0x01) == 0x01){
                    // deactivate lift fan
                    ThatEvent.EventType = ES_LiftFan;
                    ThatEvent.EventParam = 0x00;
                    PostMC(ThatEvent);
                    isLiftFanOn = false;
                    // update small PIC with e-brake state
                    UpdateSmallPIC(EBRAKE);
                } else if (!isLiftFanOn){
                    ThatEvent.EventType = ES_LiftFan;
                    ThatEvent.EventParam = 0x01;
                    PostMC(ThatEvent);
                    isLiftFanOn = true;
                    // update small PIC with current team color
                    UpdateSmallPIC(currTeam);
                }
                if ((SpecialByte & 0x02) == 0x02){
                    // unpair
                    ThisEvent.EventType = ES_MANUAL_UNPAIR;
                    PostPairingSM(ThisEvent);
                } 
            }
            /* unpair if we got any of the following events: 
                - pairing timeout
                - xmit timeout
                - PAC manual unpair
                - failed decryption
             */
            else if (  (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == PAIR_TIMER)
                    || (ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == XMIT_TIMER)
                    || (ThisEvent.EventType == ES_MANUAL_UNPAIR)
                    || (ThisEvent.EventType == ES_DECRYPT_ERROR) ){
                // change state
                CurrentState = Waiting2Pair;
                //start ADC timer
                ES_Timer_InitTimer(ADC_TIMER,500);
                // update small PIC to display unpaired status
                UpdateSmallPIC(UNPAIRED);
                // deactivate lift fan
                ThatEvent.EventType = ES_LiftFan;
                ThatEvent.EventParam = 0x00;
                PostMC(ThatEvent);
                // turn off drive propeller motors
                DriveByte = 0;
                TurnByte = 0;
                ThatEvent.EventType = ES_DRIVE_COMMAND;
                ThatEvent.EventParam = 0x01; // FORWARD
                PostMC(ThatEvent);
                // turn off pairing LED
                //LATA1 = 0;
                // disable pairing timer
                ES_Timer_StopTimer(PAIR_TIMER);
                // disable xmit timer
                ES_Timer_StopTimer(XMIT_TIMER);
                // transmit status back to PAC
                if (ThisEvent.EventType == ES_DECRYPT_ERROR){
                    //ThisEvent.EventType = ES_STATUS4; // unpaired, decrypt error
                    ThisEvent.EventType = ES_DEBUG2;
                    ThisEvent.EventParam = ((PairAddressMSB<<8) & 0xff00) + (PairAddressLSB & 0xff); // pass address of paired PAC as event parameter
                } else {
                    ThisEvent.EventType = ES_STATUS3; // unpaired, no decrypt error
                    ThisEvent.EventParam = ((PairAddressMSB<<8) & 0xff00) + (PairAddressLSB & 0xff); // pass address of paired PAC as event parameter
                }
                PostCommService(ThisEvent);
            }
        break;
    }
    return ReturnEvent;
}

/*---------------------------- Helper Functions ---------------------------*/
static void IncrementCounter(void){
    if (DecryptCounter == 31){
        DecryptCounter = 0;
    } else {
        DecryptCounter ++;
    }
}

static void UpdateSmallPIC(uint8_t PairStatus){
    if (PairStatus == UNPAIRED){
        LATA6 = 0;
        LATA7 = 0;
    } else if (PairStatus == RED_TEAM){
        LATA6 = 1;
        LATA7 = 0;
    } else if (PairStatus == BLUE_TEAM){
        LATA6 = 0;
        LATA7 = 1;
    } else if (PairStatus == EBRAKE){
        LATA6 = 1;
        LATA7 = 1;
    }
}

static void InitADC(void){
    //Port configuration
    ANSB3  = 1;   //Set pin B3 to analog
    TRISB3 = 1;   //Set pin B3 to input
    WPUB3 = 0;     //Disable weak pull-up on B3
    //Channel Selection
    CHS0 = 1;
    CHS1 = 0;
    CHS2 = 0;
    CHS3 = 1;
    CHS4 = 0;     //Select AN9 (pin B3) (the CSH<4:0> bits of the ADCON0 register determine which positive channel is selected))
    CHSN0 = 1;
    CHSN1 = 1;
    CHSN2 = 1;
    CHSN3 = 1;    //Use a single ended ADC converter by setting all of CHSN<3:0> bits in ADCON2 register
    //ADC Voltage Reference Selection
    ADPREF0 = 0;
    ADPREF1 = 0;  // we want to choose VDD for positive reference by using the ADPREF bits of the ADCON1 register
    ADNREF = 0;   // choose VSS for negative reference by using the ADNREF bits of the ADCON1 register
    //ADC Conversion Clock Source
    ADCS2 = 0;
    ADCS1 = 0;
    ADCS0 = 1;  // use FOSC/8, 16, or 32  (these are in ADCON1; we are going to choose FOSC/8, so that we can get the fastest possible conversion)
    //Interrupt Control
    ADIF = 0;   //Clear the ADC interrupt flag
    ADIE = 1;   //Enable interrupt upon the completion of a conversion (This is in the PIE1 register))
    PEIE = 1;   //Enable peripheral interrupts
    GIE = 1;   //Enable global interrupts
    //Result Formatting
    ADFM = 0; //Format the results as sign and magnitude so that we can just read the high byte and still get 8-bit resolution
    ADRMD = 1; //Choose 10 bit mode for a faster conversion
    //Enable the ADC
    ADON = 1;
}

static void GetADC(void){
    GO_nDONE = 1; //Set the bit in ADCON0 to start a conversion
}

// public getter functions to allow other modules to see this module's private variables
// this functionality was mainly for debugging
uint8_t* getEncryptionKey(void){
    return EncryptionKey;
}

uint8_t getEncryptedCHKSM(void){
    return EncryptedCHKSM;
}


uint8_t getCtrlCheckSum(void){
    return (ControlSum);
}

uint8_t getCtrlCheckSum2(void){
    return (*(recvPointer + 9)) ^ EncryptionKey[DecryptCounter-1];
}

int8_t getTurnByte(void){
    return TurnByte;
}

int8_t getDriveByte(void){
    return DriveByte;
}

uint8_t getPairAddressLSB(void){
    return PairAddressLSB;
}

uint8_t getPairAddressMSB(void){
    return PairAddressMSB;
}

uint8_t getDecryptCounter(void){
    return DecryptCounter;
}

uint8_t getSpecialByte(void){
    return SpecialByte;
}

uint8_t getTeamNumber(void){
    return TeamNumber;
}