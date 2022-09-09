#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "Mega2560PinDefs.h"
#include "DelaySafe.h"
#include "RGBLEDCONTROL.h"

// the SMC motor is controlled by a 6 bit position code 
// (can correspond to 64 different pre-programmed positions)
// We will only use 32 total positions (32 total 1/16th inch increments - total of 2 inches)
// - the requested position ("location_point") will be derived from the user input on the Nextion display
//
// RETURNS TRUE IF MOTOR MOVEMENT WAS SUCCESSFUL
// RETURNS FALSE IF MOTOR TIMED OUT DURING MOVEMENT
bool setBinaryOutputToMotor(int location_point) {
	if (location_point >= 32 || location_point < 0) {
		// TODO: Set the status indicator for motor movement RED because
		//       we are outside of our allowable range of positions
		return;
	}

	rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_PROCESSING);

	digitalWrite(SMC_BIT_DRIVE, HIGH); // make sure drive is FALSE (on PLC side) while we set our position bits
	digitalWrite(SMC_BIT_5, !(0B100000 & location_point));
	digitalWrite(SMC_BIT_4, !(0B010000 & location_point));
	digitalWrite(SMC_BIT_3, !(0B001000 & location_point));
	digitalWrite(SMC_BIT_2, !(0B000100 & location_point));
	digitalWrite(SMC_BIT_1, !(0B000010 & location_point));
	digitalWrite(SMC_BIT_0, !(0B000001 & location_point));

	bool busy_status_prev, busy_status_curr = true;
	// Now that the bits are set, pulse DRIVE signal high (on PLC side) ~15msec
	digitalWrite(SMC_BIT_DRIVE, LOW);
	delaySafeMilli(15);
	digitalWrite(SMC_BIT_DRIVE, HIGH);
	// when actuator is finished moving the BUSY bit should experience a rising transition (on arduino side)
	unsigned long motor_start_time = millis();
	unsigned long motor_move_timeout = 4000; // just a guess for now
	bool finished = false;
	while (!finished && (millis() - motor_start_time <= motor_move_timeout)) {
		busy_status_curr = digitalRead(SMC_BIT_BUSY);
		if ((busy_status_curr != busy_status_prev) && busy_status_curr == true) { // rising edge on arduino = falling edge on PLC
			finished = true;
			rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_NO_ERROR);
			break;
		}
		busy_status_prev = busy_status_curr;
	}

	if (!finished) {
		// error condition - we timed out on motor movement, something is wrong
		rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_ERROR);
	}

	return finished;
}

#endif

