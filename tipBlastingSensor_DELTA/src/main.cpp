/*
EEPROM CONTENTS DEFINITION:
Address 0 = total shafts blasted over all time (unsigned long 32 bit (4 byte) variable)
*/

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EasyNextionLibrary.h>

#include <stdlib.h> // for string operations

#include <avr/wdt.h>

#include <EEPROM.h>

// convenience defines
#define RELAY_ON            digitalWrite(8, HIGH)
#define RELAY_OFF           digitalWrite(8, LOW)
#define SHAFT_SENSOR_PIN    digitalRead(2)
#define DOOR_SENSOR_PIN     digitalRead(53) // 5V=DOOR OPEN / GND=DOOR CLOSED

/*
TODO FOR DELTA
==============================
(MY SIGNALS TO DELTA):
1. Machine is Safe Signal (Door closed)
 --> GND (SAFE) // 24V (UNSAFE)
2. Currently Blasting Signal
 --> GND (BLASTING) // 24V (NOT BLASTING)
==============================
(DELTA'S SIGNAL BACK TO ME):
1. SHAFT IN PLACE
 --> GND (NOT IN PLACE) // 24V (IN PLACE) <----- !! TODO: Follow up with them on this


ARDUINO ACTIONS:
 1. OUTPUT for "Machine is Safe"
  ---> 5V = SAFE
  ---> GND = NOT SAFE
 2. OUTPUT for "CURRENTLY BLASTING"
  ---> 5V = BLASTING
  ---> GND = NOT BLASTING

 3. INPUT for "SHAFT IN PLACE"
  ---> 5V = NOT IN PLACE
  ---> GND = IN PLACE
*/

// TODO: Determine pins
#define MACHINE_IS_SAFE digitalWrite(999999, HIGH)
#define MACHINE_NOT_SAFE digitalWrite(999999, LOW)

#define CURRENTLY_BLASTING digitalWrite(999999999, HIGH)
#define NOT_BLASTING digitalWrite(999999999, LOW)

#define DELTA_SHAFT_IN_PLACE digitalRead(99999999)




#define EEPROM_CONTENTS_START_ADDRESS 0 // 4 bytes wide

enum CLUB_TYPE { GRAPHITE, IRON, GENERIC };

/*
Machine status definitions:
PICTURE ID - DESCRIPTION
2 - "CLOSED"
3 - "NO"
4 - "NO SHAFT"
5 - "OPEN"
6 - "YES"
*/

#define NEX_CLOSED   2
#define NEX_NO       3
#define NEX_NO_SHAFT 4
#define NEX_OPEN     5
#define NEX_YES      6

// object instantiations
SoftwareSerial swSerial(11, 12); // nextion display will be connected to 11(RX-BLUE) and 12(TX-YELLOW)
EasyNex myNex(swSerial);

unsigned long debounce_timeout    = 250;  // milliseconds
unsigned long last_debounce_time  = 0;    // milliseconds
unsigned long led_on_time         = 7000; // milliseconds
unsigned long last_led_start_time = 0;
unsigned long total_shaft_count   = 0;

// FOR EEPROM OPERATIONS:
unsigned long EEPROM_last_pwr_cycle_shaft_count = 0;
unsigned long EEPROM_save_period = 3600000; // 1 hour
unsigned long EEPROM_last_save_time = 0;

struct EEPROM_CONTENTS {
  // set default values initially
  unsigned long EEPROM_total_shaft_count = 0;
  CLUB_TYPE club_type = CLUB_TYPE::GENERIC; // club type no longer tracked, making this generic for now
  unsigned long saved_on_time = 4000;
};

EEPROM_CONTENTS ec;

boolean prev_shaft_sensor_value = true; // PULL-UP, AKA no shaft present = HIGH
boolean prev_door_sensor_value = true; // DOOR-OPEN, starting with door open as default for safety
boolean turn_on_blaster_relay = false;

// variables for tracking nextion button presses
bool nexbtn_sub_1_second = false;
bool nexbtn_add_1_second = false;
bool nexbtn_reset_eeprom = false;
//bool nexbtn_switch_club_type = false;

void delaySafeMillis(unsigned long timeToWaitMilli) {
  unsigned long start_time = millis();
  while (millis() - start_time <= timeToWaitMilli) { /* just hang out */ }
}

// when WDT does not get reset this interrupt happens then the arduino restarts
ISR(WDT_vect) {
  wdt_disable();
  // quickly save our data to the EEPROM before resetting:
  ec.EEPROM_total_shaft_count += (total_shaft_count - EEPROM_last_pwr_cycle_shaft_count);
  ec.saved_on_time = led_on_time;
  EEPROM.put(EEPROM_CONTENTS_START_ADDRESS, ec);
  WDTCSR = bit (WDE); // this should reset the Arduino
}

