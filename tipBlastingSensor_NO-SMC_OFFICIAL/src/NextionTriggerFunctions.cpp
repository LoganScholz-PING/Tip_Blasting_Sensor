#include <Arduino.h>

extern bool nexbtn_sub_1_second;
extern bool nexbtn_add_1_second;

// subtract 1 second button
void trigger0() {
    nexbtn_sub_1_second = true;
}

// add 1 second button
void trigger1() {
    nexbtn_add_1_second = true;
}