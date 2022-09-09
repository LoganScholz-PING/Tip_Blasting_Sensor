#ifndef DELAY_SAFE_H
#define DELAY_SAFE_H

void delaySafeMilli(unsigned long timeToWait) {
	unsigned long start_time = millis();
	while (millis() - start_time <= timeToWait) { /* just hang out */ }
}

void delaySafeMicro(unsigned long timeToWait) {
	unsigned long start_time = micros();
	while (micros() - start_time <= timeToWait) { /* just hang out */ }
}

#endif

