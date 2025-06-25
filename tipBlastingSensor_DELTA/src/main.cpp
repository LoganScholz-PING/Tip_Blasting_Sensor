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

bool enableSerialDebug = true;

#define SHAFT_SENSE_PIN 2
#define MODE_PIN 14
#define RELAY_CTRL_PIN 8
#define DOOR_SENSE_PIN 53

// convenience defines
#define RELAY_ON         digitalWrite(RELAY_CTRL_PIN, HIGH)
#define RELAY_OFF        digitalWrite(RELAY_CTRL_PIN, LOW)
#define SHAFT_SENSOR     digitalRead(SHAFT_SENSE_PIN)
#define DOOR_SENSOR      digitalRead(DOOR_SENSE_PIN) // 5V=DOOR OPEN / GND=DOOR CLOSED
#define MODE_CHOICE      digitalRead(MODE_PIN);

// delta signals

/*
Need:
FROM Delta:
1. DELTA_INPUT_CELL_ON
2. CELL_FAULTED
3. CELL_IN_AUTO_MODE
4. CELL_SHAFT_IN_PLACE ---> DONE ()

TO Delta:
1. MACHINE_IS_SAFE ---> DONE (Except for maybe flipping logical state)
2. CURRENTLY_BLASTING ---> DONE (Except for maybe flipping logical state)
3. HEARTBEAT ---> STATIC TRUE when machine is ON
*/

// "DELTA_INPUT" means I am receiving this signal from DELTA
// "DELTA_OUTPUT" means I am transmitting this signal to DELTA
#define DELTA_OUTPUT_HEARTBEAT_PIN          33 // HB
#define DELTA_OUTPUT_SHAFT_IN_PLACE_PIN     37
#define DELTA_OUTPUT_CURRENTLY_BLASTING_PIN 45
#define DELTA_OUTPUT_MACHINE_SAFE_PIN       49

#define DELTA_INPUT_CELL_SHAFT_IN_PLACE_PIN 24
#define DELTA_INPUT_CELL_ON_PIN             28
#define DELTA_INPUT_CELL_FAULTED_PIN        32
#define DELTA_INPUT_CELL_IN_AUTO_PIN        36 

#define DELTA_MACHINE_NOT_SAFE     digitalWrite(DELTA_OUTPUT_MACHINE_SAFE_PIN, HIGH)
#define DELTA_MACHINE_IS_SAFE      digitalWrite(DELTA_OUTPUT_MACHINE_SAFE_PIN, LOW)
#define DELTA_NOT_BLASTING         digitalWrite(DELTA_OUTPUT_CURRENTLY_BLASTING_PIN, HIGH)
#define DELTA_CURRENTLY_BLASTING   digitalWrite(DELTA_OUTPUT_CURRENTLY_BLASTING_PIN, LOW)
#define DELTA_NO_SHAFT             digitalWrite(DELTA_OUTPUT_SHAFT_IN_PLACE_PIN, HIGH)
#define DELTA_YES_SHAFT            digitalWrite(DELTA_OUTPUT_SHAFT_IN_PLACE_PIN, LOW)

#define DELTA_CELL_SHAFT_IN_PLACE  !digitalRead(DELTA_INPUT_CELL_SHAFT_IN_PLACE_PIN)
#define DELTA_CELL_ON              !digitalRead(DELTA_INPUT_CELL_ON_PIN)
#define DELTA_CELL_FAULTED         !digitalRead(DELTA_INPUT_CELL_FAULTED_PIN)
#define DELTA_CELL_IN_AUTO         !digitalRead(DELTA_INPUT_CELL_IN_AUTO_PIN)

#define NEX_CLOSED         2
#define NEX_NO             3
#define NEX_NO_SHAFT       4
#define NEX_OPEN           5
#define NEX_YES            6
#define NEX_NOT_SAFE       7
#define NEX_SAFE           8
#define NEX_AUTOMATIC_MODE 9
#define NEX_MANUAL_MODE    10

