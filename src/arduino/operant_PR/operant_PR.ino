#define SKETCH_NAME "operant_PR.ino"
#define VERSION "v1.0.0"

/* ++++++++++++++++++++ INFORMATION ++++++++++++++++++++
  Meta data:
  Josh Boquiren (@thejoshbq on GitHub), Otis Lab

  "Unless the Lord builds the house, those who build it labor in vain.
  Unless the Lord watches over the city, the watchman stays awake in vain."
  Psalm 127:1

  ---------------------------------------------------------------------
  Program notes:
  - After each reward delivery, the number of required active presses is incremented.
  - An active lever press triggers a cue tone, followed by a trace interval and pump infusion, and will be labeled as "ACTIVE"
  - Presses that occur during the cue tone, trace interval, pump infusion, or timeout period will be labeled as a "TIMEOUT" press
  - All other presses will be denoted as "INACTIVE"
  - Timestamps are adjusted to the start of the program once the program is started (adusted timestamp = current timestamp - program start time)

  ---------------------------------------------------------------------
  Defaults:
  - fixed ratio, 1
  - progressive ratio, 2
  - trace interval length, 0ms (time between tone and infusion)
  - timeout period length, 20000ms (time from cue tone end)
  - cue tone length, 1600ms
  - infusion length, 2000ms
  - active lever, right-hand lever
  - laser pulse duration, 3000ms

  ---------------------------------------------------------------------
  Current pin configuration:
  - Pin 10, right-hand lever
  - Pin 13, left-hand lever
  - Pin 4, pump
  - Pin 3 (PWM capable pin), conditioned stimulus speaker (denoted as "cs") and speaker for linked/unlinked jingle
  - Pin 2, trigger for frame timestamp input signals
  - Pin 9, trigger for imaging program start and stop
  - Pin 5, lick circuit
  - Pin 6, laser

  ---------------------------------------------------------------------
  Sections:
  - Section 1: Definitions and configurations
  - Section 2: setup() and loop()
  - Section 3: Utility functions
  - Section 4: Looped functions
  - Section 5: Interrupt functions
  - Section 6: Main program

  ++++++++++++++++++++ INFORMATION ++++++++++++++++++++ */



// =======================================================
// ====================== SECTION 1 ======================
// =======================================================

// Libraries
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "Device.h"
#include "Laser.h"
#include "Lever.h"
#include "Cue.h"
#include "Pump.h"
#include "LickCircuit.h"

// Pin definitions
const byte RH_LEVER_PIN = 10;
const byte LH_LEVER_PIN = 13;
const byte CS_PIN = 3;
const byte PUMP_PIN = 4;
const byte IMAGING_TRIGGER = 9;
const byte TIMESTAMP_TRIGGER = 2;
const byte LICK_CIRCUIT_PIN = 5;
const byte LASER_PIN = 6;

// Class instantiations for components
Lever leverRH(RH_LEVER_PIN);
Lever leverLH(LH_LEVER_PIN);
Lever *activeLever = &leverRH; // pointer variable that stores a Lever object as the active lever
Lever *inactiveLever = &leverLH; // pointer variable that stores a Lever object as the inactive lever
Cue cs(CS_PIN);
Pump pump(PUMP_PIN);
LickCircuit lickCircuit(LICK_CIRCUIT_PIN);
Laser laser(LASER_PIN);

// Global Boolean variables
bool setupFinished = false; // marks setup completion
bool programIsRunning = false; // marks if the program is active or inactivate
bool linkedToGUI = false; // marks successful connection to Python
volatile bool frameSignalReceived = false; // stores if a signal was recieved from the frame input pin

// Global int variables
unsigned long baudrate = 115200; // baudrate for data transmission speed
unsigned long int differenceFromStartTime; // the difference between the time the Arduino was first powered up from when the program was started
unsigned long int traceIntervalLength = 0; // time between cue tone and infusion
unsigned long int timeoutIntervalLength = 20000; // period after an active press in which a reward is unavailable
unsigned long int timeoutIntervalStart; // int variable to store timeout interval start timestamp
unsigned long int timeoutIntervalEnd;   // int variable to store timeout interval end timestamp
unsigned long previousPing = 0; // stores last time a ping was sent
const long pingInterval = 10000; // interval at which to send a ping (milliseconds)
volatile unsigned long int frameSignalTimestamp = 0; // variable that stores the frame input timestamp
int fRatio = 1;
int pRatio = 2;
int pressCount = 0;



