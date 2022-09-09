/*
=== State Definitions ====
0 - IDLE
1 - HOMING
2 - EXTENDING
3 - BLASTING
4 - ERROR/UNKNOWN

Valid Transition Definitions:
State 0 (IDLE):
0->1 (HOMING)
0->2 (EXTENDING)
0->3 (BLASTING)
0->4 (ERROR)

State 1 (HOMING):
1->0 (IDLE)
1->2 (EXTENDING)
1->4 (ERROR)

State 2 (EXTENDING):
2->0 (IDLE)
2->4 (ERROR) - exceeded time/click counter not increasing

State 3 (BLASTING):
3->0 (IDLE)
3->4 (ERROR) - cannot turn on/off pneumatics (how to check for this?)

State 4 (ERROR):
4->0 (IDLE) - error manually acknowledged (how?)
*/

#ifndef PROGRAM_STATES_H
#define PROGRAM_STATES_H

#include "RGBLEDCONTROL.h"
#include "Mega2560PinDefs.h"

// from ShaftTipBlastSystem_Mega.ino
extern bool nbTrigger0; // Reverse Actuator 1 Second Button Hit
extern bool nbTrigger1; // Jog Extend button hit
extern bool nbTrigger2; // Home button hit
extern bool nbTrigger3; // Graphite Shaft Selected
extern bool nbTrigger4; // Steel Shaft Selected 
extern bool nbTrigger5; // Start Cycle Test
extern bool nbTrigger6; // Start Blasting
extern bool _homed_successfully;
extern int requested_sixteenths_to_move;
extern EasyNex myNex;

StateMachine blasterStateMachine = StateMachine();
enum MACHINESTATE { IDLE = 0, HOMING = 1, EXTENDING = 2, BLASTING = 3, ERROR = 4, TESTING = 5 };
MACHINESTATE _machine_state = IDLE;

unsigned long blast_time = 0;
unsigned long blast_on_timer = 2000; // TODO: The "ON" blast times need to be defined
bool blasting_complete = false;
bool extending_complete = false;
bool testing_complete = false;

// **************************************
// ****** initialization section: *******
// **************************************
// prototypes
void state0(void);
void state1(void);
void state2(void);
void state3(void);
void state4(void);
void state5(void); // just for testing
State* S0 = blasterStateMachine.addState(&state0);
State* S1 = blasterStateMachine.addState(&state1);
State* S2 = blasterStateMachine.addState(&state2);
State* S3 = blasterStateMachine.addState(&state3);
State* S4 = blasterStateMachine.addState(&state4);
State* S5 = blasterStateMachine.addState(&state5); // just for testing

// ==================================================================================
// *****************************
// **** STATE0 (IDLE STATE) ****
// *****************************
void state0() {
	// IDLE
	if (blasterStateMachine.executeOnce) {
		_machine_state = IDLE;
		// don't allow errant button presses from previous states
		// to corrupt a fresh state 0 transition
		nbTrigger0 = false; // Reverse Actuator 1 Second Button Hit
		nbTrigger1 = false; // Jog Extend button hit
		nbTrigger2 = false; // Home button hit
		nbTrigger3 = false; // Graphite Shaft Selected
		nbTrigger4 = false; // Steel Shaft Selected 
		nbTrigger5 = false; // Start Cycle Test
		nbTrigger6 = false; // Start Blasting
		blasting_complete = false; // controls S3->S0 transition
		extending_complete = false;
		rgbSetLEDColor(LED_STATE::LED_IDLE, RGB_CLR_IND::RGB_NO_ERROR);
	}
}

// TODO: FIGURE OUT HOW TO FORCE HOME THE SMC MOTOR
bool transitionS0S1() { // IDLE->HOMING
	if (nbTrigger2) { // operator pressed home actuator button while in idle state
		nbTrigger2 = false;
		_homed_successfully = false;
		return true;
	}
	return false;
}

bool transitionS0S2() { // IDLE->EXTENDING
	if (nbTrigger1) { // operator pressed extend button while in idle state
		nbTrigger1 = false;
		return true;
	}
	return false;
}

bool transitionS0S3() { // IDLE->BLASTING
	if (nbTrigger6) { // shaft tip sensor (from main loop) senses a shaft is present
		nbTrigger6 = false;
		blasting_complete = false;
		return true;
	}
	return false;
}

bool transitionS0S4() { // IDLE->ERROR
	if (false) {
		// TODO:
		// WHAT ARE THE ERROR CRITEREON?
		return true;
	}
	return false;
}

bool transitionS0S5() { // IDLE->TESTING **TEMPORARY**
	if (nbTrigger5) {
		nbTrigger5 = false;
		testing_complete = false;
		return true;
	}
	return false;
}

// ==================================================================================
// *******************************
// **** STATE1 (HOMING STATE) ****
// *******************************
void state1() {
	// HOMING
	if (blasterStateMachine.executeOnce) {
		_machine_state = HOMING;
		rgbSetLEDColor(LED_STATE::LED_HOMING, RGB_CLR_IND::RGB_PROCESSING);
		// TODO: Figure out how to force home SMC motor
		if (!_homed_successfully) {
			rgbSetLEDColor(LED_STATE::LED_HOMING, RGB_CLR_IND::RGB_ERROR);
			blasterStateMachine.transitionTo(S4); // force transition to error state
			return; // TODO: i think we need this?
		}
		rgbSetLEDColor(LED_STATE::LED_HOMING, RGB_CLR_IND::RGB_NO_ERROR);
	}
}