#define EEPROM_CONTENTS_START_ADDRESS 0 // 4 bytes wide

enum CLUB_TYPE { GRAPHITE, IRON, GENERIC };

SoftwareSerial swSerial(11, 12); // nextion display will be connected to 11(RX-BLUE) and 12(TX-YELLOW)
EasyNex myNex(swSerial);

unsigned long debounce_timeout    = 250;  // milliseconds
unsigned long last_debounce_time  = 0;    // milliseconds
unsigned long totalBlastTime         = 7000; // milliseconds
unsigned long totalBlastTime_min = 1000;
unsigned long totalBlastTime_max = 30000;
unsigned long previousBlastStartTime = 0;
unsigned long total_shaft_count   = 0;
unsigned long lastHeartbeatTime = 0;  // HB
unsigned long heartbeatPulseLength = 150;  // HB

// FOR EEPROM OPERATIONS:
unsigned long EEPROM_last_pwr_cycle_shaft_count = 0;
unsigned long EEPROM_save_period = 3600000; // 1 hour
unsigned long EEPROM_last_save_time = 0;

struct EEPROM_CONTENTS 
{
  // set default values initially
  unsigned long EEPROM_total_shaft_count = 0;
  CLUB_TYPE club_type = CLUB_TYPE::GENERIC; // club type no longer tracked, making this generic for now
  unsigned long saved_on_time = 4000;
};

EEPROM_CONTENTS ec;

boolean prev_shaft_sensor_value = true; // PULL-UP, AKA no shaft present = HIGH
boolean prev_door_sensor_value = true; // DOOR-OPEN, starting with door open as default for safety
boolean prev_sip_delta_value = false; 
boolean machineCurrentlyBlasting = false;
boolean prev_delta_cell_on_value = false;
boolean prev_delta_cell_faulted_value = true;
boolean prev_delta_cell_in_auto_value = false;

boolean currentShaftBlastHasBeenHandled_Delta = false;
boolean currentShaftBlastHasBeenHandled_Ping = false;

volatile boolean heartbeatLogicalState = false; // HB

// variables for tracking nextion button presses
bool nexbtn_sub_1_second = false;
bool nexbtn_add_1_second = false;
bool nexbtn_reset_eeprom = false;
//bool nexbtn_switch_club_type = false;

bool ModeStatus_ManualIfTrueAutoIfFalse = true; // false = AUTO MODE, true = MANUAL MODE
bool mode_switch_previous_value = false;

void delaySafeMillis(unsigned long timeToWaitMilli) 
{
  unsigned long start_time = millis();
  while (millis() - start_time <= timeToWaitMilli) { /* just hang out */ }
}

// when WDT does not get reset this interrupt happens then the arduino restarts
ISR(WDT_vect) 
{
  wdt_disable();
  // quickly save our data to the EEPROM before resetting:
  ec.EEPROM_total_shaft_count += (total_shaft_count - EEPROM_last_pwr_cycle_shaft_count);
  ec.saved_on_time = totalBlastTime;
  EEPROM.put(EEPROM_CONTENTS_START_ADDRESS, ec);
  WDTCSR = bit (WDE); // this should reset the Arduino
}

void setWDT(byte sWDT) 
{
  WDTCSR |= 0b00011000; // get register ready for writing
                        // (we have 4 CPU cycles to change the register)
  WDTCSR = sWDT | WDTO_2S; // WDT reset arduino after 4 seconds of inactivity
  wdt_reset(); // confirm the settings
}

void outputHeartbeatSignal() // old HB
{
  unsigned long current = millis();

  if((current - lastHeartbeatTime) >= heartbeatPulseLength)
  {
    // question: safeguard heartbeat time against rollover?
    heartbeatLogicalState = !heartbeatLogicalState;
    digitalWrite(DELTA_OUTPUT_HEARTBEAT_PIN, heartbeatLogicalState);
    lastHeartbeatTime = current;
  }
}

