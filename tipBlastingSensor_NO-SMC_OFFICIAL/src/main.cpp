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

#define EEPROM_CONTENTS_START_ADDRESS 0 // 4 bytes wide

enum CLUB_TYPE { GRAPHITE, IRON };

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
  unsigned long EEPROM_total_shaft_count = 0;
  CLUB_TYPE club_type = CLUB_TYPE::IRON;
  unsigned long saved_on_time = 4000;
};

EEPROM_CONTENTS ec;

boolean prev_sensor_value     = true; // PULL-UP, AKA no shaft present = HIGH
boolean turn_on_blaster_relay = false;

// variables for tracking nextion button presses
bool nexbtn_sub_1_second = false;
bool nexbtn_add_1_second = false;
bool nexbtn_reset_eeprom = false;
bool nexbtn_switch_club_type = false;

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

  if(ec.club_type == CLUB_TYPE::IRON) {
    myNex.writeNum("bt0.val", 1);
  } 
  else if(ec.club_type == CLUB_TYPE::GRAPHITE) {
    myNex.writeNum("bt0.val", 0);
  }

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
  pinMode(53, INPUT_PULLUP); // door safety pin
  pinMode(8, OUTPUT); // relay on/off pin

  pinMode(LED_BUILTIN, OUTPUT); // DEBUG - testing WDT
  digitalWrite(LED_BUILTIN, LOW); // DEBUG - testing WDT

  delaySafeMillis(5);

  RELAY_OFF;

  setWDT(0b01000000); // 00001000 = just reset if WDT not handled within timeframe
                      // 01001000 = set to trigger interrupt then reset
                      // 01000000 = just interrupt

  loadEEPROMContents();

  updateNextionScreen();
}

void Start_Blasting() {
  turn_on_blaster_relay = true;
  RELAY_ON;
}

void Stop_Blasting() {
  turn_on_blaster_relay = false;
  RELAY_OFF;
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

  handleMillisRolloverCondition(); // for both shaft timer and eeprom timer

  if(millis() - last_debounce_time > debounce_timeout) {
    if(!current_door_sensor_value) {
      if (prev_sensor_value == true && current_shaft_sensor_value == false) { // check if this is a HIGH->LOW transition
        Start_Blasting();
        last_led_start_time = millis();
        last_debounce_time = last_led_start_time; 
      }
    }
  }

  if (turn_on_blaster_relay) {
    if(millis() - last_led_start_time > led_on_time) { // shaft has been present for entire blast. turn off blasters
      Stop_Blasting();
    }
    else if (current_shaft_sensor_value || current_door_sensor_value) { // HIGH = NO SHAFT PRESENT
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

  if(nexbtn_switch_club_type) {
    if(ec.club_type == CLUB_TYPE::IRON) ec.club_type = CLUB_TYPE::GRAPHITE;
    else ec.club_type = CLUB_TYPE::IRON;
    updateEEPROMContents();
  }

  // update EEPROM every EEPROM_save_period milliseconds
  if((unsigned long)millis() - EEPROM_last_save_time >= EEPROM_save_period) {
    updateEEPROMContents();
    EEPROM_last_save_time = millis();
  }

  nexbtn_sub_1_second = false;
  nexbtn_add_1_second = false;
  nexbtn_reset_eeprom = false;
  nexbtn_switch_club_type = false;
  
  prev_sensor_value = current_shaft_sensor_value;
}