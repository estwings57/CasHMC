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

#ifndef TRANSTATISTIC_H
#define TRANSTATISTIC_H

//TranStatistic.h

#include <vector>		//vector
#include <iostream> 	//ostream
#include <stdint.h>		//uint64_t

#include "ConfigValue.h"

using namespace std;

namespace CasHMC
{
	
class TranStatistic
{
public:
	TranStatistic() {
		readPerLink = vector<uint64_t>(1, 0);
		writePerLink = vector<uint64_t>(1, 0);
		atomicPerLink = vector<uint64_t>(1, 0);
		reqPerLink = vector<uint64_t>(1, 0);
		resPerLink = vector<uint64_t>(1, 0);
		flowPerLink = vector<uint64_t>(1, 0);
		errorPerLink = vector<unsigned>(1, 0);
		retryFailPerLink = vector<unsigned>(1, 0);
		
		hmcTransmitSize = 0;
		downLinkTransmitSize = vector<uint64_t>(1, 0);
		upLinkTransmitSize = vector<uint64_t>(1, 0);
		downLinkDataSize = vector<uint64_t>(1, 0);
		upLinkDataSize = vector<uint64_t>(1, 0);
		
		tranFullSum = 0;	linkFullSum = 0;	vaultFullSum = 0;	errorRetrySum = 0;
		totalTranCount = 0;		totalErrorCount = 0;
		totalTranFullSum = 0;	totalLinkFullSum = 0;	totalVaultFullSum = 0;	totalErrorRetrySum = 0;
		
		totalReadPerLink = vector<uint64_t>(1, 0);
		totalWritePerLink = vector<uint64_t>(1, 0);
		totalAtomicPerLink = vector<uint64_t>(1, 0);
		totalReqPerLink = vector<uint64_t>(1, 0);
		totalResPerLink = vector<uint64_t>(1, 0);
		totalFlowPerLink = vector<uint64_t>(1, 0);
		totalErrorPerLink = vector<unsigned>(1, 0);
		
		totalTranFullMax = 0;	totalTranFullMin = -1;
		totalLinkFullMax = 0;	totalLinkFullMin = -1;
		totalVaultFullMax = 0;	totalVaultFullMin = -1;
		totalErrorRetryMax = 0;	totalErrorRetryMin = -1;
		totalTranStdSum = 0;	totalLinkStdSum = 0;	totalVaultStdSum = 0;	totalErrorStdSum = 0;
		
		totalHmcTransmitSize = 0;
		totalDownLinkTransmitSize = vector<uint64_t>(1, 0);
		totalUpLinkTransmitSize = vector<uint64_t>(1, 0);
		totalDownLinkDataSize = vector<uint64_t>(1, 0);
		totalUpLinkDataSize = vector<uint64_t>(1, 0);
	}
	virtual ~TranStatistic() {
		tranFullLat.clear();
		linkFullLat.clear();
		vaultFullLat.clear();
		errorRetryLat.clear();
	}
	void PushStatisPerLink() {
		for(int i=0; i<NUM_LINKS; i++) {
			readPerLink.push_back(0);
			writePerLink.push_back(0);
			atomicPerLink.push_back(0);
			reqPerLink.push_back(0);
			resPerLink.push_back(0);
			flowPerLink.push_back(0);
			errorPerLink.push_back(0);
			retryFailPerLink.push_back(0);
			
			downLinkTransmitSize.push_back(0);
			upLinkTransmitSize.push_back(0);
			downLinkDataSize.push_back(0);
			upLinkDataSize.push_back(0);
			
			totalReadPerLink.push_back(0);
			totalWritePerLink.push_back(0);
			totalAtomicPerLink.push_back(0);
			totalReqPerLink.push_back(0);
			totalResPerLink.push_back(0);
			totalFlowPerLink.push_back(0);
			totalErrorPerLink.push_back(0);

			totalDownLinkTransmitSize.push_back(0);
			totalUpLinkTransmitSize.push_back(0);
			totalDownLinkDataSize.push_back(0);
			totalUpLinkDataSize.push_back(0);
		}		
	}
	void UpdateStatis(unsigned tranFull, unsigned linkFull, unsigned vaultFull) {
		tranFullLat.push_back(tranFull);
		linkFullLat.push_back(linkFull);
		vaultFullLat.push_back(vaultFull);
		
		tranFullSum += tranFull;
		linkFullSum += linkFull;
		vaultFullSum += vaultFull;
	}
	
	//Bookkeeping and statistics
	vector<unsigned> tranFullLat;
	vector<unsigned> linkFullLat;
	vector<unsigned> vaultFullLat;
	vector<unsigned> errorRetryLat;
	
	vector<uint64_t> readPerLink;
	vector<uint64_t> writePerLink;
	vector<uint64_t> atomicPerLink;
	vector<uint64_t> reqPerLink;
	vector<uint64_t> resPerLink;
	vector<uint64_t> flowPerLink;
	vector<unsigned> errorPerLink;
	vector<unsigned> retryFailPerLink;
	
	uint64_t hmcTransmitSize;				//[Byte]
	vector<uint64_t> downLinkTransmitSize;	//[Byte]
	vector<uint64_t> upLinkTransmitSize;	//[Byte]
	vector<uint64_t> downLinkDataSize;		//[Byte]
	vector<uint64_t> upLinkDataSize;		//[Byte]
	
	uint64_t tranFullSum;
	uint64_t linkFullSum;
	uint64_t vaultFullSum;
	uint64_t errorRetrySum;
	
	//Accumulate statistics
	uint64_t totalTranCount;
	uint64_t totalErrorCount;
	uint64_t totalTranFullSum;
	uint64_t totalLinkFullSum;
	uint64_t totalVaultFullSum;
	uint64_t totalErrorRetrySum;
	
	vector<uint64_t> totalReadPerLink;
	vector<uint64_t> totalWritePerLink;
	vector<uint64_t> totalAtomicPerLink;
	vector<uint64_t> totalReqPerLink;
	vector<uint64_t> totalResPerLink;
	vector<uint64_t> totalFlowPerLink;
	vector<unsigned> totalErrorPerLink;
	
	unsigned totalTranFullMax;
	unsigned totalTranFullMin;
	unsigned totalLinkFullMax;
	unsigned totalLinkFullMin;
	unsigned totalVaultFullMax;
	unsigned totalVaultFullMin;
	unsigned totalErrorRetryMax;
	unsigned totalErrorRetryMin;
	
	double totalTranStdSum;
	double totalLinkStdSum;
	double totalVaultStdSum;
	double totalErrorStdSum;
	
	uint64_t totalHmcTransmitSize;
	vector<uint64_t> totalDownLinkTransmitSize;
	vector<uint64_t> totalUpLinkTransmitSize;
	vector<uint64_t> totalDownLinkDataSize;
	vector<uint64_t> totalUpLinkDataSize;
};

}

#endif