void updateManualOrAutoModeStatusTextOnNextionScreen()
{
  if(ModeStatus_ManualIfTrueAutoIfFalse) myNex.writeNum("p12.pic", NEX_MANUAL_MODE);
  else myNex.writeNum("p12.pic", NEX_AUTOMATIC_MODE);
}

void resetBeforeEnteringManualMode()
{
  if(enableSerialDebug) Serial.println("[INFO] Switching machine to MANUAL MODE");
  machineCurrentlyBlasting = false;
  RELAY_OFF;

  updateManualOrAutoModeStatusTextOnNextionScreen();
}

void resetBeforeEnteringAutoMode()
{
  if(enableSerialDebug) Serial.println("[INFO] Switching machine to AUTOMATIC MODE");
  machineCurrentlyBlasting = false;
  RELAY_OFF;

  updateManualOrAutoModeStatusTextOnNextionScreen();
}

void initOnStartup()
{
  // read the current value of all signals and 
  prev_shaft_sensor_value  = SHAFT_SENSOR;    // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  prev_door_sensor_value   = DOOR_SENSOR;     // LOW = DOOR CLOSED // HIGH = DOOR OPEN
  // external signals from delta machine
  prev_sip_delta_value     = DELTA_CELL_SHAFT_IN_PLACE;
  prev_delta_cell_on_value = DELTA_CELL_ON;
  prev_delta_cell_faulted_value = DELTA_CELL_FAULTED;
  prev_delta_cell_in_auto_value = DELTA_CELL_IN_AUTO;

  if(!prev_shaft_sensor_value) 
  {
    DELTA_YES_SHAFT;
    myNex.writeNum("p6.pic", NEX_YES);
    myNex.writeNum("p3.pic", NEX_YES);      // DETECT SHAFT PRESENT
  }
  else if (prev_shaft_sensor_value) 
  {
    DELTA_NO_SHAFT;
    myNex.writeNum("p6.pic", NEX_NO_SHAFT);
    myNex.writeNum("p3.pic", NEX_NO_SHAFT); // DETECT SHAFT ABSENT
  }

  if(!prev_door_sensor_value) 
  {
    DELTA_MACHINE_IS_SAFE; // to delta
    myNex.writeNum("p2.pic", NEX_CLOSED);   // DETECT DOOR OPEN->CLOSE TRANSITION
    myNex.writeNum("p5.pic", NEX_SAFE);
  } 
  else if (prev_door_sensor_value) 
  {
    DELTA_MACHINE_NOT_SAFE; // to delta
    myNex.writeNum("p2.pic", NEX_OPEN);     // DETECT DOOR CLOSE->OPEN TRANSITION
    myNex.writeNum("p5.pic", NEX_NOT_SAFE);
  }

  if(prev_sip_delta_value) 
  {
    myNex.writeNum("p8.pic", NEX_YES);
    currentShaftBlastHasBeenHandled_Delta = true; // VET THIS!!
  }
  else if(!prev_sip_delta_value) 
  {
    myNex.writeNum("p8.pic", NEX_NO_SHAFT);
    currentShaftBlastHasBeenHandled_Delta = true; // VET THIS!!
  }

  if(prev_delta_cell_on_value) 
  {
    myNex.writeNum("p9.pic", NEX_YES);
  }
  else if(!prev_delta_cell_on_value) 
  {
    myNex.writeNum("p9.pic", NEX_NO);
  }

  if(prev_delta_cell_faulted_value) 
  {
    myNex.writeNum("p10.pic", NEX_YES);
  }
  else if(!prev_delta_cell_faulted_value) 
  {
    myNex.writeNum("p10.pic", NEX_NO);
  }

  if(prev_delta_cell_in_auto_value) 
  {
    myNex.writeNum("p11.pic", NEX_YES);
  }
  else if(!prev_delta_cell_in_auto_value) 
  {
    myNex.writeNum("p11.pic", NEX_NO);
  }

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}

