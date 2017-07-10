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

#ifndef LINK_H
#define LINK_H

//Link.h

#include <stdint.h>		//uint64_t

#include "SingleVectorObject.h"
#include "ConfigValue.h"

using namespace std;

namespace CasHMC
{
//forward declaration
class LinkMaster;
class Link : public SimulatorObject
{
public:
	//
	//Functions
	//
	Link(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down, TranStatistic *statisP);
	virtual ~Link();
	void Update() {};
	void Update(bool lastUpdate);
	void UpdateStatistic(Packet *packet);
	void NoisePacket(Packet *packet);
	void PrintState();

	//
	//Fields
	//
	unsigned linkID;
	bool downstream;
	unsigned errorProba;
	LinkMaster *linkMasterP;
	TranStatistic *statis;
	SingleVectorObject<Packet> *linkSlaveP;
	
	//Currently transmitting packet through link
	Packet *inFlightPacket;
	unsigned inFlightCountdown;
};

}

#endif