// =======================================================
// ====================== SECTION 2 ======================
// =======================================================

// Setup
void setup()
{
  // 2P setup
  pinMode(IMAGING_TRIGGER, OUTPUT);
  pinMode(TIMESTAMP_TRIGGER, INPUT);
  attachInterrupt(digitalPinToInterrupt(TIMESTAMP_TRIGGER), frameSignalISR, RISING);

  // laser setup
  pinMode(laser.getPin(), OUTPUT);
  laser.disarm();
  laser.setIsRunning(false);

  // RH lever setup
  pinMode(leverRH.getPin(), INPUT_PULLUP);
  leverRH.disarm();
  leverRH.setOrientation("RH");

  // LH lever setup
  pinMode(leverLH.getPin(), INPUT_PULLUP);
  leverLH.disarm();
  leverLH.setOrientation("LH");

  // CS setup
  pinMode(cs.getPin(), OUTPUT);
  cs.disarm();

  // pump setup
  pinMode(pump.getPin(), OUTPUT);
  pump.disarm();

  // lick circuit setup
  pinMode(lickCircuit.getPin(), INPUT);
  lickCircuit.disarm();

  // Serial connection
  Serial.begin(baudrate);
  delay(2000); // delay to avoid overloading buffer
  Serial.println(SKETCH_NAME);
  setupFinished = true;
}

// Loop
void loop()
{
  PROGRAM();
  monitorSerialCommands();
}



// =======================================================
// ====================== SECTION 3 ======================
// =======================================================

void pressingDataEntry(Lever *&lever, Pump *pump)
{
  /*

     Configures data entry and sends to serial connection. Delimits data by ',' char to be parsed later by Python.

     @param &lever, lever that the entry is for
     @param *pump, pump is not a required parameter
     @return, String data for events delimited by ',' char:
       1) Component
       2) Event
       3) Start timestamp
       4) End timestamp

  */
  String pressEntry;
  String infusionEntry;
  lever->setReleaseTimestamp(millis()); // sets press release timestamp
  pressEntry = lever->getOrientation() + "_LEVER,";
  pressEntry += lever->getPressType() + "_PRESS,";
  if (differenceFromStartTime)
  { // if program is started, the time stamp is adjusted to program start time (aka differenceFromStartTime)
    pressEntry += String(lever->getPressTimestamp() - differenceFromStartTime) + ",";
    pressEntry += String(lever->getReleaseTimestamp() - differenceFromStartTime);
  }
  else
  { // if program is not started, time stamp reflects time from Arduino start up
    pressEntry += String(lever->getPressTimestamp()) + ",";
    pressEntry += String(lever->getReleaseTimestamp());
  }
  Serial.println(pressEntry); // send data to serial connection
}

void definePressActivity(bool programRunning, Lever *&lever, Cue *cue, Pump *pump)
{
  /*

     Determines press activity; pump is not a required parameter.

     @param Lever* &lever, Lever pointer object that the press occurred on
     @param Cue &cue, the associated Cue object
     @param Pump *pump, pump to deliver infusion upon an active lever press; a not-required parameter
     @function, determines the activity of a press based on the time stamp and surrounding events; if the press is active, triggers cue and pump (if exists)

  */
  long int timestamp = millis(); // captures initial timestamp
  if ((cue && cue->isArmed()) && (!pump || !pump->isArmed()))
  { // if the cue is armed and pump does not exists or the pump is not armed...
    if (timestamp >= cue->getOnTimestamp() && timestamp <= cue->getOffTimestamp() || timestamp >= timeoutIntervalStart && timestamp <= timeoutIntervalEnd)
    {
      lever->setPressType("TIMEOUT");
    } // if the press occurs during the cue tone period or timeout period, label 'TIMEOUT' and do not trigger another cue tone
    else
    { // if the press occurs outside of a cue tone, label 'ACTIVE' and trigger a cue
      lever->setPressType("ACTIVE");
      if (pressCount == fRatio - 1) {
        pressCount = 0;
        fRatio = fRatio + pRatio;
        cue->setOnTimestamp(timestamp); // set the cue tone period
        cue->setOffTimestamp(timestamp);
        if (programRunning)
        {
          timeoutIntervalStart = cue->getOffTimestamp();
          timeoutIntervalEnd = timeoutIntervalStart + timeoutIntervalLength;
        }
      }
      else {
        pressCount ++;
      }
    }
  }
  else if ((cue && cue->isArmed()) && (pump && pump->isArmed()))
  { // if the cue is armed and pump exists and is also armed and laser exists and is also armed...
    if (timestamp >= cue->getOnTimestamp() && timestamp <= pump->getInfusionEndTimestamp() || timestamp >= timeoutIntervalStart && timestamp <= timeoutIntervalEnd)
    {
      lever->setPressType("TIMEOUT");
    } // if the press occurs any time during the cue tone, trace interval, or infusion period, or timeout period label 'INACTIVE' and do not trigger new events
    else
    { // if it is a normal press, label 'ACTIVE' and trigger cue tone and pump infusion
      lever->setPressType("ACTIVE");
      if (pressCount == fRatio - 1) {
        pressCount = 0;
        fRatio = fRatio + pRatio;
        cue->setOnTimestamp(timestamp);
        cue->setOffTimestamp(timestamp);
        pump->setInfusionPeriod(cue->getOffTimestamp(), traceIntervalLength);
        String infusionEntry = "PUMP,INFUSION,";
        infusionEntry += differenceFromStartTime ? String(pump->getInfusionStartTimestamp() - differenceFromStartTime) : String(pump->getInfusionStartTimestamp());
        infusionEntry += ",";
        infusionEntry += differenceFromStartTime ? String(pump->getInfusionEndTimestamp() - differenceFromStartTime) : String(pump->getInfusionEndTimestamp());
        Serial.println(infusionEntry);
        if (programRunning)
        {
          timeoutIntervalStart = cue->getOffTimestamp();
          timeoutIntervalEnd = timeoutIntervalStart + timeoutIntervalLength;
        }
      }
      else {
        pressCount ++;
      }
    }
  }
  else
  {
    lever->setPressType("INACTIVE");
  } // if the lever is armed, but not paired with a cue or pump infusion, label as the default 'NO CONDITION'
}