void outputHeartbeatSignal_WithTimer() // new HB
{
  heartbeatLogicalState = !heartbeatLogicalState;
  digitalWrite(DELTA_OUTPUT_HEARTBEAT_PIN, heartbeatLogicalState);
}

void updateNextionScreen() 
{
  myNex.writeStr("t0.txt", String(total_shaft_count));
  unsigned long seconds = totalBlastTime / 1000;
  myNex.writeStr("t1.txt", String(seconds));
  myNex.writeStr("t2.txt", String(ec.EEPROM_total_shaft_count));
}

void updateEEPROMContents() 
{
  ec.EEPROM_total_shaft_count += (total_shaft_count - EEPROM_last_pwr_cycle_shaft_count);
  ec.saved_on_time = totalBlastTime;
  // save this total new value back into EEPROM
  EEPROM.put(EEPROM_CONTENTS_START_ADDRESS, ec);
  // reset our tracking variable so we can get an accurate shaft delta next EEPROM update
  EEPROM_last_pwr_cycle_shaft_count = total_shaft_count;

  if(enableSerialDebug)
  {
    Serial.println(" -- SAVED PARAMS: -- ");
    Serial.print("ec.club_type:");
    Serial.println(ec.club_type);
    Serial.print("ec.EEPROM_total_shaft_count:");
    Serial.println(ec.EEPROM_total_shaft_count);
    Serial.print("ec.saved_on_time:");
    Serial.println(ec.saved_on_time);
  }

  updateNextionScreen();
}

void loadEEPROMContents() 
{
  EEPROM.get(EEPROM_CONTENTS_START_ADDRESS, ec);
  
  // check for uninitialized EEPROM (will be all 1's)
  if(ec.saved_on_time > 30000) {
    ec.saved_on_time = 7000;
    ec.club_type = CLUB_TYPE::GRAPHITE;
    ec.EEPROM_total_shaft_count = 0;
    updateEEPROMContents(); // write the defaults to EEPROM
  }

  totalBlastTime = ec.saved_on_time;

  if(enableSerialDebug)
  {
    Serial.println(" -- PARAMS LOADED UPON STARTUP: -- ");
    Serial.print("ec.club_type:");
    Serial.println(ec.club_type);
    Serial.print("ec.EEPROM_total_shaft_count:");
    Serial.println(ec.EEPROM_total_shaft_count);
    Serial.print("ec.saved_on_time:");
    Serial.println(ec.saved_on_time);
  }
}

void clearEEPROMContents() 
{
  // set first 4 bytes to 0 (ADDR0,1,2,3)
  ec.EEPROM_total_shaft_count = 0x0;
  ec.saved_on_time = totalBlastTime;
  EEPROM.put(EEPROM_CONTENTS_START_ADDRESS, ec); // reset total shaft count EEPROM to 0
  updateNextionScreen();
}