bool transitionS1S0() { // HOMING->IDLE
	if (_homed_successfully) {
		return true;
	}
	return false;
}
bool transitionS1S2() {} // HOMING->EXTENDING **!! THIS TRANSITION IS OBSOLETE. EXTENDING INCLUDES HOMING !!**
bool transitionS1S4() {} // HOMING->ERROR **!! THIS TRANSITION IS OBSOLETE. ERROR TRANSITION IS FORCED IN state1()

// ==================================================================================
// **********************************
// **** STATE2 (EXTENDING STATE) ****
// **********************************
void state2() {
	// EXTENDING
	if (blasterStateMachine.executeOnce) {
		_machine_state = EXTENDING;
		rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_PROCESSING);
		// TODO: check if _homed_successfully is TRUE, if so skip homing
		if (!_homed_successfully) {
			rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_ERROR);
			blasterStateMachine.transitionTo(S4); // force transition to error state
			return; // TODO: i think we need this?
		}

		if (!setBinaryOutputToMotor(requested_sixteenths_to_move)) {
			rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_ERROR);
			blasterStateMachine.transitionTo(S4); // force transition to error state
			return; // TODO: i think we need this?
		}
		rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_NO_ERROR);
		extending_complete = true;
	}
}

bool transitionS2S0() { // EXTENDING->IDLE
	if (extending_complete && _homed_successfully) {
		return true;
	}
	return false;
}

bool transitionS2S4() {
	if (extending_complete && !_homed_successfully) {
		rgbSetLEDColor(LED_STATE::LED_EXTENDING, RGB_CLR_IND::RGB_ERROR);
		return true;
	}
	return false;
}

// ==================================================================================
// *********************************
// **** STATE3 (BLASTING STATE) ****
// *********************************
void state3() {
	// BLASTING
	if (blasterStateMachine.executeOnce) {
		rgbSetLEDColor(LED_STATE::LED_BLASTING, RGB_CLR_IND::RGB_PROCESSING);
		_machine_state = BLASTING;
		// POSSIBLE TODO: Check if Air Pressure is good before blasting
		blast_time = millis();
		digitalWrite(RELAY_1_CTRL, HIGH);
		while (millis() - blast_time >= blast_on_timer) {
			// just wait
		}
		blasting_complete = true;
	}
}

bool transitionS3S0() { // BLASTING->IDLE
	if (blasting_complete) {
		return true;
	}
	return false;
}

bool transitionS3S4() {} // BLASTING->ERROR

// ==================================================================================
// **************************************
// **** STATE4 (ERROR/UNKNOWN STATE) ****
// **************************************
void state4() {
	// IDEAS:
	//  - SET UP A "BLINKING" LOOP FOR ANY ERROR CONDITION SO 
	//    THE RGB LED RELATING TO THE STATE THAT FAILED BLINKS

	// TODO: HOW TO RECOVER? CYCLE POWER TO ARDUINO? FORCE RE-HOME MOTOR THEN MOVE BACK TO IDLE?

	// ERROR/UNKNOWN
	if (blasterStateMachine.executeOnce) {
		_machine_state = ERROR;
	}
}

// TODO: Might need transition from ERROR->HOME MOTOR here (then we can use HOME MOTOR to get back to IDLE)
bool transitionS4S0() {} // ERROR->IDLE


// ==================================================================================
// **************************************
// **** STATE5 (TESTING STATE) ****
// **************************************
void state5() {
	if (blasterStateMachine.executeOnce) {
		rgbSetLEDColor(LED_STATE::LED_ERROR, RGB_CLR_IND::RGB_PROCESSING); // gonna piggy-back on the error LED for now
		_machine_state = TESTING;
		// TODO: RETHINK CYCLE TEST WITH SMC ACTUATOR
		// ......maybe have it go 0->1->0->2->0->3->0->4.....->0->31, record the data points, and start over X times (????????)
		//runCycleTest(test_cycles, requested_sixteenths_to_move);
		testing_complete = true;
		rgbSetLEDColor(LED_STATE::LED_ERROR, RGB_CLR_IND::RGB_NO_ERROR); // gonna piggy-back on the error LED for now
	}
}

bool transitionS5S0() {
	if (testing_complete) {
		return true;
	}
	return false;
}


void initializeStateMachineTransitions() {
	// state 0 transitions
	S0->addTransition(&transitionS0S1, S1);
	S0->addTransition(&transitionS0S2, S2);
	S0->addTransition(&transitionS0S3, S3);
	S0->addTransition(&transitionS0S4, S4);
	S0->addTransition(&transitionS0S5, S5);
	// state 1 transitions
	S1->addTransition(&transitionS1S0, S0);
	S1->addTransition(&transitionS1S2, S2);
	S1->addTransition(&transitionS1S4, S4);
	// state 2 transitions
	S2->addTransition(&transitionS2S0, S0);
	S2->addTransition(&transitionS2S4, S4);
	// state 3 transitions
	S3->addTransition(&transitionS3S0, S0);
	S3->addTransition(&transitionS3S4, S4);
	// state 4 transitions
	S4->addTransition(&transitionS4S0, S0);
	// state 5 transition (JUST FOR TESTING)
	S5->addTransition(&transitionS5S0, S0); // just for testing
}

#endif
