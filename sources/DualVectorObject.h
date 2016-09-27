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

#ifndef DUALVECTOROBJ_H
#define DUALVECTOROBJ_H

//DualVectorObject.h
//
//Header file for dual buffer object class
//

#include <vector>		//vector

#include "SimulatorObject.h"
#include "Transaction.h"
#include "Packet.h"

namespace CasHMC
{

template <typename DownT, typename UpT>
class DualVectorObject : public SimulatorObject
{
public:
	DualVectorObject(ofstream &debugOut_, ofstream &stateOut_, int downBufMax, int upBufMax):
			SimulatorObject(debugOut_, stateOut_), downBufferMax(downBufMax), upBufferMax(upBufMax) {
		bufPopDelay = 0;
		downBuffers.reserve(downBufferMax);
		upBuffers.reserve(upBufferMax);
	}
	~DualVectorObject() {
		downBuffers.clear(); 
		upBuffers.clear(); 
	}
	virtual void Update()=0;
	void Step() {
		currentClockCycle++;
		bufPopDelay = (bufPopDelay>0) ? bufPopDelay-1 : 0;
	}
	
	bool ReceiveDown(DownT *downEle) {
		if(downBuffers.size() + downEle->LNG <= downBufferMax) {
			if(downBuffers.size() == 0) {
				bufPopDelay = (bufPopDelay>0) ? bufPopDelay : 1;
			}
			downBuffers.push_back(downEle);
			//If receiving packet is packet, add virtual tail packet in Buffers as long as packet length
			if(sizeof(*downEle) == sizeof(Packet)) {
				for(int i=1; i<downEle->LNG; i++) {
					downBuffers.push_back(NULL);
				}
			}
			CallbackReceiveDown(downEle, true);
			return true;
		}
		else {
			CallbackReceiveDown(downEle, false);
			return false;
		}
	}
	bool ReceiveUp(UpT *upEle) {
		if(upBuffers.size() + upEle->LNG <= upBufferMax) {
			//Upstream buffer does not need upBufPopCycle (one cycle to pop one buffer data), 
			//	because upBufferDest (upstream buffer destination) is updated before buffer class.
			upBuffers.push_back(upEle);
			//If receiving packet is packet, add virtual tail packet in Buffers as long as packet length
			if(sizeof(*upEle) == sizeof(Packet)) {
				for(int i=1; i<upEle->LNG; i++) {
					upBuffers.push_back(NULL);
				}
			}
			CallbackReceiveUp(upEle, true);
			return true;
		}
		else {
			CallbackReceiveUp(upEle, false);
			return false;
		}
	}
	virtual void CallbackReceiveDown(DownT *downEle, bool chkReceive)=0;
	virtual void CallbackReceiveUp(UpT *upEle, bool chkReceive)=0;

	int downBufferMax;
	int upBufferMax;
	vector<DownT *> downBuffers;
	vector<UpT *> upBuffers;
	int bufPopDelay;
};

}

#endif