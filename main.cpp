#include <Arduino.h>

//#define USING_SERIAL_FOR_DEBUG 1

// convenience defines
#define SHAFT_SENSOR_PIN digitalRead(2)
#define DOOR_SENSOR_PIN digitalRead(53) // 5V=DOOR OPEN / GND=DOOR CLOSED
#define RELAY_ON     digitalWrite(8, HIGH)
#define RELAY_OFF    digitalWrite(8, LOW)

unsigned long debounce_timeout   = 250;  // milliseconds
unsigned long last_debounce_time = 0;   // milliseconds
unsigned long led_on_time         = 3000;
unsigned long last_led_start_time = 0;

boolean prev_sensor_value = true; // PULL-UP, AKA no shaft present = HIGH
boolean turn_on_blaster_relay      = false;

void setup() {
#ifdef USING_SERIAL_FOR_DEBUG
  Serial.begin(115200);
#endif

  pinMode(2, INPUT);
  pinMode(53, INPUT);
  pinMode(8, OUTPUT);

  RELAY_OFF;

#ifdef USING_SERIAL_FOR_DEBUG
  Serial.println(F("Entering main loop..."));
#endif
}

void Start_Blasting() {
  turn_on_blaster_relay = true;
  RELAY_ON;
}

void Stop_Blasting() {
  turn_on_blaster_relay = false;
  RELAY_OFF;
}

void loop() {
  bool current_shaft_sensor_value = SHAFT_SENSOR_PIN; // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  bool current_door_sensor_value = DOOR_SENSOR_PIN; // LOW = DOOR CLOSED // HIGH = DOOR OPEN

  if((millis() - last_debounce_time > debounce_timeout) && !current_door_sensor_value) {
    if (prev_sensor_value == true && current_shaft_sensor_value == false) { // check if this is a HIGH->LOW transition
     
      #ifdef USING_SERIAL_FOR_DEBUG
      Serial.println(F("[INFO] DEBOUNCE TRIGGERED!"));
      #endif
      Start_Blasting();
      last_led_start_time = millis();
      last_debounce_time = last_led_start_time; 
    }
  }

  if (turn_on_blaster_relay) {
    if(millis() - last_led_start_time > led_on_time) { // shaft has been present for entire blast. turn off blasters
      #ifdef USING_SERIAL_FOR_DEBUG
      Serial.println(F("[INFO] TIMEOUT REACHED. TURNING LED OFF"));
      #endif

      Stop_Blasting();
    }
    else if (current_shaft_sensor_value || current_door_sensor_value) { // HIGH = NO SHAFT PRESENT
      #ifdef USING_SERIAL_FOR_DEBUG
      Serial.println(F("[INFO] SHAFT NO LONGER PRESENT. STOPPING."));
      #endif
      Stop_Blasting();
    }
  }
  
  #ifdef USING_SERIAL_FOR_DEBUG
  Serial.println(F("*********************************************************"));
  Serial.print(F("[DATA] turn_on_blaster_relay: ")   ); Serial.println(turn_on_blaster_relay);
  Serial.print(F("[DATA] prev_sensor_value: ")       ); Serial.println(prev_sensor_value);
  Serial.print(F("[DATA] last_led_start_time: ")     ); Serial.println(last_led_start_time);
  Serial.print(F("[DATA] last_debounce_time: ")      ); Serial.println(last_debounce_time);
  #endif

  prev_sensor_value = current_shaft_sensor_value;
}