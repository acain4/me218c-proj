;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;             PROGRAM START               ;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	
	list P=PIC12F752
	#include "p12f752.inc"
	__config (_CP_OFF & _WDT_OFF & _PWRTE_ON & _FOSC_INT & _MCLRE_OFF)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; define constants
; Define bit numbers of port A
LED1		 	equ 0x00 ; 0th bit (red team LED)
LED2		 	equ 0x01 ; 1st bit (blue team LED)
INPUT1			equ 0x02 ; 2nd bit
INPUT2			equ 0x03 ; 3rd bit
SERVO		 	equ 0x04 ; 4th bit
DEBUG			equ 0x05 ; 5th bit

; RAM locations
LastTime		equ 0x70 ; holds last high-time of servo PWM
PS_Cntr			equ 0x71 ; holds value of HLTMR postcale counter
HighTime		equ 0x72 ; holds current deired high-time for servo PWM
Rdy2Raise		equ 0x73 ; holds software flag for guarding physical flag-raising action

; ISR PUSH/POP RAM locations
WREG_TEMP		equ 0x7D
STATUS_TEMP 	equ 0x7E
PCLATH_TEMP		equ 0x7F 

; Other constants

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; program memory organization

			org 0
			goto InitFunc
			org 4
			goto GenISR

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; Define tables
			; list any tables of contants here

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; Debug routine
Debug:
			; set and clear debug line
			banksel LATA
			bsf LATA, DEBUG
			NOP
			NOP
			NOP
			bcf LATA, DEBUG	
		return
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; InitFunc
InitFunc:
			; configure OSCCON to 4 MHz oscillator
			banksel OSCCON
			bsf OSCCON, 0x05
			bcf OSCCON, 0x04

			; init variables
			call InitVars
			; init port A pins
			call InitPins
			; init interrupts
			call InitInts
			goto Main
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; Main
Main: 
			; check states of input pins, respond accordingly
			; 	if INPUT1 is low: (case1)
			;		if INPUT 2 is low: (case1A)
			;			- Unpaired
			; 		else: (case1B)
			;			- Blue Team
			; 	else: (case2)
			; 		if INPUT 2 is low: (case2A)
			;			- Red Team
			;		else: (case2B)
			;			- *UNIMPLEMENTED* 

			banksel PORTA
			btfsc PORTA, INPUT1
			goto case2

case1:
				btfsc PORTA, INPUT2
				goto case1B
case1A				call Unpaired
					goto Done
case1B				call BlueTeam
					goto Done

case2:
				banksel PORTA
				btfsc PORTA, INPUT2
				goto case2B
case2A				call RedTeam
					goto Done
case2B				goto Done

Done:
			goto Main


;______________________________________________________________________ Initialization Routines
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; InitPins
InitPins: 
			; disable analog functionality of all pins
			banksel ANSELA
			clrf ANSELA
			; set LATA as desired before enabling outputs
			banksel LATA
			movlw b'00000000' ; 00 00 00 00
			movwf LATA
			; enable outputs on port A
			banksel TRISA
			movlw b'00001100' ; 00 00 11 00
			movwf TRISA
		return

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; InitInts
InitInts:
			; enable peripheral interrupts
			bsf INTCON, PEIE
			; enable global interrupts
			bsf INTCON, GIE	
		return	
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; InitTimers
;InitTimers:
			; initialize timers, if necessary
		;return
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; InitVars
InitVars:
			; init PS_Cntr to zero
			movlw 0x00
			movwf PS_Cntr
			; init Rdy2Raise to zero
			movlw 0x00
			movwf Rdy2Raise
		return

