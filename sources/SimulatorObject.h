/*********************************************************************************
*  CasHMC v1.2 - 2016.09.27
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright (c) 2016, Dong-Ik Jeon
*                      Ki-Seok Chung
*                      Hanyang University
*                      estwings57 [at] gmail [dot] com
*  All rights reserved.
*********************************************************************************/

#ifndef SIMULATOROBJ_H
#define SIMULATOROBJ_H

//SimulatorObject.h
//
//Header file for simulator object class
//

#include <stdint.h>		//uint64_t
#include <fstream>		//ofstream
#include <iomanip>		//setw()
#include <sstream>		//stringstream

using namespace std;

namespace CasHMC
{
	
class SimulatorObject
{
public:
	SimulatorObject(ofstream &debugOut_, ofstream &stateOut_):debugOut(debugOut_), stateOut(stateOut_) {
		currentClockCycle = 0;
	}
	virtual void Update()=0;
	void Step() {
		currentClockCycle++;
	}
	
	uint64_t currentClockCycle;

	//Output log files
	ofstream &debugOut;
	ofstream &stateOut;
	string header;
	stringstream classID;
	
};

}

#endif