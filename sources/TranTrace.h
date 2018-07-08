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

#ifndef TRANTRACE_H
#define TRANTRACE_H

//TranTrace.h

#include <stdint.h>		//uint64_t

#include "TranStatistic.h"

using namespace std;

namespace CasHMC
{

class TranTrace
{
public:
	TranTrace(TranStatistic *sta):statis(sta) {
		tranTransmitTime = 0;
		tranFullLat = 0;
		linkTransmitTime = 0;
		linkFullLat = 0;
		vaultIssueTime = 0;
		vaultFullLat = 0;
	}
	~TranTrace() {
		if(tranFullLat==0 || linkFullLat==0 || vaultFullLat==0) {
			ERROR(" (Trace) == Error - Full latency is '0'  (tranFullLat : "<<tranFullLat
					<<", linkFullLat : "<<linkFullLat<<", vaultFullLat : "<<vaultFullLat<<")");
			exit(0);
		}
		else {
			statis->UpdateStatis(tranFullLat, linkFullLat, vaultFullLat);
		}
	}
	
	//Identifier
	TranStatistic *statis;
	
	//Trace latency info
	unsigned tranTransmitTime;	//[CPU clock] Time to send transaction to HMC controller from bus
	unsigned tranFullLat;		//[CPU clock] Total latency time flowing transaction
	
	unsigned linkTransmitTime;	//[CPU clock] Time to send request packet through link
	unsigned linkFullLat;		//[CPU clock] Total latency time from sending request to returning response packet through link
	
	unsigned vaultIssueTime; 	//[DRAM clock (tCK)] Time to issue ACTIVATE command corresponding to this transaction
	unsigned vaultFullLat; 		//[DRAM clock (tCK)] Total latency time from issue ACTIVATE command to return data
};

}

#endif