void startProgram(byte pin)
{
  /*

     Handles triggering the 2P microscope to start collecting frames and assigning differenceFromStartTime variable.

     @param byte pin, pin assigned as mircoscope session trigger
     @function, sends a signal to the trigger pin that starts the frame recording and also assigns the differenceFromStartTime variable to which relevant timestamps will be adjusted

  */
  Serial.println();
  Serial.println("========== PROGRAM START ==========");
  Serial.println();
  digitalWrite(pin, HIGH);            // trigger imaging to begin capturing frames
  delay(50);                          // short delay to make sure data is fully transmitted
  digitalWrite(pin, LOW);             // finish trigger
  differenceFromStartTime = millis(); // set offset to calculate timestamps from program start
  laser.setPreviousStim(millis());
}

void endProgram(byte pin)
{
  /*

     Handles triggering the 2P microscope to end collecting frames.

     @param pin, pin assigned as mircoscope trigger
     @function, sends a signal to the trigger pin that ends the frame recording

  */
  Serial.println();
  Serial.println("========== PROGRAM END ==========");
  Serial.println();
  digitalWrite(pin, HIGH); // second trigger signals end of imaging
  delay(50);
  digitalWrite(pin, LOW);
}

void setTraceInterval(unsigned long length)
{
  /*

     The trace interval is the length of time between the cue tone and the pump infusion. A user can change this value both from the Python GUI and through serial communication.

     @param length, user input value in milliseconds to change the trace interval to
     @function, sets 'traceIntervalLength' to the user input 'length'
     @return, prints the change to serial

  */
  traceIntervalLength = length;
  Serial.println("SET TRACE INTERVAL TO: " + String(traceIntervalLength));
}

void setTimeoutInterval(unsigned long length)
{
  timeoutIntervalLength = length;
  Serial.println("SET TIMEOUT INTERVAL TO: " + String(timeoutIntervalLength));
}

void connectionJingle(String connected, Cue cue)
{
  /*

     Tone jingle that plays upon changing the 'linkedToGUI' variable to true or false. Signifies when setup and user are ready to transmit data.

     @param connected, String value of either "LINK" or "UNLINK" to mirror Boolean value
     @param Cue cue, Cue object to play the tone from
     @function, plays an 'up' tone when 'linkedToGUI' is set to true and a 'down' town when 'linkedToGUI' is set to false
     @return, prints "Linked to GUI" or "Unlinked from GUI" respectively

  */
  if (connected == "LINK")
  {
    linkedToGUI = true;
    static int pitch = 500; // starting tone frequency
    for (int i = 0; i < 3; i++)
    {
      tone(cue.getPin(), pitch, 100);
      delay(100);
      noTone(cue.getPin());
      pitch += 500; // increase the frequency with each iteration
    }
    pitch = 500;              // reset the frequency
    Serial.println("LINKED"); // output connection status
  }
  else if (connected == "UNLINK")
  {
    linkedToGUI = false;
    static int pitch = 1500; // starting tone frequency
    for (int i = 0; i < 3; i++)
    {
      tone(cue.getPin(), pitch, 100);
      delay(100);
      noTone(cue.getPin());
      pitch -= 500; // decrease the frequency
    }
    pitch = 1500;               // reset the frequency
    Serial.println("UNLINKED"); // output connection status
  }
}

