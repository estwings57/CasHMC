/*********************************************************************************
*  CasHMC v1.3 - 2017.07.10
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright 2016, Dong-Ik Jeon
*                  Ki-Seok Chung
*                  Hanyang University
*                  estwings57 [at] gmail [dot] com
*  All rights reserved.
*********************************************************************************/

#ifndef CROSSBARSWITCH_H
#define CROSSBARSWITCH_H

//CrossbarSwitch.h

#include <vector>		//vector

#include "DualVectorObject.h"
#include "ConfigValue.h"
#include "LinkMaster.h"

using namespace std;

namespace CasHMC
{
	
class CrossbarSwitch : public DualVectorObject<Packet, Packet>
{
public:
	//
	//Functions
	//
	CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_);
	virtual ~CrossbarSwitch();
	void CallbackReceiveDown(Packet *downEle, bool chkReceive);
	void CallbackReceiveUp(Packet *upEle, bool chkReceive);
	void Update();
	void PrintState();

	//
	//Fields
	//
	vector<DualVectorObject<Packet, Packet> *> downBufferDest;
	vector<LinkMaster *> upBufferDest;
	int inServiceLink;
	vector<unsigned> pendingSegTag;		//Store segment packet tag for returning
	vector<Packet *> pendingSegPacket;	//Store segment packets
};

}

#endif