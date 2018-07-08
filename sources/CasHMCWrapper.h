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

#ifndef CASHMCWRAPPER_H
#define CASHMCWRAPPER_H

//CasHMCWrapper.h

#include <time.h>		//time
#include <sys/stat.h>	//mkdir
#include <errno.h>		//EEXIST
#include <unistd.h>		//access()
#include <sstream>		//stringstream
#include <fstream>		//ofstream
#include <vector>		//vector

#include "ConfigReader.h"
#include "ConfigValue.h"
#include "TranStatistic.h"
#include "CallBack.h"
#include "HMCController.h"
#include "Link.h"
#include "HMC.h"

using namespace std;

namespace CasHMC
{

class CasHMCWrapper : public TranStatistic
{
public:
	//
	//Functions
	//
	CasHMCWrapper(string simCfg, string dramCfg);
	virtual ~CasHMCWrapper();
	void RegisterCallbacks(TransCompCB *readCB, TransCompCB *writeCB);
	bool ReceiveTran(TransactionType tranType, uint64_t addr, unsigned size);
	bool ReceiveTran(Transaction *tran);
	bool CanAcceptTran();
	void UpdateMSHR(unsigned mshr);
	void Update();
	void DownLinkUpdate(bool lastUpdate);
	void UpLinkUpdate(bool lastUpdate);
	void PrintEpochHeader();
	void PrintSetting(struct tm t);
	void MakePlotData();
	void PrintEpochStatistic();
	void PrintFinalStatistic();
	string DataScaling(double dataScale);
	
	//
	//Fields
	//
	HMCController *hmcCont;
	vector<Link *> downstreamLinks;
	vector<Link *> upstreamLinks;
	HMC *hmc;
	
	uint64_t currentClockCycle;
	uint64_t dramTuner;
	uint64_t downLinkTuner;
	uint64_t downLinkClock;
	uint64_t upLinkTuner;
	uint64_t upLinkClock;
	double linkPeriod;
	string logName;
	int logNum;
	
	unsigned cpu_link_ratio;
	unsigned cpu_link_tune;
	unsigned clockTuner_link;
	unsigned clockTuner_HMC;
	
	//Temporary variable for plot data
	uint64_t hmcTransmitSizeTemp;
	vector<uint64_t> downLinkDataSizeTemp;
	vector<uint64_t> upLinkDataSizeTemp;
	
	//Output log files
	ofstream settingOut;
	ofstream debugOut;
	ofstream stateOut;
	ofstream plotDataOut;
	ofstream plotScriptOut;
	ofstream resultOut;
};

}

#endif