void sendSetupJSON()
{
  /*

     Sends program configuration data to Python.

  */
  StaticJsonDocument<200> doc;
  doc["DOC"] = SKETCH_NAME;
  doc["VERSION"] = VERSION;

  doc["TRACE INTERVAL LENGTH"] = traceIntervalLength;
  doc["TIMEOUT INTERVAL LENGTH"] = timeoutIntervalLength;
  doc["DELTA START TIME"] = differenceFromStartTime;
  doc["BAUDRATE"] = baudrate;

  doc["CS DURATION"] = cs.getDuration();
  doc["CS FREQUENCY"] = cs.getFrequency();
  doc["PUMP INFUSION LENGTH"] = pump.getInfusionDuration();
  doc["LASER STIM LENGTH"] = laser.getStimDuration();

  serializeJson(doc, Serial);
  Serial.println('\n');
}



// =======================================================
// ====================== SECTION 4 ======================
// =======================================================

void pingDevice()
{
  /*

     Pings the connected device occasionally as a keep-alive.

  */
  unsigned long currentMillis = millis();
  if (currentMillis - previousPing >= pingInterval)
  {
    previousPing = currentMillis;
    Serial.println("ping");
  }
}

void manageCue(Cue *cue)
{
  /*

     Controls cue tone operation.

     @param Cue *cue, the optional cue speaker object of interest
     @function, if the cue is armed and current time stamp is during the assigned tone time period, speaker turns on, otherwise turns the cue off

  */
  long int timestamp = millis();
  if (cue)
  {
    if (cue->isArmed())
    { // cue must be armed in order to run
      if (timestamp <= cue->getOffTimestamp() && timestamp >= cue->getOnTimestamp())
      { // if current time stamp is during the tone interval
        cue->on(); // turn on cue
        cue->setRunning(true);
      }
      else
      { // if the time stamp is not in a tone interval
        cue->off(); // turn cue off
        cue->setRunning(false);
      }
    }
  }
}

void managePump(Pump *pump)
{
  /*

     Controls pump operation; similar to 'manageCue(Cue &cue)'.

     @param *pump, pump of interest is also not a required parameter for this function to run, as this function gets fed into another method where the pump is not required
     @function, turns the pump on during the infusion period, otherwise keeps it off

  */
  long int timestamp = millis();
  if (pump->isArmed())
  { // if the pump is armed and the laser is not armed...
    if (timestamp <= pump->getInfusionEndTimestamp() && timestamp >= pump->getInfusionStartTimestamp())
    { // if the current time stamp is within the infusion time period...
      pump->on(); // turn the pump on
      pump->setRunning(true);
    }
    else
    { // outside the infusion time stamp
      pump->off(); // turn the pump off
      pump->setRunning(false);
    }
  }
}

void manageLaser(Laser &laser)
{
  unsigned long currentMillis = millis();
  String stimEntry;

  if (laser.isArmed() && programIsRunning) {
    if (currentMillis - laser.getPreviousStim() >= laser.getStimDuration()) {
      laser.setPreviousStim(currentMillis);
      if (laser.getIsRunning()) {
        laser.setIsRunning(false);
      }
      else if (!laser.getIsRunning()) {
        laser.setIsRunning(true);
        stimEntry = "LASER,STIM,";
        stimEntry += differenceFromStartTime ? String(laser.getPreviousStim() - differenceFromStartTime) : String(laser.getPreviousStim());
        stimEntry += ",";
        stimEntry += differenceFromStartTime ? String(laser.getPreviousStim() - differenceFromStartTime + laser.getStimDuration()) : String(laser.getPreviousStim() + laser.getStimDuration());
        Serial.println(stimEntry);
      }
    }
    if (laser.getIsRunning()) {
      laser.off();
    }
    else if (!laser.getIsRunning()) {
      laser.on();
    }
  }
  else {
    laser.setIsRunning(false);
    laser.off();
  }
}

