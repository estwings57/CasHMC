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

#ifndef SINGLEVECTOROBJ_H
#define SINGLEVECTOROBJ_H

//SingleVectorObject.h
//
//Header file for single buffer object class
//

#include <vector>			//vector

#include "SimulatorObject.h"
#include "Transaction.h"
#include "Packet.h"

namespace CasHMC
{
enum LinkState
{
	NORMAL,
	START_RETRY,
	LINK_RETRY
};

template <typename BufT>
class SingleVectorObject : public SimulatorObject
{
public:
	SingleVectorObject(ofstream &debugOut_, ofstream &stateOut_, int bufMax, bool down):
			SimulatorObject(debugOut_, stateOut_), bufferMax(bufMax), downstream(down) {
		currentState = NORMAL;
		Buffers.reserve(bufferMax);
	}
	~SingleVectorObject() {
		Buffers.clear();
		linkRxTx.clear();
	}
	virtual void Update()=0;
	void Step() {
		currentClockCycle++;
		for(int i=0; i<Buffers.size(); i++) {
			if(Buffers[i] != NULL) {
				Buffers[i]->bufPopDelay = (Buffers[i]->bufPopDelay>0) ? Buffers[i]->bufPopDelay-1 : 0;
			}
		}
	}
	
	bool Receive(BufT *ele) {
		if(Buffers.size() + ele->LNG <= bufferMax) {
			Buffers.push_back(ele);
			//If receiving packet is packet, add virtual tail packet in Buffers as long as packet length
			if(sizeof(*ele) == sizeof(Packet)) {
				for(int i=1; i<ele->LNG; i++) {
					Buffers.push_back(NULL);
				}
			}
			CallbackReceive(ele, true);
			return true;
		}
		else {
			CallbackReceive(ele, false);
			return false;
		}
	}
	virtual void CallbackReceive(BufT *ele, bool chkReceive)=0;

	int bufferMax;
	bool downstream;
	vector<BufT *> Buffers;
	vector<BufT *> linkRxTx;
	LinkState currentState;
};

}

#endif