void setup() 
{
  wdt_disable(); // data sheet recommends disabling wdt immediately while uC starts up
  
  if(enableSerialDebug) Serial.begin(9600);
  myNex.begin(38400);

  pinMode(SHAFT_SENSE_PIN, INPUT); // shaft sensor pin
  pinMode(DOOR_SENSE_PIN, INPUT_PULLUP); // 5V = DOOR OPEN. Door 
                             // safety pin. INPUT_PULLUP so that if door sensor is missing,
                             // the system will safely default to a "DOOR OPEN" state
  pinMode(MODE_PIN, INPUT_PULLUP); // 5V(HIGH) = MANUAL MODE, GND(LOW) = AUTO MODE
  pinMode(RELAY_CTRL_PIN, OUTPUT); // relay on/off pin

  pinMode(DELTA_OUTPUT_MACHINE_SAFE_PIN, OUTPUT);
  pinMode(DELTA_OUTPUT_HEARTBEAT_PIN, OUTPUT);
  pinMode(DELTA_OUTPUT_CURRENTLY_BLASTING_PIN, OUTPUT);
  pinMode(DELTA_OUTPUT_SHAFT_IN_PLACE_PIN, OUTPUT);

  pinMode(DELTA_INPUT_CELL_SHAFT_IN_PLACE_PIN, INPUT);
  pinMode(DELTA_INPUT_CELL_ON_PIN, INPUT); 
  pinMode(DELTA_INPUT_CELL_FAULTED_PIN, INPUT); 
  pinMode(DELTA_INPUT_CELL_IN_AUTO_PIN, INPUT); 

  delaySafeMillis(5);

  DELTA_MACHINE_NOT_SAFE; // to delta
  DELTA_NOT_BLASTING; // to delta
  DELTA_NO_SHAFT;

  RELAY_OFF;

  // hard code to TRUE (which means setting the pin false because it will go through opto-isolation)
  digitalWrite(DELTA_OUTPUT_HEARTBEAT_PIN, false);  // HB 

  ModeStatus_ManualIfTrueAutoIfFalse = MODE_CHOICE; // check initial state of the switch
  mode_switch_previous_value = ModeStatus_ManualIfTrueAutoIfFalse;

  if(!ModeStatus_ManualIfTrueAutoIfFalse)
  {
    resetBeforeEnteringAutoMode();
  }
  else if(ModeStatus_ManualIfTrueAutoIfFalse)
  {
    resetBeforeEnteringManualMode();
  }

  setWDT(0b01000000); // 00001000 = just reset if WDT not handled within timeframe
                      // 01001000 = set to trigger interrupt then reset
                      // 01000000 = just interrupt

  loadEEPROMContents();

  initOnStartup(); // poll all inputs and reflect them to nextion screen

  updateNextionScreen();
}

void Start_Blasting() 
{
  DELTA_CURRENTLY_BLASTING; // signal to delta
  machineCurrentlyBlasting = true;
  myNex.writeNum("p4.pic", NEX_YES); // "BLASTING = YES"
  myNex.writeNum("p7.pic", NEX_YES);
  RELAY_ON;
}

void Stop_Blasting() 
{
  machineCurrentlyBlasting = false;
  RELAY_OFF;
  DELTA_NOT_BLASTING; // signal to delta
  myNex.writeNum("p4.pic", NEX_NO); // "BLASTING = NO"
  myNex.writeNum("p7.pic", NEX_NO);
  total_shaft_count += 1;
  updateNextionScreen(); // update the shaft count
}

void handleMillisRolloverCondition() 
{
  // Shaft debounce rollover:
  if(millis() < last_debounce_time) last_debounce_time = 0;
  // EEPROM rollover:
  if((unsigned long)millis() < EEPROM_last_save_time) EEPROM_last_save_time = 0;
}

bool checkDeltaMachineIsWorkingAndAvailable()
{
  bool out = false;

  bool current_delta_cell_on_value = DELTA_CELL_ON;
  bool current_delta_cell_faulted_value = DELTA_CELL_FAULTED;
  bool current_delta_cell_in_auto_value = DELTA_CELL_IN_AUTO;

  out = current_delta_cell_on_value and current_delta_cell_in_auto_value and !current_delta_cell_faulted_value;

  return out;
}

