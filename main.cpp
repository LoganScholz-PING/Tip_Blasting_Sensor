#include <Arduino.h>

// convenience defines
#define SENSOR_PIN digitalRead(2)
#define RELAY_ON     digitalWrite(8, HIGH)
#define RELAY_OFF    digitalWrite(8, LOW)

unsigned long debounce_timeout   = 25;  // milliseconds
unsigned long last_debounce_time = 0;   // milliseconds
unsigned long led_on_time         = 4000;
unsigned long last_led_start_time = 0;

boolean prev_sensor_value = true; // PULL-UP, AKA no shaft present = HIGH
boolean turn_on_blaster_relay      = false;

void setup() 
{
  //Serial.begin(115200);

  pinMode(2, INPUT);
  pinMode(8, OUTPUT);

  RELAY_OFF;

  //Serial.println(F("Entering main loop...")); 
}


void Start_Blasting()
{
  turn_on_blaster_relay = true;
  RELAY_ON;
}

void Stop_Blasting()
{
  turn_on_blaster_relay = false;
  RELAY_OFF;
}


void loop() 
{
  bool current_sensor_value = SENSOR_PIN; // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT

  if(millis() - last_debounce_time > debounce_timeout)
  {
    if (prev_sensor_value == true && current_sensor_value == false) // check if this is a HIGH->LOW transition
    {
      //Serial.println(F("[INFO] DEBOUNCE TRIGGERED!"));
      Start_Blasting();
      last_led_start_time = millis();
      last_debounce_time = last_led_start_time; 
    }
  }

  if (turn_on_blaster_relay)
  {
    if(millis() - last_led_start_time > led_on_time) // shaft has been present for entire blast. turn off blasters
    {
      //Serial.println(F("[INFO] TIMEOUT REACHED. TURNING LED OFF"));
      Stop_Blasting();
    }
    else if (current_sensor_value) // HIGH = NO SHAFT PRESENT
    {
      //Serial.println(F("[INFO] SHAFT NO LONGER PRESENT. STOPPING."));
      Stop_Blasting();
    }
  }
  
  //Serial.println(F("*********************************************************"));
  //Serial.print(F("[DATA] turn_on_blaster_relay: ")   ); Serial.println(turn_on_blaster_relay);
  //Serial.print(F("[DATA] prev_sensor_value: ")       ); Serial.println(prev_sensor_value);
  //Serial.print(F("[DATA] current_sensor_pin_value: ")); Serial.println(current_sensor_pin_value);
  //Serial.print("[DATA] last_led_start_time: "     ); Serial.println(last_led_start_time);
  //Serial.print("[DATA] last_debounce_time: "      ); Serial.println(last_debounce_time);
  prev_sensor_value = current_sensor_value;
}