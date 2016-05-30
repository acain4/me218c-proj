/****************************************************************************
 Module
   CommService.c

 Description
   Communication from TIVA to XBee

 Notes

****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
*/
#include <stdio.h>
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_Timers.h"
#include "CommService.h"
#include "PIC16F1788.h"
#include "MotorControl.h"
#include "PairingSM.h"

/*----------------------------- Module Defines ----------------------------*/
#define REQ_PAIR 0x00
#define ENCR_KEY 0x01
#define INCOMING_PCKT 0x81

/*---------------------------- Module Functions ---------------------------*/
static void initBRG( void);
static void initTXUART( void);
static void initRXUART( void);
static void SendPacket(uint8_t);
static void BuildPacket(uint8_t);

/*---------------------------- Module Variables ---------------------------*/
static CommServiceState_t CurrentState;
static uint8_t MyPriority;

static uint8_t ReceiveLength;  // length of array that we are receiving
static uint8_t ReceiveCounter; // index of array that we are currently writing
static uint8_t ReceiveArray[50]; // RecvArray: holds bytes from received packet
static uint8_t ReceiveCheckSum = 0; //keeps running total of data bytes received
static uint8_t RecvDataLength; // length of RF data portion of packet
static uint8_t ThisData;

static uint8_t PACAddressLSB;
static uint8_t PACAddressMSB;

// initialize a pointer to ReceiveArray's location in memory
static uint8_t *recvPointer;

// List different data arrays here
#define PAIRED_NO_ERROR 0x00; // STATUS1
#define PAIRED_DEC_ERROR 0x01; // STATUS2
#define UNPAIRED_NO_ERROR 0x02; // STATUS3
#define UNPAIRED_DEC_ERROR 0x03; // STATUS4
#define DEBUG1 0x04; // DEBUG message
#define DEBUG2 0x05 // DEBUG message

static uint8_t DataArrays[6][6] = {
    // first entry in each row is the length of the data
    {5,   0x03,0x01,0x00} ,          // Paired with no decrypt error
    {3,   0x03,0x03,0x00} ,          // Paired with decrypt error
    {5,   0x03,0x00,0x00} ,          // Not Paired with no decrypt error
    {3,   0x03,0x02,0x00} ,          // Not Paired with decrypt error
    {4,   0x03,0x01,0x00,0x00},         // DEBUG message
    {5,   0x03,0x02,0x00,0x00,0x00}     // DEBUG message
};

// PacketArray to hold transmit data (set to hold max number of possible bytes)
static uint8_t TransmitArray[20];
static uint8_t Checksum = 0;
static uint8_t DataLength = 0;
static uint8_t PacketLength = 0;

/*------------------------------ Module Code ------------------------------*/
/***********************************
            Init function
 ***********************************/
bool InitCommService ( uint8_t Priority )
{
    MyPriority = Priority;
    // put us into the Initial PseudoState
    CurrentState = InitComm;
    // pull in pointer to receive array
    recvPointer = &ReceiveArray[0];
    // init UART hardware
    initBRG();    //Configure the baudrate generator
    initRXUART(); //Init EUSART module for RX
    initTXUART(); //Init EUSART module for TX
    // post event to move into run function
    ES_Event ThisEvent;
    ThisEvent.EventType = ES_INIT;
    PostCommService(ThisEvent);
    return true;
}

/***********************************
            Post function
 ***********************************/
bool PostCommService( ES_Event ThisEvent )
{
  return ES_PostToService( MyPriority, ThisEvent);
}

/***********************************
            Run function
 ***********************************/
