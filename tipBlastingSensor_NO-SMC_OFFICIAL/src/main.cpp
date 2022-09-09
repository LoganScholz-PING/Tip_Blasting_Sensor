/*
TODO LIST FOR NEXTION CHANGEOVER
1. *DONE*
2. *DONE*
3. *DONE*
4. *DONE*
*/

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EasyNextionLibrary.h>

#include <stdlib.h> // for string operations

#include <avr/wdt.h>

// convenience defines
#define RELAY_ON            digitalWrite(8, HIGH)
#define RELAY_OFF           digitalWrite(8, LOW)
#define SHAFT_SENSOR_PIN    digitalRead(2)
#define DOOR_SENSOR_PIN     digitalRead(53) // 5V=DOOR OPEN / GND=DOOR CLOSED


  // -------------------------
 // SET THE CLUB TYPE HERE!! |
// ---------------------------
enum CLUB_TYPE { GRAPHITE, IRON };
CLUB_TYPE club_type = CLUB_TYPE::GRAPHITE;


// object instantiations
SoftwareSerial swSerial(11, 12); // nextion display will be connected to 11(RX) and 12(TX)
EasyNex myNex(swSerial);

unsigned long debounce_timeout    = 250;  // milliseconds
unsigned long last_debounce_time  = 0;    // milliseconds
unsigned long led_on_time         = 7000; // milliseconds
unsigned long last_led_start_time = 0;
unsigned long total_shaft_count   = 0;

boolean prev_sensor_value     = true; // PULL-UP, AKA no shaft present = HIGH
boolean turn_on_blaster_relay = false;

// variables for tracking nextion button presses
bool nexbtn_sub_1_second = false;
bool nexbtn_add_1_second = false;
//bool nexbtn_fake_shaft   = false; // TODO: DELETE

void delaySafeMillis(unsigned long timeToWaitMilli) {
  unsigned long start_time = millis();
  while (millis() - start_time <= timeToWaitMilli) { /* just hang out */ }
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
}

void setup() {
  myNex.begin(38400);

  wdt_disable(); // data sheet recommends disabling wdt immediately while uC starts up

  pinMode(2, INPUT); // shaft sensor pin
  pinMode(53, INPUT_PULLUP); // door safety pin
  pinMode(8, OUTPUT); // relay on/off pin

  delaySafeMillis(5);

  RELAY_OFF;

  setWDT(0b00001000); // 00001000 = just reset if WDT not handled within timeframe
                      // 01001000 = set to trigger interrupt then reset
                      // 01000000 = just interrupt 
  
  switch(club_type) { // sets initial blast time when machine is power cycled
    case(CLUB_TYPE::GRAPHITE):
      led_on_time = 7000;
      break;
    case(CLUB_TYPE::IRON):
      led_on_time = 4000;
      break;
    default:
      club_type = CLUB_TYPE::IRON;
      break;
  }

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

void loop() {
  wdt_reset(); // if we don't reset the WDT within 2 seconds the arduino will restart
  bool current_shaft_sensor_value  = SHAFT_SENSOR_PIN;    // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  bool current_door_sensor_value   = DOOR_SENSOR_PIN;     // LOW = DOOR CLOSED // HIGH = DOOR OPEN

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
    updateNextionScreen();
  }

  if(nexbtn_add_1_second) {
    led_on_time += 1000;
    if(led_on_time > 15000) led_on_time = 15000; // clamp to a max time
    updateNextionScreen();
  }

  nexbtn_sub_1_second = false;
  nexbtn_add_1_second = false;
  
  prev_sensor_value = current_shaft_sensor_value;
}