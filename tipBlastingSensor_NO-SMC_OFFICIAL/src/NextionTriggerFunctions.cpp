#include <Arduino.h>

extern bool nexbtn_sub_1_second;
extern bool nexbtn_add_1_second;
extern bool nexbtn_reset_eeprom;
extern bool nexbtn_switch_club_type;

// subtract 1 second button
void trigger0() {
    nexbtn_sub_1_second = true;
}

// add 1 second button
void trigger1() {
    nexbtn_add_1_second = true;
}

void trigger2() {
    nexbtn_reset_eeprom = true;
}

void trigger3() {
    nexbtn_switch_club_type = true;
}