ES_Event RunCommService( ES_Event ThisEvent )
{
    ES_Event ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT; // assume no errors
    CommServiceState_t NextState = CurrentState;
    /******************   Transmitting   ********************/
    if ( ThisEvent.EventType == ES_STATUS1 ){
        ThisData = PAIRED_NO_ERROR;
        DataArrays[ThisData][0x03] = getEncryptedCHKSM();
        SendPacket(ThisData);
    } else if (ThisEvent.EventType == ES_STATUS3) {
        ThisData = UNPAIRED_NO_ERROR;
        DataArrays[ThisData][0x03] = getEncryptedCHKSM();
        SendPacket(ThisData);
    } else if (ThisEvent.EventType == ES_STATUS4) {
        ThisData = UNPAIRED_DEC_ERROR;
        DataArrays[ThisData][0x03] = getEncryptedCHKSM();
        SendPacket(ThisData);
    } else if (ThisEvent.EventType == ES_DEBUG1) {
        // DEBUGGING MESSAGE 
        ThisData = DEBUG1;
        DataArrays[ThisData][0x03] = getEncryptedCHKSM();
        DataArrays[ThisData][0x04] = *(getEncryptionKey() + 31);
        SendPacket(ThisData);
    } else if (ThisEvent.EventType == ES_DEBUG2) {
        // DEBUGGING MESSAGE 
        ThisData = DEBUG2;
        DataArrays[ThisData][0x03] = getEncryptedCHKSM();
        DataArrays[ThisData][0x04] = getCtrlCheckSum();
        DataArrays[ThisData][0x05] = getCtrlCheckSum2();
        SendPacket(ThisData);
    }
    /********************   Receiving   *********************/
    switch ( CurrentState )
    {
        case InitComm: 
            if ( ThisEvent.EventType == ES_INIT ){ 
                NextState = WaitFor7E;
                ES_Timer_InitTimer( CommTimer, 100 );
            }
        break;
        case WaitFor7E:
            if ( ThisEvent.EventType == ES_ReceivedByte && ThisEvent.EventParam == 0x7E){  
                NextState = WaitForMSB;
                ES_Timer_InitTimer( CommTimer, 200);
            }
        break;
        case WaitForMSB:  
            if ( ThisEvent.EventType == ES_ReceivedByte && ThisEvent.EventParam == 0x0){  
                NextState = WaitForLSB;
                ES_Timer_InitTimer(CommTimer, 200);
             }
            else if ( ThisEvent.EventType == ES_ReceivedByte && ThisEvent.EventParam != 0x0){  
                NextState = WaitFor7E;
             }
            else if ( ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == CommTimer){  
                NextState = WaitFor7E;
             }
        break;
        case WaitForLSB:
            if ( ThisEvent.EventType == ES_ReceivedByte){  
                ReceiveLength = ThisEvent.EventParam;
                ReceiveCounter = ReceiveLength;
                ReceiveCheckSum = 0;
                NextState = SuckUpPacket;
                ES_Timer_InitTimer(CommTimer, 200);
            }
            else if ( ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == CommTimer){  
                NextState = WaitFor7E;
             }
         break;
         case SuckUpPacket:
            if ( ThisEvent.EventType == ES_ReceivedByte){  
                if(ReceiveCounter != 0){
                    ReceiveArray[ReceiveLength-ReceiveCounter] = ThisEvent.EventParam;
                    ReceiveCheckSum += ThisEvent.EventParam;
                    ReceiveCounter--;
                    ES_Timer_InitTimer(CommTimer, 200);
                } else{
                    NextState = WaitFor7E;
                    if (ReceiveCheckSum + ThisEvent.EventParam != 0xFF){
                        //Raise a flag for bad checksum
                        LATA3 = 1; // Using RA3 for indicating checksum error
                    } else{
                        LATA3 = 0;
                    }
                    // if received packet is of type "incoming packet"
                    if (ReceiveArray[0] == INCOMING_PCKT){
                        // pull in address LSB and MSB
                        PACAddressMSB = ReceiveArray[1];
                        PACAddressLSB = ReceiveArray[2];
                        RecvDataLength = ReceiveLength - 5;
                        // post to PairingSM to let it know we got a new packet
                        ThisEvent.EventType = ES_NEW_PACKET;
                        ThisEvent.EventParam = RecvDataLength; // pass length of RF data as event parameter
                        PostPairingSM(ThisEvent);
                    }
                }
            
            } else if ( ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == CommTimer){  
                NextState = WaitFor7E;
            }
            break;
    }
    CurrentState = NextState;                  
    return ReturnEvent;
}