void monitorPressing(bool programRunning, Lever *&lever, Cue *cue, Pump *pump)
{
  /*

     Monitors lever pressing by reading the pin's switch state. Debounces without delay() function calls. Manages setting cue tone and infusion periods.

     @param programRunning, Boolean variable that, when true, allows the function to output the proper output data
     @param Lever* &lever, pointer Lever object of interest
     @param Cue *cue, optional Cue object
     @param Pump *pump, optional Pump object
     @function, records the timestamp of a LOW pin reading and determines the activity of the press based on the timestamp in relation to associated components and other blocking time periods (i.e. timeout period)

  */
  static unsigned long lastDebounceTime = 0; // stores the last time the lever input was toggled
  const unsigned long debounceDelay = 100;   // the debounce time; increase if the output flickers
  manageCue(cue);                       // manages cue delivery
  managePump(pump);                        // manages infusion delivery
  if (lever->isArmed())
  {
    bool currentLeverState = digitalRead(lever->getPin()); // reads the current state of the lever
    if (currentLeverState != lever->getPreviousLeverState())
    { // if the lever state has changed
      lastDebounceTime = millis(); // resets the debouncing timer
    }
    if ((millis() - lastDebounceTime) > debounceDelay)
    { // if the debounce period has passed, the lever's state is stable
      if (currentLeverState != lever->getStableLeverState())
      {
        lever->setStableLeverState(currentLeverState); // update the stable lever state
        if (currentLeverState == LOW)
        { // lever press detected (assuming LOW means pressed)
          lever->setPressTimestamp(millis());                    // set the press timestamp at the moment of detection
          definePressActivity(programRunning, lever, cue, pump); // determine whether the press is active
        }
        else
        { // lever release detected
          lever->setReleaseTimestamp(millis()); // set the release timestamp at the moment of detection
          pressingDataEntry(lever, pump);       // handle data entry on lever release
        }
      }
    }
    lever->setPreviousLeverState(currentLeverState); // update the previous lever state for next iteration
  }
}

void monitorLicking(LickCircuit &lickSpout)
{
  /*

     Monitors licking in the form of HIGH vs. LOW signals from the lickSpout pin. Using similar debouncing logic to monitorPressing that avoids delay() function calls.

     @param LickCircuit &lickSpout, LickCircuit object of interest.
     @function, reads lickSpout pin and assigns a HIGH reading a timestamp once, then updates the previous timestamp recorded and previous lick state.

  */
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 25;
  if (lickSpout.isArmed())
  {
    bool currentLickState = digitalRead(lickSpout.getPin());
    if (currentLickState != lickSpout.getPreviousLickState())
    {
      lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay)
    {
      if (currentLickState != lickSpout.getStableLickState())
      {
        lickSpout.setStableLickState(currentLickState);
        if (currentLickState == HIGH)
        {
          lickSpout.setLickTouchTimestamp(millis());
        }
        else
        {
          lickSpout.setLickReleaseTimestamp(millis());
          String lickEntry = "LICK_CIRCUIT,LICK," + String(lickSpout.getLickTouchTimestamp() - differenceFromStartTime) + "," + String(lickSpout.getLickReleaseTimestamp() - differenceFromStartTime);
          Serial.println(lickEntry);
        }
      }
    }
    lickSpout.setPreviousLickState(currentLickState);
  }
}

