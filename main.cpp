#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//#define USING_SERIAL_FOR_DEBUG 1

// convenience defines
#define SHAFT_SENSOR_PIN digitalRead(2)
#define DOOR_SENSOR_PIN digitalRead(53) // 5V=DOOR OPEN / GND=DOOR CLOSED
#define RELAY_ON     digitalWrite(8, HIGH)
#define RELAY_OFF    digitalWrite(8, LOW)
#define READ_ADD_1_SEC_BTN  digitalRead(23)
#define READ_SUB_1_SEC_BTN  digitalRead(25)

#define BTN_ADD_1_SECOND 23
#define BTN_RMV_1_SECOND 25

#define SCREEN_WIDTH   128 // OLED display width, in pixels
#define SCREEN_HEIGHT  32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C /// both are 0x3C
Adafruit_SSD1306 LED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long debounce_timeout   = 250;  // milliseconds
unsigned long last_debounce_time = 0;   // milliseconds
unsigned long led_on_time         = 3000;
unsigned long last_led_start_time = 0;
unsigned long total_shaft_count   = 0;

boolean prev_sensor_value = true; // PULL-UP, AKA no shaft present = HIGH
boolean turn_on_blaster_relay      = false;

// this enum will make it cleaner to control the OLED screen
enum BTN_ACTION_ENUM {
  OLED_ADD_1_SECOND,
  OLED_SUB_1_SECOND,
  OLED_INCREMENT_SHAFT_COUNT,
  OLED_NO_ACTION
};

void delaySafeMillis(unsigned long timeToWaitMilli) {
  unsigned long start_time = millis();
  while (millis() - start_time <= timeToWaitMilli) { /* just hang out */ }
}

void redrawOLEDScreen(BTN_ACTION_ENUM btn_action) {
  switch (btn_action) {
    case OLED_ADD_1_SECOND:
      led_on_time += 1000;
      if(led_on_time > 10000) led_on_time = 10000; // latch @ 10 secs max
      break;
    case OLED_SUB_1_SECOND:
      led_on_time -= 1000;
      if(led_on_time <= 0)    led_on_time = 1000;  // latch @ 1 sec min
      break;
    case OLED_INCREMENT_SHAFT_COUNT:
      total_shaft_count += 1;
      break;
    case OLED_NO_ACTION:
    default:
      // default cases
      break;
  }

  LED.clearDisplay();
  LED.setTextColor(SSD1306_WHITE);
  LED.setTextSize(1);
  
  // redraw current blast time
  LED.setCursor(0,0);
  LED.print("ON TIME: ");
  unsigned long seconds = led_on_time / 1000;
  LED.print((int)seconds);
  LED.println(" SEC\n");

  // redraw total # shafts blasted (since last microcontroller restart)
  //LED.setCursor(64, 0);
  LED.print("COUNT: ");
  //LED.setCursor(64, 20);
  LED.print((int)total_shaft_count);

  LED.display();
}

void setup() {
#ifdef USING_SERIAL_FOR_DEBUG
  Serial.begin(115200);
#endif

  pinMode(2, INPUT);
  pinMode(53, INPUT);
  pinMode(8, OUTPUT);
  pinMode(23, INPUT_PULLUP); // add 1 second button
  pinMode(25, INPUT_PULLUP); // remove 1 second button

  RELAY_OFF;

  if(!LED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
  {
#ifdef USING_SERIAL_FOR_DEBUG
    Serial.println(F("SSD1306 allocation failed"));
#endif   
    for(;;); // Don't proceed, loop forever
  } 

  redrawOLEDScreen(BTN_ACTION_ENUM::OLED_NO_ACTION);

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
  redrawOLEDScreen(BTN_ACTION_ENUM::OLED_INCREMENT_SHAFT_COUNT);
}

void loop() {
  bool current_add_1_sec_btn_value = !READ_ADD_1_SEC_BTN; // LOW = BUTTON PUSHED
  bool current_sub_1_sec_btn_value = !READ_SUB_1_SEC_BTN; // LOW = BUTTON PUSHED
  bool current_shaft_sensor_value = SHAFT_SENSOR_PIN; // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  bool current_door_sensor_value = DOOR_SENSOR_PIN; // LOW = DOOR CLOSED // HIGH = DOOR OPEN

  if(millis() - last_debounce_time > debounce_timeout) {
    if(current_add_1_sec_btn_value) {
      redrawOLEDScreen(BTN_ACTION_ENUM::OLED_ADD_1_SECOND);
      last_debounce_time = millis(); 
    } 
    else if (current_sub_1_sec_btn_value) {
      redrawOLEDScreen(BTN_ACTION_ENUM::OLED_SUB_1_SECOND);
      last_debounce_time = millis();
    } 
    
    if(!current_door_sensor_value) {
      if (prev_sensor_value == true && current_shaft_sensor_value == false) { // check if this is a HIGH->LOW transition
        #ifdef USING_SERIAL_FOR_DEBUG
        Serial.println(F("[INFO] DEBOUNCE TRIGGERED!"));
        #endif
        Start_Blasting();
        last_led_start_time = millis();
        last_debounce_time = last_led_start_time; 
      }
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
      Serial.println(F("[INFO] SHAFT NO LONGER PRESENT OR DOOR OPEN. STOPPING."));
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