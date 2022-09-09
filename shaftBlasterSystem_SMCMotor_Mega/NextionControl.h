#ifndef NEX_DEFS_H
#define NEX_DEFS_H

#include <EasyNextionLibrary.h>
#include "MotorControl.h"

// externs from ShaftTipBlastSystem_Mega.ino
extern EasyNex myNex;
extern shaftTypes currentShaft;
extern bool nbTrigger0; // Reverse Actuator 1 Second Button Hit
extern bool nbTrigger1; // Jog Extend button hit
extern bool nbTrigger2; // Home button hit
extern bool nbTrigger3; // Graphite Shaft Selected
extern bool nbTrigger4; // Steel Shaft Selected 
extern bool nbTrigger5; // Start Cycle Test
//extern bool nbTrigger6; // Start blasting **!! This won't be triggered through nextion !!**
extern int requested_sixteenths_to_move;
extern int test_cycles;


// externs from MotorControl.h
extern bool setBinaryOutputToMotor(int num_of_sixteenth_increments);

// this trigger is only for testing and debugging. 
// ultimately, the operator will not be allowed to 
// move the actuator in the retract direction, the
// operator will be forced to pressed "HOME" to move
// the actuator back to retract home position, after
// which they can then extend the actuator X number
// of 1/16th inches
//void trigger0() {
//    nbTrigger0 = true;
//    //Serial.println(F("[INFO] JOG RET"));
//    myNex.writeStr("t2.txt", "JOGGING RET");
//    moveActuatorRetract_1Second();
//    myNex.writeStr("t2.txt", "FINISHED JOG RET");
//}

void trigger1() {
    requested_sixteenths_to_move = myNex.readNumber("n0.val");
    nbTrigger1 = true;
}

//void trigger2() {
//    // HOME
//    nbTrigger2 = true;
//    /*myNex.writeStr("t2.txt", "HOMING MOTOR STARTED");
//    homeMotor();
//    myNex.writeStr("tx.txt", "HOMING MOTOR COMPLETE");*/
//}

void trigger3() {
    // GRAPHITE SELECTED
    nbTrigger3 = true;
    myNex.writeStr("t2.txt", "GRAPHITE SELECTED");
    currentShaft = graphite;
}

void trigger4() {
    // STEEL SELECTED
    nbTrigger4 = true;
    myNex.writeStr("t2.txt", "STEEL SELECTED");
    currentShaft = steel;
}

// might need to create a state for this test
// ONLY S0->Cycling State and Cycling State->S0 allowed
void trigger5() {
    // start cycle test
    nbtrigger5 = true;
    test_cycles = mynex.readnumber("n1.val");
    requested_sixteenths_to_move = mynex.readnumber("n0.val");
}

#endif

