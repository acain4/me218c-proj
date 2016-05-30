/****************************************************************************
 Module
   MotorControl.c

 Description
   Give all direct controls to lift fan and thrust motors.

 Notes

****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include <xc.h>
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "PIC16F1788.h"
#include "MotorControl.h"
#include <stdio.h>
#include "bitdefs.h"
#include "PairingSM.h"

/*----------------------------- Module Defines ----------------------------*/
#define PWM_FREQ 7500 
#define PR2_VAL (8000000/PWM_FREQ) / (4*4) - 1
#define LSB8_MASK 0xff
#define LSB2_MASK 0x03
#define BITS_5and4_MASK 0x30 
#define FORWARD = 0x01
#define BACKWARD = 0x00

/*---------------------------- Module Prototypes ---------------------------*/
static void InitPWM(void);
static void GetSpeed(void);
static void InitOtherPins(void);

/*---------------------------- Module Variables ---------------------------*/
static uint8_t MyPriority;
static uint8_t MotorSpeed;
static int8_t DriveCommand;

/*------------------------------ Framework Code ------------------------------*/
/***********************************
            Init function
 ***********************************/
bool InitMC ( uint8_t Priority )
{
  MyPriority = Priority;
  // Initialize PWM hardware
  InitPWM();
  // Init other pins
  InitOtherPins();
  return true;
}


/***********************************
            Post function
 ***********************************/
bool PostMC( ES_Event ThisEvent ) 
{
    return ES_PostToService( MyPriority, ThisEvent);
}

/***********************************
            Run function
 ***********************************/
ES_Event RunMC( ES_Event CurrentEvent )
{
    ES_Event ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT; // assume no errors 
    static int16_t RightDuty;
    static int16_t LeftDuty;
    static int16_t RightDuty2Register;
    static int16_t LeftDuty2Register;
    if(CurrentEvent.EventType == ES_INIT){
        //ES_Timer_InitTimer(MC_TIMER,10);
    } else if (CurrentEvent.EventType == ES_DRIVE_COMMAND){
        RightDuty = 100*getDriveByte()/128;
        LeftDuty = 100*getDriveByte()/128;
        RightDuty -= (50*getTurnByte()/128);
        LeftDuty += (50*getTurnByte()/128);
        if (RightDuty > 99){
            RightDuty = 99;
        }
        else if (RightDuty < -99){
            RightDuty = -99;
        }
        if (LeftDuty > 99){
            LeftDuty = 99;
        }
        else if (LeftDuty < -99){
            LeftDuty = -99;
        }
        RightDuty2Register = RightDuty*4*(PR2_VAL+1)/100;   // This values goes from -MAX to MAX, MAX = 4*(PR2_VAL+1))
        LeftDuty2Register = LeftDuty*4*(PR2_VAL+1)/100;     // This values goes from -MAX to MAX, MAX = 4*(PR2_VAL+1))
        //Take care of right motor
        if (RightDuty2Register < 0) {
            RightDuty2Register = (100+RightDuty)*4*(PR2_VAL+1)/100; // This would make it positive and inverts the polarity
            LATC2 = 1;
        } else {
            LATC2 = 0;
        }
        CCPR2L = (RightDuty2Register >> 2) & LSB8_MASK; // mask bits 2-9 of 16-bit integer before writing to 8-bit register
        CCP2CON |= (((RightDuty2Register & (LSB2_MASK)) << 4) & BITS_5and4_MASK);
        //Take care of left motor
        if (LeftDuty2Register < 0) {
            LeftDuty2Register = (100+LeftDuty)*4*(PR2_VAL+1)/100; // This would make it positive and inverts the polarity
            LATC5 = 1;
        } else {
            LATC5 = 0;
        }
        CCPR3L = (LeftDuty2Register >> 2) & LSB8_MASK; // mask bits 2-9 of 16-bit integer before writing to 8-bit register
        CCP3CON |= (((LeftDuty2Register & (LSB2_MASK)) << 4) & BITS_5and4_MASK);    
    }else if (CurrentEvent.EventType == ES_LED){
        if (CurrentEvent.EventParam == 0x01){
            //turn on LED
            LATA1 = 1;
        }else if (CurrentEvent.EventParam == 0x00){
            //turn off LED
            LATA1 = 0;
        }
    }else if (CurrentEvent.EventType == ES_LiftFan){
        if (CurrentEvent.EventParam == 0x01){
            //turn on LiftFan
            LATA0 = 1;
        }else if (CurrentEvent.EventParam == 0x00){
            //turn off LiftFan
            LATA0 = 0;
        }
    }
    return ReturnEvent;
}

/*****************************Helper Functions******************************/
// InitPWM: function to initialize PWM for drive motors
static void InitPWM(){
    /* Notes:
     * CCP3 resets to PC6
     * CCP2 resets to PC1
     */
    
    // Disable CCP1 and CCP2 by setting TRISC1 and TRISC2
    TRISC1 = 0x01;
    TRISC6 = 0x01;
    
    // Load PR2 register with PWM period
    PR2 = PR2_VAL;
    
    // Configure CCP Modules for PWM mode 
    CCP3M0 = 0x00;
    CCP3M1 = 0x00;
    CCP3M2 = 0x01;
    CCP3M3 = 0x01;
    
    CCP2M0 = 0x00;
    CCP2M1 = 0x00;
    CCP2M2 = 0x01;
    CCP2M3 = 0x01;
    
    // Load CCPRxL registers and CCPxCON registers with PWM duty cycle
    CCPR3L = 0x00;//0x0c;
    DC3B0 = 0x00;
    DC3B1 = 0x00; //0x01;
    
    CCPR2L = 0x00;//0x0c;
    DC2B0 = 0x00;
    DC2B1 = 0x00;//0x01;
    
    /* Configure and start Timer2:
     * Clear TMR2IF interrupt flag
     * Config T2CKPS bits of T2CON with prescaler
     * Enable Timer2 by setting TMR2ON in T2CON
     */
    TMR2IF = 0x00;
    T2CKPS0 = 0x00;
    T2CKPS1 = 0x01;
    TMR2ON = 0x01;
    
    // Enable CCP1 and CCP2 via TRISC
    TRISC1 = 0x00;
    TRISC6 = 0x00;
    
    // Enable PC2 and PC5
    TRISC2 = 0x00;
    TRISC5 = 0x00;
}

// GetSpeed: function to decide PWM duty cycle based on input pins
// input pins: C4, C5, C7 (8 different possible speeds)
static void GetSpeed(){
    MotorSpeed = 0x00;
    if(RC4 == 0x01){
        MotorSpeed |= BIT0HI;
        // Debug: toggle A2 
        //LATA2 ^= 1;
    }
    if(RC5 == 0x01){
        MotorSpeed |= BIT1HI;
    }
    if(RC7 == 0x01){
        MotorSpeed |= BIT2HI;
    }
}

//InitOtherPins: function to initialize miscellaneous I/O pins
static void InitOtherPins(){
    // ensure PA0 is cleared to start (this controls lift fan)
	LATA0 = 0;
}