void setWDT(byte sWDT) {
  WDTCSR |= 0b00011000; // get register ready for writing
                        // (we have 4 CPU cycles to change the register)
  WDTCSR = sWDT | WDTO_2S; // WDT reset arduino after 4 seconds of inactivity
  wdt_reset(); // confirm the settings
}

void updateNextionScreen() {
  myNex.writeStr("t0.txt", String(total_shaft_count));
  unsigned long seconds = led_on_time / 1000;
  myNex.writeStr("t1.txt", String(seconds));
  myNex.writeStr("t2.txt", String(ec.EEPROM_total_shaft_count));
}

void updateNextionScreenMachineStatus() {

}

void updateEEPROMContents() {
  ec.EEPROM_total_shaft_count += (total_shaft_count - EEPROM_last_pwr_cycle_shaft_count);
  ec.saved_on_time = led_on_time;
  // save this total new value back into EEPROM
  EEPROM.put(EEPROM_CONTENTS_START_ADDRESS, ec);
  // reset our tracking variable so we can get an accurate shaft delta next EEPROM update
  EEPROM_last_pwr_cycle_shaft_count = total_shaft_count;

  // Serial.println(" -- SAVED PARAMS -- ");
  // Serial.print("ec.club_type:");
  // Serial.println(ec.club_type);
  // Serial.print("ec.EEPROM_total_shaft_count:");
  // Serial.println(ec.EEPROM_total_shaft_count);
  // Serial.print("ec.saved_on_time:");
  // Serial.println(ec.saved_on_time);

  updateNextionScreen();
}

void loadEEPROMContents() {
  EEPROM.get(EEPROM_CONTENTS_START_ADDRESS, ec);
  
  // check for uninitialized EEPROM (will be all 1's)
  if(ec.saved_on_time > 30000) {
    ec.saved_on_time = 7000;
    ec.club_type = CLUB_TYPE::GRAPHITE;
    ec.EEPROM_total_shaft_count = 0;
    updateEEPROMContents(); // write the defaults to EEPROM
  }

  led_on_time = ec.saved_on_time;

  // if(ec.club_type == CLUB_TYPE::IRON) {
  //   myNex.writeNum("bt0.val", 1);
  // } 
  // else if(ec.club_type == CLUB_TYPE::GRAPHITE) {
  //   myNex.writeNum("bt0.val", 0);
  // }
  // else {
  //   // CLUB_TYPE::GENERIC
  //   // TODO
  // }

  // Serial.println(" -- LOADED PARAMS -- ");
  // Serial.print("ec.club_type:");
  // Serial.println(ec.club_type);
  // Serial.print("ec.EEPROM_total_shaft_count:");
  // Serial.println(ec.EEPROM_total_shaft_count);
  // Serial.print("ec.saved_on_time:");
  // Serial.println(ec.saved_on_time);

}

void clearEEPROMContents() {
  // set first 4 bytes to 0 (ADDR0,1,2,3)
  ec.EEPROM_total_shaft_count = 0x0;
  ec.saved_on_time = led_on_time;
  EEPROM.put(EEPROM_CONTENTS_START_ADDRESS, ec); // reset total shaft count EEPROM to 0
  updateNextionScreen();
}

void setup() {
  //Serial.begin(9600); // for testing EEPROM
  myNex.begin(38400);

  wdt_disable(); // data sheet recommends disabling wdt immediately while uC starts up

  pinMode(2, INPUT); // shaft sensor pin
  pinMode(53, INPUT_PULLUP); // 5V = DOOR OPEN. Door 
                             // safety pin. INPUT_PULLUP so that if door sensor is missing,
                             // the system will safely default to a "DOOR OPEN" state
  pinMode(8, OUTPUT); // relay on/off pin

  delaySafeMillis(5);

  NOT_BLASTING; // to delta
  MACHINE_NOT_SAFE; // to delta
  RELAY_OFF;

  setWDT(0b01000000); // 00001000 = just reset if WDT not handled within timeframe
                      // 01001000 = set to trigger interrupt then reset
                      // 01000000 = just interrupt

  loadEEPROMContents();

  updateNextionScreen();
}

void Start_Blasting() {
  CURRENTLY_BLASTING; // signal to delta
  turn_on_blaster_relay = true;
  myNex.writeNum("p4.pic", NEX_YES); // "BLASTING = YES"
  RELAY_ON;
}

