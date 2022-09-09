#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <avr/wdt.h>

// convenience defines
#define RELAY_ON            digitalWrite(8, HIGH)
#define RELAY_OFF           digitalWrite(8, LOW)
#define SHAFT_SENSOR_PIN    digitalRead(2)
#define DOOR_SENSOR_PIN     digitalRead(53) // 5V=DOOR OPEN / GND=DOOR CLOSED

#define SCREEN_WIDTH   128 // OLED display width, in pixels
#define SCREEN_HEIGHT  32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C /// both are 0x3C
Adafruit_SSD1306 LED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long debounce_timeout    = 250;  // milliseconds
unsigned long last_debounce_time  = 0;   // milliseconds
unsigned long led_on_time         = 3000; // milliseconds.. whatever you set this to will be the starting time
unsigned long last_led_start_time = 0;
unsigned long total_shaft_count   = 0;

boolean prev_sensor_value     = true; // PULL-UP, AKA no shaft present = HIGH
boolean turn_on_blaster_relay = false;

// this enum will make it cleaner to control the OLED screen
enum BTN_ACTION_ENUM {
  OLED_INCREMENT_SHAFT_COUNT,
  OLED_NO_ACTION
};

void delaySafeMillis(unsigned long timeToWaitMilli) {
  unsigned long start_time = millis();
  while (millis() - start_time <= timeToWaitMilli) { /* just hang out */ }
}

void redrawOLEDScreen(BTN_ACTION_ENUM btn_action) {
  switch (btn_action) {
    case (BTN_ACTION_ENUM::OLED_INCREMENT_SHAFT_COUNT):
      total_shaft_count += 1;
      break;
    case (BTN_ACTION_ENUM::OLED_NO_ACTION):
      break;
    default:
      break;
  }

  delaySafeMillis(5);

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
  LED.print("COUNT: ");
  LED.print((int)total_shaft_count);

  LED.display();
}

void setWDT(byte sWDT) {
  WDTCSR |= 0b00011000; // get register ready for writing
                        // (we have 4 CPU cycles to change the register)
  WDTCSR = sWDT | WDTO_2S; // WDT reset arduino after 4 seconds of inactivity
  wdt_reset(); // confirm the settings
}

void setup() {
  wdt_disable(); // data sheet recommends disabling wdt immediately while uC starts up

  pinMode(2, INPUT);
  pinMode(53, INPUT);
  pinMode(8, OUTPUT);
  
  delaySafeMillis(5);

  RELAY_OFF;

  if(!LED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
  {
    for(;;); // Don't proceed, loop forever
  } 

  redrawOLEDScreen(BTN_ACTION_ENUM::OLED_NO_ACTION);

  setWDT(0b00001000); // 00001000 = just reset if WDT not handled within timeframe
                      // 01001000 = set to trigger interrupt then reset
                      // 01000000 = just interrupt 
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

  prev_sensor_value = current_shaft_sensor_value;
}