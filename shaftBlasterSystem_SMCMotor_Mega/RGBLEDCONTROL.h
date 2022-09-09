#ifndef RGB_LED_CONTROL_H
#define RGB_LED_CONTROL_H

#include "Mega2560PinDefs.h"
#include <FastLED.h>

#define NUM_LEDS 5

CRGB status_LEDs[NUM_LEDS];
typedef enum { RGB_NO_ERROR = 0x008000, RGB_ERROR = 0xFF0000, RGB_PROCESSING = 0xF5F5F5, RGB_OFF = 0x000000 } RGB_CLR_IND; // Green / Red / WhiteSmoke (From FastLED Library)
typedef enum { LED_IDLE = 0, LED_HOMING = 1, LED_EXTENDING = 2, LED_BLASTING = 3, LED_ERROR = 4 } LED_STATE;

void rgbClearLEDS() {
	for (int i = 0; i < NUM_LEDS; ++i) {
		status_LEDs[i] = RGB_CLR_IND::RGB_OFF;
	}
	FastLED.show();
}

void rgbSetLEDColor(LED_STATE LED_NUM, RGB_CLR_IND LED_COLOR) {
	rgbClearLEDS();
	status_LEDs[LED_NUM] = LED_COLOR;
	FastLED.show();
}

void initializeRGBLEDStatusIndicator() {
	FastLED.addLeds < WS2812B, LED_STATUS_INDICATOR_PIN, GRB >(status_LEDs, NUM_LEDS);
	FastLED.setBrightness(200);
	rgbClearLEDS();
}

#endif