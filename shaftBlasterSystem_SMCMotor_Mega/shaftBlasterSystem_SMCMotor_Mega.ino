enum shaftTypes { graphite, steel };
shaftTypes currentShaft = graphite;

#include <Arduino.h>
#include <EasyNextionLibrary.h>
#include <LinkedList.h>
#include <StateMachine.h>
#include <FastLED.h>
// == custom includes ==
#include "Mega2560PinDefs.h"
#include "DelaySafe.h"
#include "RGBLEDCONTROL.h"
#include "MotorControl.h"
#include "ProgramStates.h"

// === NEXTION DEFINITIONS ===
EasyNex myNex(Serial2); // NEXTION TO SERIAL 2

// === GENERAL GLOBAL VARIABLE DEFINITIONS ===
bool nbTrigger0 = false; // Reverse Actuator 1 Second Button Hit
bool nbTrigger1 = false; // Jog Extend button hit
bool nbTrigger2 = false; // Home button hit
bool nbTrigger3 = false; // Graphite Shaft Selected
bool nbTrigger4 = false; // Steel Shaft Selected 
bool nbTrigger5 = false; // Start Cycle Test
bool nbTrigger6 = false; // Start Blasting
bool shaft_sense_pin_previous = true; // 5V  = no shaft present
bool shaft_sense_pin_current = true;  // GND = shaft present
bool _homed_successfully = false;
int requested_sixteenths_to_move = 0;
int test_cycles = 0;

extern MACHINESTATE _machine_state; // from ProgramStates.h

void setup() {
	initializePins();
	myNex.begin(57600);
	initializeStateMachineTransitions();
}


void loop() {
	shaft_sense_pin_current = digitalRead(SHAFT_SENSE_PIN);
	if ((shaft_sense_pin_current != shaft_sense_pin_previous) && (shaft_sense_pin_current == false) && _machine_state == IDLE) {
		nbTrigger6 = true; // shaft detected, start sand blasting if machine is in the correct state
	}

	myNex.NextionListen();
	blasterStateMachine.run();
	shaft_sense_pin_previous = shaft_sense_pin_current;
}
