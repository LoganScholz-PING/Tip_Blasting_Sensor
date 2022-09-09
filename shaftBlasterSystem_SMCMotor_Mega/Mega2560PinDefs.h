#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include "DelaySafe.h"

/*
Note on relay shield - Shield covers following pins:
A0-A5
Digital 0-13+GND+AREF+SDA1+SCL1
!! The following pins are truly off-limits: D7, D6, D5, D4 !!
*/

// **********************
// Relay pin definitions
// ===OUTPUTS===
#define RELAY_1_CTRL 7 // sand blast (air solenoid valve) control pin
#define RELAY_2_CTRL 6 // unused
#define RELAY_3_CTRL 5 // unused
#define RELAY_4_CTRL 4 // unused

// **********************
// 4n27 Shaft-Presence Sense Pin
// 5V = no shaft present
// ~GND = shaft present
// ===INPUTS===
#define SHAFT_SENSE_PIN 30

// **********************
// RGB LED Status Indicator Driver Pin
// ===OUTPUTS===
#define LED_STATUS_INDICATOR_PIN 38 // D38 has a timer - not sure if this is needed for WS2812 comm

// **********************
// SMC Motor Control
// ===OUTPUTS===
// !!!!!! NOTE THAT THIS SMC LOGIC IS NEGATED !!!!!!
// ----> We have to use opto-isolation to translate 5V logic to 24V PLC Logic <----
// ARDUINO 0V (FALSE) = PLC 24V (TRUE)
// ARDUINO 5V (TRUE)  = PLC 0V  (FALSE)
#define SMC_BIT_5 53
#define SMC_BIT_4 51
#define SMC_BIT_3 49
#define SMC_BIT_2 47
#define SMC_BIT_1 45
#define SMC_BIT_0 43
#define SMC_BIT_DRIVE 41
// ===INPUTS===
#define SMC_BIT_BUSY  39

void initializePins() {
	//OUTPUTS:
	pinMode(RELAY_1_CTRL, OUTPUT);
	pinMode(LED_STATUS_INDICATOR_PIN, OUTPUT);
	pinMode(SMC_BIT_5, OUTPUT);
	pinMode(SMC_BIT_4, OUTPUT);
	pinMode(SMC_BIT_3, OUTPUT);
	pinMode(SMC_BIT_2, OUTPUT);
	pinMode(SMC_BIT_1, OUTPUT);
	pinMode(SMC_BIT_0, OUTPUT);
	pinMode(SMC_BIT_DRIVE, OUTPUT);
	delaySafeMicro(100);
	//INPUTS:
	pinMode(SHAFT_SENSE_PIN, INPUT);
	pinMode(SMC_BIT_BUSY, INPUT);
	delaySafeMicro(100);
	// SET INITIAL OUTPUT STATES
	digitalWrite(RELAY_1_CTRL, LOW);
	digitalWrite(LED_STATUS_INDICATOR_PIN, LOW);
	digitalWrite(SMC_BIT_5, HIGH);
	digitalWrite(SMC_BIT_4, HIGH);
	digitalWrite(SMC_BIT_3, HIGH);
	digitalWrite(SMC_BIT_2, HIGH);
	digitalWrite(SMC_BIT_1, HIGH);
	digitalWrite(SMC_BIT_0, HIGH);
	digitalWrite(SMC_BIT_DRIVE, HIGH);
	delaySafeMicro(100);
}

#endif