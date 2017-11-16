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

#ifndef HMCCONTROLLER_H
#define HMCCONTROLLER_H

//HMCController.h

#include <vector>		//vector

#include "DualVectorObject.h"
#include "ConfigValue.h"
#include "CallBack.h"
#include "LinkMaster.h"
#include "LinkSlave.h"

using namespace std;

namespace CasHMC
{

class HMCController : public DualVectorObject<Transaction, Packet>
{
public:
	//
	//Functions
	//
	HMCController(ofstream &debugOut_, ofstream &stateOut_);
	virtual ~HMCController();
	void RegisterCallbacks(TransCompCB *readCB, TransCompCB *writeCB);
	void CallbackReceiveDown(Transaction *downEle, bool chkReceive);
	void CallbackReceiveUp(Packet *upEle, bool chkReceive);
	bool CanAcceptTran();
	void Update();
	Packet *ConvTranIntoPacket(Transaction *tran);
	void LinkPowerEntryManager();
	void LinkPowerStateManager();
	void LinkPowerExitManager();
	void UpdateMSHR(unsigned mshr);
	void LinkStatistic();
	void PrintState();
	
	//
	//Fields
	//
	vector<LinkMaster *> downLinkMasters;
	vector<LinkSlave *> upLinkSlaves;
	int inServiceLink;
	
	unsigned maxLinkBand;
	unsigned linkEpochCycle;
	uint64_t requestAccLNG;
	uint64_t responseAccLNG;
	unsigned sleepLink;
	unsigned alloMSHR;
	uint64_t accuMSHR;
	
	unsigned returnTransCnt;
	unsigned quiesceClk;
	uint64_t staggerSleep;
	uint64_t staggerActive;
	uint64_t staySREF;
	vector<uint64_t> stayActive;
	vector<uint64_t> quiesceLink;
	vector<uint64_t> modeTransition;
	vector<bool> staggeringSleep;	
	vector<bool> staggeringActive;	
	
	vector<uint64_t> linkSleepTime;
	vector<uint64_t> linkDownTime;
	
	TransCompCB *readDone;
	TransCompCB *writeDone;
};

}

#endif