void Stop_Blasting() {
  turn_on_blaster_relay = false;
  RELAY_OFF;
  NOT_BLASTING; // signal to delta
  myNex.writeNum("p4.pic", NEX_NO); // "BLASTING = NO"
  total_shaft_count += 1;
  updateNextionScreen(); // update the shaft count
}

void handleMillisRolloverCondition() {
  // Shaft debounce rollover:
  if(millis() < last_debounce_time) last_debounce_time = 0;
  // EEPROM rollover:
  if((unsigned long)millis() < EEPROM_last_save_time) EEPROM_last_save_time = 0;
}

int eeprom_update_counter = 0; // DEBUG
void loop() {
  wdt_reset(); // if we don't reset the WDT within 2 seconds the arduino will restart
               // NOTE: If we DO restart due to WDT, the EEPROM settings will be updated before the restart
  bool current_shaft_sensor_value  = SHAFT_SENSOR_PIN;    // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  bool current_door_sensor_value   = DOOR_SENSOR_PIN;     // LOW = DOOR CLOSED // HIGH = DOOR OPEN
  bool current_delta_sip_value     = DELTA_SHAFT_IN_PLACE; // (Shaft In Place) LOW = IN PLACE // HIGH = NOT IN PLACE

  // update the nextion display machine status indicators for DOOR PRESENCE
  if(current_door_sensor_value != prev_door_sensor_value) {
    if(!current_door_sensor_value) {
      MACHINE_IS_SAFE; // to delta
      myNex.writeNum("p2.pic", NEX_CLOSED);   // DETECT DOOR OPEN->CLOSE TRANSITION
    } 
    else if (current_door_sensor_value) {
      MACHINE_NOT_SAFE; // to delta
      myNex.writeNum("p2.pic", NEX_OPEN);     // DETECT DOOR CLOSE->OPEN TRANSITION
    }
  }

  // update the nextion display machine status indicators for SHAFT PRESENCE
  if(current_shaft_sensor_value != prev_shaft_sensor_value) {
    if(!current_shaft_sensor_value) {
      myNex.writeNum("p3.pic", NEX_YES);      // DETECT SHAFT PRESENT
    }
    else if (current_shaft_sensor_value) {
      myNex.writeNum("p3.pic", NEX_NO_SHAFT); // DETECT SHAFT ABSENT
    }
  }

  handleMillisRolloverCondition(); // for both shaft timer and eeprom timer

  if(!turn_on_blaster_relay && (millis() - last_debounce_time > debounce_timeout)) {
    if(!current_door_sensor_value) {
      //if (prev_shaft_sensor_value == true && current_shaft_sensor_value == false) { // check if this is a HIGH->LOW transition
      if(!current_delta_sip_value && !current_shaft_sensor_value) {
        //myNex.writeNum("p3.pic", NEX_YES); // "SHAFT SENSE = YES" !!! TODO: SHOW NEXTION SCREEN "SHAFT IN PLACE" IMAGE
        Start_Blasting();
        last_led_start_time = millis();
        last_debounce_time = last_led_start_time; 
      }
    }
  }

  if (turn_on_blaster_relay) {
    if(current_door_sensor_value) {
      Stop_Blasting();
    }
    else if(current_delta_sip_value) {
      Stop_Blasting();
    }
    else if(millis() - last_led_start_time > led_on_time) { // shaft has been present for entire blast. turn off blasters
      Stop_Blasting();
    }
    else if (current_shaft_sensor_value) { // HIGH = NO SHAFT PRESENT
      myNex.writeNum("p3.pic", NEX_NO_SHAFT); // "SHAFT SENSE = NO SHAFT"
      Stop_Blasting();
    }
  }

  myNex.NextionListen();

  if(nexbtn_sub_1_second) {
    led_on_time -= 1000;
    if(led_on_time <= 0) led_on_time = 1000; // clamp to a min time
    updateEEPROMContents();
  }

  if(nexbtn_add_1_second) {
    led_on_time += 1000;
    if(led_on_time > 15000) led_on_time = 15000; // clamp to a max time
    updateEEPROMContents();
  }

  if(nexbtn_reset_eeprom) {
    clearEEPROMContents();
  }

  // update EEPROM every EEPROM_save_period milliseconds
  if((unsigned long)millis() - EEPROM_last_save_time >= EEPROM_save_period) {
    updateEEPROMContents();
    EEPROM_last_save_time = millis();
  }

  nexbtn_sub_1_second = false;
  nexbtn_add_1_second = false;
  nexbtn_reset_eeprom = false;
  
  prev_shaft_sensor_value = current_shaft_sensor_value;
  prev_door_sensor_value  = current_door_sensor_value;
}