int eeprom_update_counter = 0; // DEBUG
void automationModeLoop()
{
  // my local machine signals
  bool current_shaft_sensor_value  = SHAFT_SENSOR;    // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  bool current_door_sensor_value   = DOOR_SENSOR;     // LOW = DOOR CLOSED // HIGH = DOOR OPEN
  // external signals from delta machine
  bool current_delta_sip_value     = DELTA_CELL_SHAFT_IN_PLACE;
  bool current_delta_cell_on_value = DELTA_CELL_ON;
  bool current_delta_cell_faulted_value = DELTA_CELL_FAULTED;
  bool current_delta_cell_in_auto_value = DELTA_CELL_IN_AUTO;

  bool DELTA_MACHINE_AVAILABLE = current_delta_cell_on_value and current_delta_cell_in_auto_value and !current_delta_cell_faulted_value;

  // update the nextion display machine status indicators for DOOR PRESENCE
  if(current_door_sensor_value != prev_door_sensor_value) 
  {
    if(!current_door_sensor_value) 
    {
      DELTA_MACHINE_IS_SAFE; // to delta
      myNex.writeNum("p2.pic", NEX_CLOSED);   // DETECT DOOR OPEN->CLOSE TRANSITION
      myNex.writeNum("p5.pic", NEX_SAFE);
      if(enableSerialDebug) Serial.println("[INFO] DOOR CLOSE OPEN->CLOSE TRANSITION");
    } 
    else if (current_door_sensor_value) 
    {
      DELTA_MACHINE_NOT_SAFE; // to delta
      myNex.writeNum("p2.pic", NEX_OPEN);     // DETECT DOOR CLOSE->OPEN TRANSITION
      myNex.writeNum("p5.pic", NEX_NOT_SAFE);
      if(enableSerialDebug) Serial.println("[INFO] DOOR CLOSE CLOSE->OPEN TRANSITION");
    }
  }

  // ======= DELTA SPECIFIC NEXTION DISPLAY FUNCTIONS =======
  

  // update nextion display DELTA_CELL_ON information
  if(current_delta_cell_on_value != prev_delta_cell_on_value) 
  {
    if(current_delta_cell_on_value) 
    {
      myNex.writeNum("p9.pic", NEX_YES);
      if(enableSerialDebug) Serial.println("[INFO] DELTA CELL ON NO->YES TRANSITION");
    }
    else if(!current_delta_cell_on_value) 
    {
      myNex.writeNum("p9.pic", NEX_NO);
      if(enableSerialDebug) Serial.println("[INFO] DELTA CELL ON YES->NO TRANSITION");
    }
  }

  // update nextion display DELTA_CELL_FAULTED information
  if(current_delta_cell_faulted_value != prev_delta_cell_faulted_value) 
  {
    if(current_delta_cell_faulted_value) 
    {
      myNex.writeNum("p10.pic", NEX_YES);
      if(enableSerialDebug) Serial.println("[INFO] DELTA CELL FAULTED NO->YES TRANSITION");
    }
    else if(!current_delta_cell_faulted_value) 
    {
      myNex.writeNum("p10.pic", NEX_NO);
      if(enableSerialDebug) Serial.println("[INFO] DELTA CELL FAULTED YES->NO TRANSITION");
    }
  }

  // update nextion display DELTA_CELL_IN_AUTO information
  if(current_delta_cell_in_auto_value != prev_delta_cell_in_auto_value) 
  {
    if(current_delta_cell_in_auto_value) 
    {
      myNex.writeNum("p11.pic", NEX_YES);
      if(enableSerialDebug) Serial.println("[INFO] DELTA CELL IN AUTO NO->YES TRANSITION");
    }
    else if(!current_delta_cell_in_auto_value) 
    {
      myNex.writeNum("p11.pic", NEX_NO);
      if(enableSerialDebug) Serial.println("[INFO] DELTA CELL IN AUTO YES->NO TRANSITION");
    }
  }

  // update the nextion display machine status indicators for SHAFT PRESENCE
  if(current_shaft_sensor_value != prev_shaft_sensor_value) 
  {
    if(!current_shaft_sensor_value) 
    {
      DELTA_YES_SHAFT;
      myNex.writeNum("p6.pic", NEX_YES);
      myNex.writeNum("p3.pic", NEX_YES);      // DETECT SHAFT PRESENT
    }
    else if (current_shaft_sensor_value) 
    {
      DELTA_NO_SHAFT;
      myNex.writeNum("p6.pic", NEX_NO_SHAFT);
      myNex.writeNum("p3.pic", NEX_NO_SHAFT); // DETECT SHAFT ABSENT
    }
  }

  handleMillisRolloverCondition(); // for both shaft timer and eeprom timer

  // update nextion display shaft in place information
  if(current_delta_sip_value != prev_sip_delta_value) 
  {
    if(current_delta_sip_value) 
    {
      myNex.writeNum("p8.pic", NEX_YES);
      if(enableSerialDebug) Serial.println("[INFO] DELTA SIP NO->YES TRANSITION");

      // for aligning the local "shaft in place" logic to the external Delta robot "shaft in place" logic. It will keep blasting over and over without this
      currentShaftBlastHasBeenHandled_Delta = false; 
    }
    else if(!current_delta_sip_value) 
    {
      myNex.writeNum("p8.pic", NEX_NO_SHAFT);
      if(enableSerialDebug) Serial.println("[INFO] DELTA SIP YES->NO TRANSITION");

      //currentShaftBlastHasBeenHandled_Delta = true; // test 
    }
  }

  if(DELTA_MACHINE_AVAILABLE && !currentShaftBlastHasBeenHandled_Delta && !machineCurrentlyBlasting && (millis() - last_debounce_time > debounce_timeout)) 
  {
    if(!current_door_sensor_value) 
    {
      // this dual check for a shaft may be redundant, probably just need current_shaft_sensor_value 
      if(current_delta_sip_value && !current_shaft_sensor_value) 
      {
        Start_Blasting();
        currentShaftBlastHasBeenHandled_Delta = true;
        //currentShaftBlastHasBeenHandled_Ping = true;
        previousBlastStartTime = millis();
        last_debounce_time = previousBlastStartTime; 
      }
    }
  }

  if (machineCurrentlyBlasting) 
  {
    if(current_door_sensor_value) 
    {
      // Something has opened the door during a blast cycle
      if(enableSerialDebug) Serial.println("[INFO] STOPPED BLASTING. Reason: DOOR OPEN");
      Stop_Blasting();
    }
    else if(!current_delta_sip_value) 
    {
      // Delta machine has removed the shaft from the sensor for some reason
      // TODO: Maybe don't need this? Probably safe to leave it
      if(enableSerialDebug) Serial.println("[INFO] STOPPED BLASTING. DELTA SHAFT NOT IN PLACE");
      Stop_Blasting();
    }
    else if(!DELTA_MACHINE_AVAILABLE) 
    {
      // Delta machine has become unavailable for any reason
      if(enableSerialDebug) Serial.println("[INFO] STOPPED BLASTING. DELTA MACHINE NOT AVAILABLE");
      Stop_Blasting();
    }
    else if(millis() - previousBlastStartTime > totalBlastTime) 
    { 
      // shaft has been present for entire blast. turn off blasters
      if(enableSerialDebug) Serial.println("[INFO] STOPPED BLASTING (success). BLAST TIME ACCOMPLISHED!");
      Stop_Blasting();
    }
    else if (current_shaft_sensor_value) 
    { 
      // HIGH = NO SHAFT PRESENT
      // Something or someone has moved the shaft away from the sensor
      if(enableSerialDebug) Serial.println("[INFO] STOPPED BLASTING. PHYSICAL SHAFT REMOVED FROM SENSOR");
      myNex.writeNum("p3.pic", NEX_NO_SHAFT); // "SHAFT SENSE = NO SHAFT"
      Stop_Blasting();
    }
  }

  myNex.NextionListen();

  if(nexbtn_sub_1_second) 
  {
    totalBlastTime -= 1000;
    if(totalBlastTime <= 0) totalBlastTime = totalBlastTime_min; // clamp to a min time
    updateEEPROMContents();
  }

  if(nexbtn_add_1_second) 
  {
    totalBlastTime += 1000;
    if(totalBlastTime > totalBlastTime_max) totalBlastTime = totalBlastTime_max; // clamp to a max time
    updateEEPROMContents();
  }

  if(nexbtn_reset_eeprom) 
  {
    clearEEPROMContents();
  }

  // update EEPROM every EEPROM_save_period milliseconds
  if((unsigned long)millis() - EEPROM_last_save_time >= EEPROM_save_period) 
  {
    updateEEPROMContents();
    EEPROM_last_save_time = millis();
  }

  nexbtn_sub_1_second = false;
  nexbtn_add_1_second = false;
  nexbtn_reset_eeprom = false;
  
  // local machine tracking:
  prev_shaft_sensor_value = current_shaft_sensor_value;
  prev_door_sensor_value  = current_door_sensor_value;
  // external delta machine tracking:
  prev_sip_delta_value = current_delta_sip_value;
  prev_delta_cell_on_value = current_delta_cell_on_value;
  prev_delta_cell_faulted_value = current_delta_cell_faulted_value;
  prev_delta_cell_in_auto_value = current_delta_cell_in_auto_value;

  //outputHeartbeatSignal();  // HB
}

