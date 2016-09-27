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

#ifndef HMCCONTROLLER_H
#define HMCCONTROLLER_H

//HMCController.h

#include <vector>		//vector

#include "DualVectorObject.h"
#include "SimConfig.h"
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
	void CallbackReceiveDown(Transaction *downEle, bool chkReceive);
	void CallbackReceiveUp(Packet *upEle, bool chkReceive);
	void Update();
	Packet *ConvTranIntoPacket(Transaction *tran);
	void PrintState();
	
	//
	//Fields
	//
	vector<LinkMaster *> downLinkMasters;
	vector<LinkSlave *> upLinkSlaves;
	int inServiceLink;	
};

}

#endif