;______________________________________________________________________ Other Subroutines
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; RedTeam
RedTeam:
			; turn off blue LED
			banksel LATA
			bcf LATA, LED2
			; turn on red LED
			;banksel LATA
			bsf LATA, LED1
			; raise the pairing flag
			btfss Rdy2Raise, 0x01
			call RaiseFlag
		return
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BlueTeam
BlueTeam:			
			; turn off red LED
			banksel LATA
			bcf LATA, LED1
			; turn on blue LED
			;banksel LATA
			bsf LATA, LED2
			; raise the pairing flag
			btfss Rdy2Raise, 0x01
			call RaiseFlag
		return
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; Unpaired
Unpaired:
			; turn off both LEDS
			banksel LATA
			bcf LATA, LED1
			bcf LATA, LED2
			; lower flag
			call LowerFlag
		return
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; Celebration
Celebration:
			; unimplemented 
		return
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; RaiseFlag
RaiseFlag:
			; raise software flag
			bsf Rdy2Raise, 0x01

			; set servo line high to begin PWM
			banksel LATA
			bsf LATA, SERVO

			; set TMR2 to interrupt after 2 ms
			banksel PR2
			movlw 0xff
			movwf PR2
			banksel T2CON
			movlw b'00111100' 
			movwf T2CON
			; clear TMR2 int flags
			banksel PIR1
			bcf PIR1,TMR2IF
			; enable TMR2 interrupts
			banksel PIE1
			bsf PIE1, TMR2IE

			; set HLTMR to count to 0.0655 s (then postscale to get 4.5 sec)
			banksel HLTPR
			movlw 0x7f 
			movwf HLTPR
			; set HLTMR to ignore hardware edge resets
			banksel HLTCON1
			bcf HLTCON1, HLTREREN
			bcf HLTCON1, HLTFEREN
			; clear interrupt flag for HLTMR
			banksel PIR1
			bcf PIR1, HLTIF
			; enable interrupt for HLTMR
			banksel PIE1
			bsf PIE1, HLTIE
			; clear HLTMR
			banksel HLTTMR
			clrf HLTTMR
			; turn on HLTMR
			banksel HLTCON0
			movlw b'01111111'
			movwf HLTCON0

			; set LastTime variable to 20 (represents 2 ms)
			movlw d'20'
			movwf LastTime
			; set HighTime variable to 200
			movlw d'200'
			movwf HighTime
			; set PS_Cntr to 68
			movlw d'68'
			movwf PS_Cntr

		return

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; LowerFlag
LowerFlag:			
			; disable HLTMR
			banksel HLTCON0
			bcf HLTCON0, HLTON
			; set HighTime to 10
			movlw d'70'
			movwf HighTime
			; set LastTime to 1
			movlw d'7'
			movwf LastTime
			; lower software flag
			bcf Rdy2Raise, 0x01
		return

;______________________________________________________________________ ISRs
GenISR:
				
PUSH: 
	 		movwf WREG_TEMP ;save WREG
			movf STATUS,W ;store STATUS in WREG
			clrf STATUS ;change to file register bank0
			movwf STATUS_TEMP ;save STATUS value
			movf PCLATH,W ;store PCLATH in WREG
			movwf PCLATH_TEMP ;save PCLATH value
			clrf PCLATH ;change to program memory page0

ISR_BODY:
			; branch to sub-ISRs
			; if this interrupt came from TMR2, go to TMR2 ISR
			banksel PIR1
			btfsc PIR1, TMR2IF
			goto TMR2_ISR
			; else if this interrupt came from HLTMR, go to HLT ISR
			banksel PIR1
			btfsc PIR1, HLTIF
			goto HLT_ISR
			; else, go to POP
			goto POP

TMR2_ISR:
			; first, clear TMR2 flag
			banksel PIR1
			bcf PIR1, TMR2IF
			; toggle servo line
			banksel PORTA
			btfss PORTA, SERVO
			goto SetServo
ClrServo:
			banksel LATA
			bcf LATA, SERVO
			; set TMR2 to interrupt after (200-LastTime)/10 ms
			banksel PR2
			movf LastTime, W
			sublw d'200'
			movwf PR2
			banksel T2CON
			movlw b'00101111' ; postscaler = 6, prescaler = 16
			movwf T2CON
			goto POP
SetServo:
			banksel LATA
			bsf LATA, SERVO
			; set TMR2 to interrupt after (HighTime/100) ms
			banksel PR2
			movf HighTime, W
			movwf PR2
			banksel T2CON
			movlw b'01001100' ; postscaler = 10
			movwf T2CON
			goto POP
			
HLT_ISR:
			; clear HLTMR interrupt flag
			banksel PIR1
			bcf PIR1, HLTIF
			; decrement PS_Cntr, if zero:
			;		-  reset PS_Cntr to 68 
			;		-  subtract 10 from HighTime
			;		-  subtract 1 from LastTime
			decfsz PS_Cntr
			goto POP
			movlw d'68'
			movwf PS_Cntr
			movlw d'10'
			subwf HighTime
			movlw d'1'
			subwf LastTime
			goto POP

POP: 
	 		clrf STATUS ;select bank 0
			movf PCLATH_TEMP,W ;store saved PCLATH value in WREG
			movwf PCLATH ;restore PCLATH
			movf STATUS_TEMP,W ;store saved STATUS value in WREG
			movwf STATUS ;restore STATUS
			swapf WREG_TEMP,F ;prepare WREG to be restored
			swapf WREG_TEMP,W ;restore WREG keeping STATUS bits
		retfie

	end

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;               PROGRAM END               ;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;*********************************************************************;
;************************      NOTES      ****************************; 
;
; Communication bits from BIG PIC:
;
;					____INPUT1__|___INPUT2___
;		Unpaired 	|	Low		|	Low		|
;		Red Team	|	High	|	Low		|
;		Blue Team	|	Low		|	High	|
;		Unused		|	High	|	High	|
;