void manualModeLoop()
{
  bool current_shaft_sensor_value  = SHAFT_SENSOR;    // LOW = SHAFT PRESENT // HIGH = NO SHAFT PRESENT
  bool current_door_sensor_value   = DOOR_SENSOR;     // LOW = DOOR CLOSED // HIGH = DOOR OPEN

  // update the nextion display machine status indicators for DOOR PRESENCE
  if(current_door_sensor_value != prev_door_sensor_value) {
    if(!current_door_sensor_value) {
      myNex.writeNum("p2.pic", NEX_CLOSED);   // DETECT DOOR OPEN->CLOSE TRANSITION
    } 
    else if (current_door_sensor_value) {
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

  if(!machineCurrentlyBlasting && (millis() - last_debounce_time > debounce_timeout)) {
    if(!current_door_sensor_value) {
      if (prev_shaft_sensor_value == true && current_shaft_sensor_value == false) { // check if this is a HIGH->LOW transition
        myNex.writeNum("p3.pic", NEX_YES); // "SHAFT SENSE = YES"
        Start_Blasting();
        previousBlastStartTime = millis();
        last_debounce_time = previousBlastStartTime; 
      }
    }
  }

  if (machineCurrentlyBlasting) {
    if(current_door_sensor_value) {
      Stop_Blasting();
    }
    else if(millis() - previousBlastStartTime > totalBlastTime) { // shaft has been present for entire blast. turn off blasters
      Stop_Blasting();
    }
    else if (current_shaft_sensor_value) { // HIGH = NO SHAFT PRESENT
      myNex.writeNum("p3.pic", NEX_NO_SHAFT); // "SHAFT SENSE = NO SHAFT"
      Stop_Blasting();
    }
  }

  myNex.NextionListen();

  if(nexbtn_sub_1_second) {
    totalBlastTime -= 1000;
    if(totalBlastTime <= 0) totalBlastTime = totalBlastTime_min; // clamp to a min time
    updateEEPROMContents();
  }

  if(nexbtn_add_1_second) {
    totalBlastTime += 1000;
    if(totalBlastTime > totalBlastTime_max) totalBlastTime = totalBlastTime_max; // clamp to a max time
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
  //nexbtn_switch_club_type = false;
  
  prev_shaft_sensor_value = current_shaft_sensor_value;
  prev_door_sensor_value  = current_door_sensor_value;
}



void loop() 
{
  wdt_reset(); // if we don't reset the WDT within 2 seconds the arduino will restart
               // NOTE: If we DO restart due to WDT, the EEPROM settings will be updated before the restart
  
  bool mode_switch_current_value = MODE_CHOICE;

  if(mode_switch_current_value != mode_switch_previous_value)
  {
    ModeStatus_ManualIfTrueAutoIfFalse = mode_switch_current_value;

    if(!ModeStatus_ManualIfTrueAutoIfFalse) // False = Auto
    {
      resetBeforeEnteringAutoMode();
    }
    else if(ModeStatus_ManualIfTrueAutoIfFalse) // True = Manual Mode
    {
      resetBeforeEnteringManualMode();
    }
  }
  
  if(ModeStatus_ManualIfTrueAutoIfFalse)
  {
    manualModeLoop();
  }
  else if(!ModeStatus_ManualIfTrueAutoIfFalse)
  {
    automationModeLoop();
  }

  mode_switch_previous_value = mode_switch_current_value; 
}