/***************************************************************************
 private functions
 ***************************************************************************/
//Init EUSART module for Transmitting
static void initTXUART( void){
    //GPIO config for TX
    ANSB6  = 0;      //Set TX pin to digital
    TRISB6 = 0;     //Set TX pin to output
    TXSEL  = 1;      //Select RB6 to be TX pin 
    SPEN   = 1;       //Serial port enable
    TXEN   = 1;       //Enable TX
}

//Init EUSART module for Receiving
static void initRXUART( void){
    GIE = 0;
    //GPIO config for RX
    //ANSB7 = 0;      //RB7 is default to digital
    TRISB7 = 1;      //Set RX pin to input
    RXSEL  = 1;      //Select RB7 to be RX pin 
    SPEN   = 1;      //Serial port enable
    RCIE   = 1;      //RX interrupt enable
    PEIE   = 1;      //Peripheral interrupt enable
    GIE    = 1;      //Global interrupt enable
    CREN   = 1;      //Enable RX
}

//Setup the baudrate generator
static void initBRG( void){

    //Configure to 8-bit/async Fosc/[16(n+1)]
    BRGH = 1;
    BRG16 = 0;
    SYNC = 0;   //Enable Async comm
    //Configure baudrate to 9600
    SP1BRGL = 0xD0;

}

/* 
BuildPacket: takes a data array and builds it into a full transmission packet

input parameters: uint8_t WhichData (which data to use)

example function call: 
    BuildPacket(TESTDATA1);
*/

static void BuildPacket(uint8_t WhichData){
    // pull in length of data to transmit
    DataLength = DataArrays[WhichData][0];
    // calculate total packet length (data length + 9)
    PacketLength = DataLength + 9;
    /********************   build PacketArray   *****************/
    // start delimiter = 0x7E
    TransmitArray[0] = 0x7e;
    // MSB of length = 0x00
    TransmitArray[1] = 0x00;
    // LSB of length = DataLength+5
    TransmitArray[2] = DataLength+5;
    // API Identifier = 0x01
    TransmitArray[3] = 0x01;
    // Frame ID = 0x00
    TransmitArray[4] = 0x00;
    // Destination Address = 0x2182 or 0x2082 for our Xbees
    TransmitArray[5] = getPairAddressMSB(); 
    TransmitArray[6] = getPairAddressLSB();
    // Options byte = 0x00
    TransmitArray[7] = 0x00;
    // RF Data
    for(int i=0; i<DataLength; i++){
        TransmitArray[8+i] = DataArrays[WhichData][i+1];
    }
    // Checksum
    Checksum = 0;
    for(int i=3; i<8+DataLength; i++){
        Checksum += TransmitArray[i];
    }
    Checksum = 0xff - Checksum;
    TransmitArray[8+DataLength] = Checksum;
    /****************   end: build PacketArray   ****************/
}

/*
SendPacket: transmits a full packet to Xbee via UART

input parameters: uint8_t WhichData (which data to use)

example function call: 
    SendPacket(TESTDATA1);
*/
static void SendPacket(uint8_t WhichData){
    // build the packet
    BuildPacket(WhichData);
    // send each byte of the packet
    for(int i=0; i<PacketLength; i++){
        // load data register
        TX1REG = TransmitArray[i];
        // wait for data register to empty
        while( TRMT == 0 );
         
    }
}

uint8_t* getRecvArray(void){
    return ReceiveArray;
}

uint8_t getPACAddressLSB(void){
    return PACAddressLSB;
}

uint8_t getPACAddressMSB(void){
    return PACAddressMSB;
}