void monitorSerialCommands()
{
  /*

     Monitors and handles incoming commands from serial connection.

  */
  if (setupFinished) {
    if (Serial.available() > 0)
    { // checks to see if there is an input from serial
      String command = Serial.readStringUntil('\n'); // stops reading string at '\n' char
      command.trim();                                // eliminates white space

      // link commands
      if (command == "LINK")
      {
        connectionJingle("LINK", cs);
      }
      else if (command == "UNLINK")
      {
        connectionJingle("UNLINK", cs);
      }

      // program commands
      else if (command == "START-PROGRAM")
      {
        startProgram(IMAGING_TRIGGER);
        sendSetupJSON();
        programIsRunning = true;
      }
      else if (command == "END-PROGRAM")
      {
        endProgram(IMAGING_TRIGGER);
        programIsRunning = false;
        delay(1000);
      }
      else if (command.startsWith("SET_RATIO:"))
      {
        long initfRatio = command.substring(String("SET_RATIO:").length()).toInt();
        fRatio = initfRatio;
      }
      else if (command.startsWith("SET_PRATIO:"))
      {
        long initpRatio = command.substring(String("SET_PRATIO:").length()).toInt();
        pRatio = initpRatio;
      }
      else if (command.startsWith("SET_TIMEOUT_PERIOD_LENGTH:"))
      {
        long initTimeoutPeriodLength = command.substring(String("SET_TIMEOUT_PERIOD_LENGTH:").length()).toInt();
        setTimeoutInterval(initTimeoutPeriodLength);
      }

      // RH lever commands
      else if (command == "ARM_LEVER_RH")
      {
        leverRH.arm();
      }
      else if (command == "DISARM_LEVER_RH")
      {
        leverRH.disarm();
      }
      else if (command == "ACTIVE_LEVER_RH")
      {
        activeLever = &leverRH;   // RH lever is active
        inactiveLever = &leverLH; // LH lever is inactive
        Serial.print("ACTIVE LEVER: ");
        Serial.println(activeLever->getOrientation());
      }

      // LH lever commands
      else if (command == "ARM_LEVER_LH")
      {
        leverLH.arm();
      }
      else if (command == "DISARM_LEVER_LH")
      {
        leverLH.disarm();
      }
      else if (command == "ACTIVE_LEVER_LH")
      {
        activeLever = &leverLH;   // LH lever is active
        inactiveLever = &leverRH; // RH lever is inactive
        Serial.print("ACTIVE LEVER: ");
        Serial.println(activeLever->getOrientation());
      }

      // CS commands
      else if (command == "ARM_CS")
      {
        cs.arm();
      }
      else if (command == "DISARM_CS")
      {
        cs.disarm();
      }
      else if (command.startsWith("SET_FREQUENCY_CS:"))
      {
        long frequency = command.substring(String("SET_FREQUENCY_CS:").length()).toInt();
        cs.setFrequency(frequency);
      }
      else if (command.startsWith("SET_DURATION_CS:"))
      {
        long duration = command.substring(String("SET_DURATION_CS:").length()).toInt();
        cs.setDuration(duration);
      }

      // pump commands
      else if (command == "ARM_PUMP")
      {
        pump.arm();
      }
      else if (command == "DISARM_PUMP")
      {
        pump.disarm();
      }
      else if (command.startsWith("SET_TRACE_INTERVAL:"))
      {
        long length = command.substring(String("SET_TRACE_INTERVAL:").length()).toInt();
        setTraceInterval(length);
      }
      else if (command == "PUMP_TEST_ON")
      {
        pump.on();
      }
      else if (command == "PUMP_TEST_OFF")
      {
        pump.off();
      }

      // laser commands
      else if (command == "ARM_LASER")
      {
        laser.arm();
      }
      else if (command == "DISARM_LASER")
      {
        laser.disarm();
      }

      // lick circuit commands
      else if (command == "ARM_LICK_CIRCUIT")
      {
        lickCircuit.arm();
      }
      else if (command == "DISARM_LICK_CIRCUIT")
      {
        lickCircuit.disarm();
      }

      else
      {
        Serial.println(">>> Command [" + command + "] is invalid.");
      }
      delay(50); // short delay to make sure that command goes through
    }
  }
}

// =======================================================
// ====================== SECTION 5 ======================
// =======================================================

void frameSignalISR()
{
  /*

     This is an interrupt protocol function that prioritizes timestamp signals over other logical processes ensuring all frames are collected.

     @function, captures timestamps for frame output and adjusts them to differenceFromStartTime

  */
  frameSignalReceived = true;
  frameSignalTimestamp = millis() - differenceFromStartTime; // capture the timestamp, adjusted by differenceFromStartTime
}
void handleFrameSignal()
{
  if (frameSignalReceived)
  {
    noInterrupts(); // temporarily disable interrupts to safely access shared variables
    frameSignalReceived = false;
    long int timestamp = frameSignalTimestamp;
    interrupts(); // re-enable interrupts
    Serial.println("FRAME_TIMESTAMP," + String(timestamp));
  }
}



// =======================================================
// ====================== SECTION 6 ======================
// =======================================================

void PROGRAM()
{
  /*

     Assembles desired functions to form program.

  */
  if (linkedToGUI)
  {
    monitorPressing(programIsRunning, activeLever, &cs, &pump);         // monitor pressing for active lever requires cs and pump parameter
    monitorPressing(programIsRunning, inactiveLever, nullptr, nullptr); // monitor pressing for inactive lever does not require cs or pump parameter
    monitorLicking(lickCircuit);
    manageLaser(laser);
    handleFrameSignal();
    pingDevice();
  }
}
