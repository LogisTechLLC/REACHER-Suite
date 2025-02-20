#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h> 
#include "Cue.h"     
#include "Pump.h"    
#include "Laser.h"   
#include "Lever.h"   

void connectionJingle(String connected, Cue &cue, bool &linkedToGUI);
void pingDevice(unsigned long &previousPing, const unsigned long pingInterval);

extern bool programIsRunning;
extern long unsigned int differenceFromStartTime; 

void manageCue(Cue *cue);
void managePump(Pump *pump);
void manageStim(Laser &laser);
void monitorPressing(bool programRunning, Lever *&lever, Cue *cue, Pump *pump, Laser *laser);

#endif
