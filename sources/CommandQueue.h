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

#ifndef COMMANDQUEUE_H
#define COMMANDQUEUE_H

//CommandQueue.h

#include <vector>		//vector

#include "SimulatorObject.h"
#include "DRAMConfig.h"
#include "DRAMCommand.h"

#define ACCESSQUE(b) (QUE_PER_BANK==true ? queue[b] : queue[0])
#define POPCYCLE(b) (QUE_PER_BANK==true ? bufPopDelayPerBank[b] : bufPopDelayPerBank[0])
#define BANKSTATE(b) vaultContP->dramP->bankStates[b]

using namespace std;

namespace CasHMC
{
//forward declaration
class VaultController;
class CommandQueue : public SimulatorObject
{
public:
	//
	//Functions
	//
	CommandQueue(ofstream &debugOut_, ofstream &stateOut_, VaultController *parent, unsigned id);
	virtual ~CommandQueue();
	bool AvailableSpace(unsigned bank, unsigned cmdN);
	void Enqueue(unsigned bank, DRAMCommand *enqCMD);
	bool CmdPop(DRAMCommand **popedCMD);
	bool isIssuable(DRAMCommand *issueCMD);
	bool isEmpty();
	void Update();
	void PrintState();

	//
	//Fields
	//
	unsigned cmdQueID;
	unsigned issuedBank;
	bool refreshWaiting;
	VaultController *vaultContP;

	
	vector<bool> atomicLock;
	vector<unsigned> atomicLockTag;
	vector<int> bufPopDelayPerBank;
	vector< vector<DRAMCommand *> > queue;
	vector<unsigned> tFAWCountdown;
	vector<unsigned> rowAccessCounter;
};

}

#endif