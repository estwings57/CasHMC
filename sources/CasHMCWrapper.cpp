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

#include "CasHMCWrapper.h"

using namespace std;

long numSimCycles;
double memUtil;
double rwRatio;
string traceType = "lib";
string traceFileName = "";

namespace CasHMC
{
	
CasHMCWrapper::CasHMCWrapper(string simCfg, string dramCfg)
{
	//Loading configure parameters
	ReadIniFile(simCfg);
	ReadIniFile(dramCfg);
	PushStatisPerLink();
	
	//
	//Class variable initialization
	//
	currentClockCycle = 0;
	dramTuner = 1;
	downLinkTuner = 1;
	downLinkClock = 1;
	upLinkTuner = 1;
	upLinkClock = 1;
	linkPeriod = (1/LINK_SPEED);
	
	//Make class objects
	downstreamLinks.reserve(NUM_LINKS);
	upstreamLinks.reserve(NUM_LINKS);
	for(int l=0; l<NUM_LINKS; l++) {
		downstreamLinks.push_back(new Link(debugOut, stateOut, l, true, this));
		upstreamLinks.push_back(new Link(debugOut, stateOut, l, false, this));
	}
	hmcCont = new HMCController(debugOut, stateOut);
	hmc = new HMC(debugOut, stateOut);
	
	//Link master, Link, and Link slave are linked each other by respective lanes
	for(int l=0; l<NUM_LINKS; l++) {
		//Downstream
		hmcCont->downLinkMasters[l]->linkP = downstreamLinks[l];
		hmcCont->downLinkMasters[l]->localLinkSlave = hmcCont->upLinkSlaves[l];
		downstreamLinks[l]->linkMasterP = hmcCont->downLinkMasters[l];
		downstreamLinks[l]->linkSlaveP = hmc->downLinkSlaves[l];
		hmc->downLinkSlaves[l]->downBufferDest = hmc->crossbarSwitch;
		hmc->downLinkSlaves[l]->localLinkMaster = hmc->upLinkMasters[l];
		//Upstream
		hmc->crossbarSwitch->upBufferDest[l] = hmc->upLinkMasters[l];
		hmc->upLinkMasters[l]->linkP = upstreamLinks[l];
		hmc->upLinkMasters[l]->localLinkSlave = hmc->downLinkSlaves[l];
		upstreamLinks[l]->linkMasterP = hmc->upLinkMasters[l];
		upstreamLinks[l]->linkSlaveP = hmcCont->upLinkSlaves[l];
		hmcCont->upLinkSlaves[l]->upBufferDest = hmcCont;
		hmcCont->upLinkSlaves[l]->localLinkMaster = hmcCont->downLinkMasters[l];
	}

	//Check CPU clock cycle and link speed
	if(CPU_CLK_PERIOD < linkPeriod) {	//Check CPU clock cycle and link speed
		ERROR(" == Error - WRONG CPU clock cycle or link speed (CPU_CLK_PERIOD should be bigger than (1/LINK_SPEED))");
		ERROR(" == Error - CPU_CLK_PERIOD : "<<CPU_CLK_PERIOD<<"  1/LINK_SPEED : "<<linkPeriod);
		exit(0);
	}
	
	//
	// Log files generation
	//
	int status_rst = 1;
	int status_grp = 1;
	status_rst = mkdir("result", S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
	status_grp = mkdir("graph", S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
	
	time_t now;
    struct tm t;
    time(&now);
    t = *localtime(&now);
	
	cout<<endl<<"****************************************************************"<<endl;
	cout<<"*                      CasHMC version 1.3                      *"<<endl;
	cout<<"*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
			<<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
			<<"            *"<<endl;
#ifndef DEBUG_LOG		
	cout<<"*                                                              *"<<endl;
	cout<<"*  Module abbreviation info.                                   *"<<endl;
	cout<<"*  [HC]   : HMC Controller                                     *"<<endl;
	cout<<"*  [LM_U/Di] : ith Up/Downstream Link Master                   *"<<endl;
	cout<<"*  [LK_U/Di] : ith Up/Downstream Link                          *"<<endl;
	cout<<"*  [LS_U/Di] : ith Up/Downstream Link Slave                    *"<<endl;
	cout<<"*  [CS]   : Crossbar Switch                                    *"<<endl;
	cout<<"*  [VC_j] : jth Vault Controller                               *"<<endl;
	cout<<"*  [CQ_k] : k-bank Command Queue                               *"<<endl;
	cout<<"*  [DR_j] : jth DRAM                                           *"<<endl;
#endif		
	cout<<"****************************************************************"<<endl<<endl;
	cout.setf(ios::left); 
	cout<<" = Log folder generating information"<<endl;
	if(status_rst == 0) {
		cout<<"   Making result folder success (./result/)"<<endl;
	}
	else {
		if(errno == EEXIST) {
			cout<<"   Result folder already exists (./result/)"<<endl;
			status_rst = 0;
		}
		else {
			cout<<"   Making result folder fail"<<endl;
			cout<<"   Debug and result files will be generated in CasHMC folder (./)"<<endl<<endl;
		}
	}
	if(status_grp == 0) {
		cout<<"   Making graph folder success (./graph/)"<<endl;
	}
	else {
		if(errno == EEXIST) {
			cout<<"   Graph folder already exists (./graph/)"<<endl;
			status_grp = 0;
		}
		else {
			cout<<"   Making graph folder fail"<<endl;
			cout<<"   graph files will be generated in CasHMC folder (./)"<<endl<<endl;
		}
	}
	
	if(status_rst == 0)
		logName = "result/CasHMC";
	else
		logName = "CasHMC";
	
	string plotName;
	if(status_grp == 0)
		plotName = "graph/CasHMC_plot_no";
	else
		plotName = "CasHMC_plot_no";

	unsigned int ver_num = 0;
	stringstream temp_vn;
	while(1) { 
		logName += "_no";
		temp_vn << ver_num;
		logName += temp_vn.str();
		logName += "_setting.log";
		
		if(access(logName.c_str(), 0) == -1)	break;
		else {
			logName.erase(logName.find("_no"));
			temp_vn.str( string() );
			temp_vn.clear();
			ver_num++;
		}
	}
	cout.setf(ios::left);
	cout<<endl<<"   === Simulation start === "<<endl;
	
	if(BANDWIDTH_PLOT) {
		hmcTransmitSizeTemp = 0;
		downLinkDataSizeTemp = vector<uint64_t>(NUM_LINKS, 0);
		upLinkDataSizeTemp = vector<uint64_t>(NUM_LINKS, 0);
		
		//Plot data file initialization
		plotName += temp_vn.str();
		plotName += ".dat";
		plotDataOut.open(plotName.c_str());
		plotDataOut.setf(ios::left);
		cout<<"  [ "<<plotName<<" ] is generated"<<endl;
		
		plotDataOut<<setw(8)<<setfill(' ')<<"#Epoch";
		for(int i=0; i<NUM_LINKS; i++) {
			stringstream linkNum;
			linkNum << i;
			string plotLink = "Link[" + linkNum.str() + ']';
			plotDataOut<<setw(10)<<setfill(' ')<<plotLink;
		}
		plotDataOut<<setw(10)<<setfill(' ')<<"HMC bandwidth"<<endl;
		
		//Plot script file initialization
		plotName.erase(plotName.find(".dat"));
		plotName += ".gnuplot";
		plotScriptOut.open(plotName.c_str());
		cout<<"  [ "<<plotName<<" ] is generated"<<endl;
		
		plotName = "CasHMC_plot_no";
		plotName += temp_vn.str();
		plotName += ".png";
		
		plotScriptOut<<"set term png size 800, 500 font \"Times-Roman,\""<<endl;
		plotScriptOut<<"set output '"<<plotName<<"'"<<endl;
		plotScriptOut<<"set title \"CasHMC bandwidth graph\" font \",18\""<<endl;
		plotScriptOut<<"set autoscale"<<endl;
		plotScriptOut<<"set grid"<<endl;
		plotScriptOut<<"set xlabel \"Simulation plot epoch\""<<endl;
		plotScriptOut<<"set ylabel \"Bandwidth [GB/s]\""<<endl;
		plotScriptOut<<"set key left box"<<endl;
		plotScriptOut<<"plot  \"CasHMC_plot_no"<<ver_num<<".dat\" using 1:"<<NUM_LINKS+2<<" title 'HMC' with lines lw 2 , \\"<<endl;
		for(int i=0; i<NUM_LINKS; i++) {
			plotScriptOut<<"      \"CasHMC_plot_no"<<ver_num<<".dat\" using 1:"<<i+2<<" title 'Link["<<i<<"]' with lines lw 1 , \\"<<endl;
		}
	}
	
	settingOut.open(logName.c_str());
	PrintSetting(t);
	cout<<"  [ "<<logName<<" ] is generated"<<endl;
	
	logName.erase(logName.find("_s"));
	logNum = 0;
	PrintEpochHeader();
}

CasHMCWrapper::~CasHMCWrapper()
{
	PrintEpochStatistic();
	PrintFinalStatistic();
	debugOut.flush();		debugOut.close();
	stateOut.flush();		stateOut.close();
	plotDataOut.flush();	plotDataOut.close();
	plotScriptOut.flush();	plotScriptOut.close();
	resultOut.flush();		resultOut.close();
	
	for(int l=0; l<NUM_LINKS; l++) {
		delete downstreamLinks[l];
		delete upstreamLinks[l];
	}
	downstreamLinks.clear(); 
	upstreamLinks.clear(); 
	delete hmcCont;
	hmcCont = NULL;
	delete hmc;
	hmc = NULL;
}

//
//Register read and write callback funtions
//
void CasHMCWrapper::RegisterCallbacks(TransCompCB *readCB, TransCompCB *writeCB)
{
	hmcCont->RegisterCallbacks(readCB, writeCB);
}

//
//Check buffer available space and receive transaction
//
bool CasHMCWrapper::ReceiveTran(TransactionType tranType, uint64_t addr, unsigned size)
{
	Transaction *newTran = new Transaction(tranType, addr, size, this);

	if(hmcCont->ReceiveDown(newTran)) {
		DE_CR(ALI(18)<<" (BUS)"<<ALI(15)<<*newTran<<"Down) SENDING transaction to HMC controller (HC)");
		return true;
	}
	else {
		newTran->ReductGlobalID();
		delete newTran->trace;
		delete newTran;
		return false;
	}
}

bool CasHMCWrapper::ReceiveTran(Transaction *tran)
{
	if(hmcCont->ReceiveDown(tran)) {
		DE_CR(ALI(18)<<" (BUS)"<<ALI(15)<<*tran<<"Down) SENDING transaction to HMC controller (HC)");
		return true;
	}
	else {
		return false;
	}
}

bool CasHMCWrapper::CanAcceptTran(void)
{
	return hmcCont->CanAcceptTran();
}

//
//Update the allocated MSHR size (It's for MSHR and AUTONOMOUS link power management)
//
void CasHMCWrapper::UpdateMSHR(unsigned mshr)
{
	hmcCont->UpdateMSHR(mshr);;
}

//
//Updates the state of HMC Wrapper
//
void CasHMCWrapper::Update()
{
	if(BANDWIDTH_PLOT && currentClockCycle > 0 && currentClockCycle%PLOT_SAMPLING == 0) {
		MakePlotData();
	}
	
#ifdef DEBUG_LOG
	if(currentClockCycle > 0 && currentClockCycle%LOG_EPOCH == 0) {
		cout<<"\n   === Simulation ["<<currentClockCycle/LOG_EPOCH<<"] epoch starts  ( CPU clk:"<<currentClockCycle<<" ) ===   "<<endl;
		PrintEpochStatistic();
		if(DEBUG_SIM) {
			debugOut.flush();	debugOut.close();
		}
		if(STATE_SIM) {
			stateOut.flush();	stateOut.close();
		}
		if(DEBUG_SIM || STATE_SIM) {
			PrintEpochHeader();
		}
	}
#endif

	//
	//update all class (HMC controller, links, and HMC block)
	//Roughly synchronize CPU clock cycle, link speed, and HMC clock cycle
	//
	hmcCont->Update();
	//Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
	for(int l=0; l<NUM_LINKS; l++) {
		hmcCont->downLinkMasters[l]->Update();
	}
	
		//Downstream links update at CPU clock cycle (depending on ratio of CPU cycle to Link cycle)
		while(CPU_CLK_PERIOD*downLinkTuner > linkPeriod*(downLinkClock + 1)) {
			DownLinkUpdate(false);
		}
		if(CPU_CLK_PERIOD*downLinkTuner == linkPeriod*(downLinkClock + 1)) {
			DownLinkUpdate(false);
			downLinkTuner = 0;
			downLinkClock = 0;
		}
		DownLinkUpdate(true);
	
			//HMC update at CPU clock cycle
			if(CPU_CLK_PERIOD <= tCK) {
				if(CPU_CLK_PERIOD*dramTuner > tCK*hmc->clockTuner) {
					hmc->Update();
				}
				else if(CPU_CLK_PERIOD*dramTuner == tCK*hmc->clockTuner) {
					dramTuner = 0;
					hmc->clockTuner = 0;
					hmc->Update();
				}
			}
			else {
				while(CPU_CLK_PERIOD*dramTuner > tCK*(hmc->clockTuner + 1)) {
					hmc->Update();
				}
				if(CPU_CLK_PERIOD*dramTuner == tCK*(hmc->clockTuner + 1)) {
					hmc->Update();
					dramTuner = 0;
					hmc->clockTuner = 0;
				}
				hmc->Update();
			}
		
		//Upstream links update at CPU clock cycle (depending on ratio of CPU cycle to Link cycle)
		while(CPU_CLK_PERIOD*upLinkTuner > linkPeriod*(upLinkClock + 1)) {
			UpLinkUpdate(false);
		}
		if(CPU_CLK_PERIOD*upLinkTuner == linkPeriod*(upLinkClock + 1)) {
			UpLinkUpdate(false);
			upLinkTuner = 0;
			upLinkClock = 0;
		}
		UpLinkUpdate(true);
	
	//Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
	for(int l=0; l<NUM_LINKS; l++) {
		hmcCont->upLinkSlaves[l]->Update();
	}
	
#ifndef DEBUG_LOG
	cout<<endl;
#endif
	//Print respective class state
	hmcCont->PrintState();
	for(int l=0; l<NUM_LINKS; l++) {
		hmcCont->downLinkMasters[l]->PrintState();
	}
		for(int l=0; l<NUM_LINKS; l++) {
			downstreamLinks[l]->PrintState();
		}
			hmc->PrintState();
		for(int l=0; l<NUM_LINKS; l++) {
			upstreamLinks[l]->PrintState();
		}
	for(int l=0; l<NUM_LINKS; l++) {
		hmcCont->upLinkSlaves[l]->PrintState();
	}
	
	currentClockCycle++;
	downLinkTuner++;
	upLinkTuner++;
	dramTuner++;
	DE_ST("\n---------------------------------------[ CPU clk:"<<currentClockCycle<<" / HMC clk:"<<hmc->currentClockCycle<<" ]---------------------------------------");
}

//
//Update links
//
void CasHMCWrapper::DownLinkUpdate(bool lastUpdate)
{
	for(int l=0; l<NUM_LINKS; l++) {
		downstreamLinks[l]->Update(lastUpdate);
	}
	downLinkClock++;
}

void CasHMCWrapper::UpLinkUpdate(bool lastUpdate)
{
	for(int l=0; l<NUM_LINKS; l++) {
		upstreamLinks[l]->Update(lastUpdate);
	}
	upLinkClock++;
}

//
//Print simulation debug and state every epoch
//
void CasHMCWrapper::PrintEpochHeader()
{
#ifdef DEBUG_LOG
	time_t now;
    struct tm t;
    time(&now);
    t = *localtime(&now);
	
	stringstream temp_vn;
	if(DEBUG_SIM) {
		string debName = logName + "_debug";
		while(1) { 
			debName += "[";
			temp_vn << logNum;
			debName += temp_vn.str();
			debName += "].log";
			if(access(debName.c_str(), 0)==-1)		break;
			else {
				debName.erase(debName.find("["));
				logNum++;
				temp_vn.str( string() );
				temp_vn.clear();
			}
		}
		
		debugOut.open(debName.c_str());
		debugOut.setf(ios::left);
		cout<<"  [ "<<debName<<" ] is generated"<<endl;
	}
	
	if(STATE_SIM) {
		string staName = logName + "_state";
		if(!DEBUG_SIM) {
			stringstream temp_vn_st;
			while(1) { 
				staName += "[";
				temp_vn_st << logNum;
				staName += temp_vn_st.str();
				staName += "].log";
				if(access(staName.c_str(), 0)==-1)		break;
				else {
					staName.erase(staName.find("["));
					logNum++;
					temp_vn_st.str( string() );
					temp_vn_st.clear();
				}
			}
		}
		else {
			staName += "[";
			temp_vn.str( string() );
			temp_vn.clear();
			temp_vn << logNum;
			staName += temp_vn.str();
			staName += "].log";
		}
		
		stateOut.open(staName.c_str());
		stateOut.setf(ios::left);
		cout<<"  [ "<<staName<<" ] is generated"<<endl;
	}
	cout<<endl;
	
	//
	//Simulation header generation
	//
	DE_ST("****************************************************************");
	DE_ST("*                      CasHMC version 1.3                      *");
	DEBUG("*                    simDebug log file ["<<logNum<<"]                     *");
	STATE("*                      state log file ["<<logNum<<"]                      *");
	DE_ST("*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
			<<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
			<<"            *");
	DE_ST("*                                                              *");
	DE_ST("*  Module abbreviation info.                                   *");
	DE_ST("*  [HC]   : HMC Controller                                     *");
	DE_ST("*  [LM_U/Di] : ith Up/Downstream Link Master                   *");
	DE_ST("*  [LK_U/Di] : ith Up/Downstream Link                          *");
	DE_ST("*  [LS_U/Di] : ith Up/Downstream Link Slave                    *");
	DE_ST("*  [CS]     : Crossbar Switch                                  *");
	DE_ST("*  [VC_j]   : jth Vault Controller                             *");
	DE_ST("*  [CQ_j-k] : jth k-bank Command Queue                         *");
	DE_ST("*  [DR_j]   : jth DRAM                                         *");
	STATE("*                                                              *");
	STATE("*  # RETRY (packet1 index)[packet1](packet2 index)[packet2]    *");
	DE_ST("****************************************************************");
	DE_ST(endl<<"- Trace type : "<<traceType);
	if(traceType == "random") {
		DE_ST("- Frequency of requests : "<<memUtil);
		DE_ST("- The percentage of reads [%] : "<<rwRatio);
	}
	else if(traceType == "file") {
		DE_ST("- Trace file : "<<traceFileName);
	}
#endif
	DE_ST(endl<<"---------------------------------------[ CPU clk:"<<currentClockCycle<<" / HMC clk:"<<hmc->currentClockCycle<<" ]---------------------------------------");
}

//
//Print simulation setting info
//
void CasHMCWrapper::PrintSetting(struct tm t)
{
	settingOut<<"****************************************************************"<<endl;
	settingOut<<"*                      CasHMC version 1.3                      *"<<endl;
	settingOut<<"*                       setting log file                       *"<<endl;
	settingOut<<"*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
			<<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
			<<"            *"<<endl;
	settingOut<<"****************************************************************"<<endl;
	
	settingOut<<endl<<"        ==== Memory transaction setting ===="<<endl;
	settingOut<<ALI(36)<<" CPU cycles to be simulated : "<<numSimCycles<<endl;
	settingOut<<ALI(36)<<" CPU clock period [ns] : "<<CPU_CLK_PERIOD<<endl;
	settingOut<<ALI(36)<<" Data size of DRAM request [byte] : "<<TRANSACTION_SIZE<<endl;
	settingOut<<ALI(36)<<" Request buffer max size : "<<MAX_REQ_BUF<<endl;
	settingOut<<ALI(36)<<" Trace type : "<<traceType<<endl;
	if(traceType == "random") {
		settingOut<<ALI(36)<<" Frequency of requests : "<<memUtil<<endl;
		settingOut<<ALI(36)<<" The percentage of reads [%] : "<<rwRatio<<endl;
	}
	else if(traceType == "file") {
		settingOut<<ALI(36)<<" Trace file name : "<<traceFileName<<endl;
	}

	settingOut<<endl<<"              ==== Link(SerDes) setting ===="<<endl;
	settingOut<<ALI(36)<<" CRC checking : "<<(CRC_CHECK ? "Enable" : "Disable")<<endl;
	settingOut<<ALI(36)<<" The number of links : "<<NUM_LINKS<<endl;
	settingOut<<ALI(36)<<" Link width : "<<LINK_WIDTH<<endl;
	settingOut<<ALI(36)<<" Link speed [Gb/s] : "<<LINK_SPEED<<endl;
	settingOut<<ALI(36)<<" Link master/slave max buffer size: "<<MAX_LINK_BUF<<endl;
	settingOut<<ALI(36)<<" Crossbar switch buffer size : "<<MAX_CROSS_BUF<<endl;
	settingOut<<ALI(36)<<" Retry buffer max size : "<<MAX_RETRY_BUF<<endl;
	settingOut<<ALI(36)<<" Time to calculate CRC [clk] : "<<CRC_CAL_CYCLE<<endl;
	settingOut<<ALI(36)<<" The number of IRTRY packet : "<<NUM_OF_IRTRY<<endl;
	settingOut<<ALI(36)<<" Retry attempt limit : "<<RETRY_ATTEMPT_LIMIT<<endl;
	settingOut<<ALI(36)<<" Link BER (the power of 10) : "<<LINK_BER<<endl;
	switch(LINK_PRIORITY) {
	case ROUND_ROBIN:
		settingOut<<ALI(36)<<" Link priority scheme : "<<"ROUND_ROBIN"<<endl;
		break;
	case BUFFER_AWARE:
		settingOut<<ALI(36)<<" Link priority scheme : "<<"BUFFER_AWARE"<<endl;
		break;
	default:
		settingOut<<ALI(36)<<" Link priority scheme : "<<LINK_PRIORITY<<endl;
		break;
	}
	switch(LINK_POWER) {
	case NO_MANAGEMENT:
		settingOut<<ALI(36)<<" Link power management : "<<"NO_MANAGEMENT"<<endl;
		break;
	case QUIESCE_SLEEP:
		settingOut<<ALI(36)<<" Link power management : "<<"QUIESCE_SLEEP"<<endl;
		break;
	case MSHR:
		settingOut<<ALI(36)<<" Link power management : "<<"MSHR"<<endl;
		break;
	case LINK_MONITOR:
		settingOut<<ALI(36)<<" Link power management : "<<"LINK_MONITOR"<<endl;
		break;
	case AUTONOMOUS:
		settingOut<<ALI(36)<<" Link power management : "<<"AUTONOMOUS"<<endl;
		break;
	default:
		settingOut<<ALI(36)<<" Link power management : "<<LINK_POWER<<endl;
		break;
	}
	if(LINK_POWER != NO_MANAGEMENT) {
		settingOut<<ALI(36)<<" Rreqeust num to awake : "<<AWAKE_REQ<<endl;
		settingOut<<ALI(36)<<" tQUIESCE : "<<tQUIESCE<<endl;
		settingOut<<ALI(36)<<" One epoch for link management : "<<LINK_EPOCH<<endl;
		settingOut<<ALI(36)<<" Scaling factor of MSHR : "<<MSHR_SCALING<<endl;
		settingOut<<ALI(36)<<" Scaling factor of link monitor : "<<LINK_SCALING<<endl;
	}
	
	settingOut<<endl<<"              ==== DRAM general setting ===="<<endl;
	settingOut<<ALI(36)<<" Memory density : "<<MEMORY_DENSITY<<endl;
	settingOut<<ALI(36)<<" The number of vaults : "<<NUM_VAULTS<<endl;
	settingOut<<ALI(36)<<" The number of banks : "<<NUM_BANKS<<endl;
	settingOut<<ALI(36)<<" The number of rows : "<<NUM_ROWS<<endl;
	settingOut<<ALI(36)<<" The number of columns : "<<NUM_COLS<<endl;
	settingOut<<ALI(36)<<" Address mapping (Max block size) : "<<ADDRESS_MAPPING<<endl;
	settingOut<<ALI(36)<<" Vault controller max buffer size : "<<MAX_VLT_BUF<<endl;
	settingOut<<ALI(36)<<" Command queue max size : "<<MAX_CMD_QUE<<endl;
	settingOut<<ALI(36)<<" Command queue structure : "<<(QUE_PER_BANK ? "Bank-level command queue (Bank-Level parallelism)" : "Vault-level command queue (Non bank-Level parallelism)")<<endl;
	settingOut<<ALI(36)<<" Memory scheduling : "<<(OPEN_PAGE ? "open page policy" : "close page policy" )<<endl;
	settingOut<<ALI(36)<<" The maximum row buffer accesses : "<<MAX_ROW_ACCESSES<<endl;
	settingOut<<ALI(36)<<" Power-down mode : "<<(USE_LOW_POWER ? "Enable" : "Disable")<<endl;
	
	settingOut<<endl<<" ==== DRAM timing setting ===="<<endl;
	settingOut<<" Refresh period : "<<REFRESH_PERIOD<<endl;
	settingOut<<" tCK    [ns] : "<<tCK<<endl;
	settingOut<<" CWL   [clk] : "<<CWL<<endl;
	settingOut<<"  CL   [clk] : "<<CL<<endl;
	settingOut<<"  AL   [clk] : "<<AL<<endl;
	settingOut<<" tRAS  [clk] : "<<tRAS<<endl;
	settingOut<<" tRCD  [clk] : "<<tRCD<<endl;
	settingOut<<" tRRD  [clk] : "<<tRRD<<endl;
	settingOut<<" tRC   [clk] : "<<tRC<<endl;
	settingOut<<" tRP   [clk] : "<<tRP<<endl;
	settingOut<<" tCCD  [clk] : "<<tCCD<<endl;
	settingOut<<" tRTP  [clk] : "<<tRTP<<endl;
	settingOut<<" tWTR  [clk] : "<<tWTR<<endl;
	settingOut<<" tWR   [clk] : "<<tWR<<endl;
	settingOut<<" tRTRS [clk] : "<<tRTRS<<endl;
	settingOut<<" tRFC  [clk] : "<<tRFC<<endl;
	settingOut<<" tFAW  [clk] : "<<tFAW<<endl;
	settingOut<<" tCKE  [clk] : "<<tCKE<<endl;
	settingOut<<" tXP   [clk] : "<<tXP<<endl;
	settingOut<<" tCMD  [clk] : "<<tCMD<<endl;
	settingOut<<"  RL   [clk] : "<<RL<<endl;
	settingOut<<"  WL   [clk] : "<<WL<<endl;
	settingOut<<"  BL   [clk] : "<<BL<<endl;
	
	settingOut.flush();		settingOut.close();
}

//
//Print plot data file for bandwidth graph
//
void CasHMCWrapper::MakePlotData()
{
	double elapsedPlotTime = (double)(PLOT_SAMPLING*CPU_CLK_PERIOD*1E-9);
	
	//Effective link bandwidth
	vector<double> downLinkEffecPlotBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> upLinkEffecPlotBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> linkEffecPlotBandwidth = vector<double>(NUM_LINKS, 0);
	for(int i=0; i<NUM_LINKS; i++) {
		downLinkEffecPlotBandwidth[i] = (downLinkDataSize[i] - downLinkDataSizeTemp[i]) / elapsedPlotTime / (1<<30);
		upLinkEffecPlotBandwidth[i] = (upLinkDataSize[i] - upLinkDataSizeTemp[i]) / elapsedPlotTime / (1<<30);
		linkEffecPlotBandwidth[i] = downLinkEffecPlotBandwidth[i] + upLinkEffecPlotBandwidth[i];
		downLinkDataSizeTemp[i] = downLinkDataSize[i];
		upLinkDataSizeTemp[i] = upLinkDataSize[i];
	}
	
	//HMC bandwidth
	double hmcPlotBandwidth = (hmcTransmitSize - hmcTransmitSizeTemp)/elapsedPlotTime/(1<<30);
	hmcTransmitSizeTemp = hmcTransmitSize;
	
	plotDataOut<<setw(8)<<setfill(' ')<<currentClockCycle/PLOT_SAMPLING;
	for(int i=0; i<NUM_LINKS; i++) {
		plotDataOut<<setw(10)<<setfill(' ')<<linkEffecPlotBandwidth[i];
	}
	plotDataOut<<setw(10)<<setfill(' ')<<hmcPlotBandwidth<<endl;
}

//
//Print transaction traced statistic on epoch boundaries
//
void CasHMCWrapper::PrintEpochStatistic()
{
	uint64_t elapsedCycles;
	if(currentClockCycle%LOG_EPOCH == 0) {
		elapsedCycles = LOG_EPOCH;
	}
	else {
		elapsedCycles = currentClockCycle%LOG_EPOCH;
	}
	
	//Count transaction and packet type
	uint64_t epochReads = 0;
	uint64_t epochWrites = 0;
	uint64_t epochAtomics = 0;
	uint64_t epochReq = 0;
	uint64_t epochRes = 0;
	uint64_t epochFlow = 0;
	unsigned epochError = 0;
	
	for(int i=0; i<NUM_LINKS; i++) {
		epochReads	+= readPerLink[i];
		epochWrites	+= writePerLink[i];
		epochAtomics+= atomicPerLink[i];
		epochReq	+= reqPerLink[i];
		epochRes	+= resPerLink[i];
		epochFlow	+= flowPerLink[i];
		epochError	+= errorPerLink[i];
	}
	
	//Ttransaction traced latency statistic calculation
	if(tranFullLat.size() != linkFullLat.size()
	|| linkFullLat.size() != vaultFullLat.size()
	|| tranFullLat.size() != vaultFullLat.size()) {
		ERROR(" == Error - Traces vector size are different (tranFullLat : "<<tranFullLat.size()
				<<", linkFullLat : "<<linkFullLat.size()<<", vaultFullLat : "<<vaultFullLat.size());
		exit(0);
	}
	unsigned tranCount = tranFullLat.size();
	unsigned errorCount = errorRetryLat.size();
	for(int i=0; i<errorCount; i++) {
		errorRetrySum += errorRetryLat[i];
	}
	
	unsigned tranFullMax = 0;
	unsigned tranFullMin = -1;//max unsigned value
	unsigned linkFullMax = 0;
	unsigned linkFullMin = -1;
	unsigned vaultFullMax = 0;
	unsigned vaultFullMin = -1;
	unsigned errorRetryMax = 0;
	unsigned errorRetryMin = -1;
	
	double tranStdSum = 0;
	double linkStdSum = 0;
	double vaultStdSum = 0;
	double errorStdSum = 0;
	
	double tranFullMean = (tranCount==0 ? 0 : (double)tranFullSum/tranCount);
	double linkFullMean = (tranCount==0 ? 0 : (double)linkFullSum/tranCount);
	double vaultFullMean = (tranCount==0 ? 0 : (double)vaultFullSum/tranCount);
	double errorRetryMean = (errorCount==0 ? 0 : (double)errorRetrySum/errorCount);
	
	for(int i=0; i<tranCount; i++) {		
		tranFullMax = max(tranFullLat[i], tranFullMax);
		tranFullMin = min(tranFullLat[i], tranFullMin);
		tranStdSum += pow(tranFullLat[i]-tranFullMean, 2);

		linkFullMax = max(linkFullLat[i], linkFullMax);
		linkFullMin = min(linkFullLat[i], linkFullMin);
		linkStdSum += pow(linkFullLat[i]-linkFullMean, 2);

		vaultFullMax = max(vaultFullLat[i], vaultFullMax);
		vaultFullMin = min(vaultFullLat[i], vaultFullMin);
		vaultStdSum += pow(vaultFullLat[i]-vaultFullMean, 2);
	}
	for(int i=0; i<errorCount; i++) {
		errorRetryMax = max(errorRetryLat[i], errorRetryMax);
		errorRetryMin = min(errorRetryLat[i], errorRetryMin);
		errorStdSum += pow(errorRetryLat[i]-errorRetryMean, 2);
	}
	double tranStdDev = sqrt(tranCount==0 ? 0 : tranStdSum/tranCount);
	double linkStdDev = sqrt(tranCount==0 ? 0 : linkStdSum/tranCount);
	double vaultStdDev = sqrt(tranCount==0 ? 0 : vaultStdSum/tranCount);
	double errorRetryDev = sqrt(errorCount==0 ? 0 : errorStdSum/errorCount);
	tranFullLat.clear();
	linkFullLat.clear();
	vaultFullLat.clear();
	errorRetryLat.clear();
	
	//Bandwidth calculation
	double elapsedTime = (double)(elapsedCycles*CPU_CLK_PERIOD*1E-9);
	double hmcBandwidth = hmcTransmitSize/elapsedTime/(1<<30);
	vector<double> downLinkBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> upLinkBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> linkBandwidth = vector<double>(NUM_LINKS, 0);
	double linkBandwidthSum = 0;
	vector<double> downLinkEffecBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> upLinkEffecBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> linkEffecBandwidth = vector<double>(NUM_LINKS, 0);
	vector<uint64_t> linkDataSize = vector<uint64_t>(NUM_LINKS, 0);
	for(int i=0; i<NUM_LINKS; i++) {
		downLinkBandwidth[i] = downLinkTransmitSize[i] / elapsedTime / (1<<30);
		upLinkBandwidth[i] = upLinkTransmitSize[i] / elapsedTime / (1<<30);
		linkBandwidth[i] = downLinkBandwidth[i] + upLinkBandwidth[i];
		linkBandwidthSum += linkBandwidth[i];
		//Effective bandwidth is only data bandwith regardless of packet header and tail bits
		downLinkEffecBandwidth[i] = downLinkDataSize[i] / elapsedTime / (1<<30);
		upLinkEffecBandwidth[i] = upLinkDataSize[i] / elapsedTime / (1<<30);
		linkEffecBandwidth[i] = downLinkEffecBandwidth[i] + upLinkEffecBandwidth[i];
		linkDataSize[i] = downLinkDataSize[i] + upLinkDataSize[i];
	}
	
	//Link low power statistic
	double SleepModeAve = 0;
	double DownModeAve = 0;
	vector<double> linkSleepMode = vector<double>(NUM_LINKS, 0);
	vector<double> linkDownMode = vector<double>(NUM_LINKS, 0);
	for(int i=0; i<NUM_LINKS; i++) {
		linkSleepMode[i] = ((double)hmcCont->linkSleepTime[i]/currentClockCycle)*100;
		linkDownMode[i] = ((double)hmcCont->linkDownTime[i]/currentClockCycle)*100;
		SleepModeAve += linkSleepMode[i];
		DownModeAve += linkDownMode[i];
	}
	SleepModeAve /= NUM_LINKS;
	DownModeAve /= NUM_LINKS;
	
	
	//Print epoch statistic result
	STATE(endl<<endl<<"  ============= ["<<currentClockCycle/LOG_EPOCH<<"] Epoch statistic result ============="<<endl);
	STATE("  Current epoch : "<<currentClockCycle/LOG_EPOCH);
	STATE("  Current clock : "<<currentClockCycle<<"  (epoch clock : "<<elapsedCycles<<")"<<endl);
	
	STATE("        HMC bandwidth : "<<ALI(7)<<hmcBandwidth<<" GB/s  (Considered only data size)");
	STATE("       Link bandwidth : "<<ALI(7)<<linkBandwidthSum<<" GB/s  (Included flow packet)");
	STATE("     Transmitted data : "<<ALI(7)<<DataScaling(hmcTransmitSize)<<" ("<<hmcTransmitSize<<" B)"<<endl);
	
	if(LINK_POWER != NO_MANAGEMENT) {
		STATE("     Sleep mode ratio : "<<ALI(7)<<SleepModeAve<<" %");
		STATE("      Down mode ratio : "<<ALI(7)<<DownModeAve<<" %");
		STATE("Total low power ratio : "<<ALI(7)<<SleepModeAve+DownModeAve<<" %"<<endl);
	}
	
	STATE("    Tran latency mean : "<<tranFullMean*CPU_CLK_PERIOD<<" ns");
	STATE("                 std  : "<<tranStdDev*CPU_CLK_PERIOD<<" ns");
	STATE("                 max  : "<<tranFullMax*CPU_CLK_PERIOD<<" ns");
	STATE("                 min  : "<<(tranFullMin!=-1 ? tranFullMin : 0)*CPU_CLK_PERIOD<<" ns");
	STATE("    Link latency mean : "<<linkFullMean*CPU_CLK_PERIOD<<" ns");
	STATE("                 std  : "<<linkStdDev*CPU_CLK_PERIOD<<" ns");
	STATE("                 max  : "<<linkFullMax*CPU_CLK_PERIOD<<" ns");
	STATE("                 min  : "<<(linkFullMin!=-1 ? linkFullMin : 0)*CPU_CLK_PERIOD<<" ns");
	STATE("   Vault latency mean : "<<vaultFullMean*tCK<<" ns");
	STATE("                 std  : "<<vaultStdDev*tCK<<" ns");
	STATE("                 max  : "<<vaultFullMax*tCK<<" ns");
	STATE("                 min  : "<<(vaultFullMin!=-1 ? vaultFullMin : 0)*tCK<<" ns");
	STATE("   Retry latency mean : "<<errorRetryMean*CPU_CLK_PERIOD<<" ns");
	STATE("                 std  : "<<errorRetryDev*CPU_CLK_PERIOD<<" ns");
	STATE("                 max  : "<<errorRetryMax*CPU_CLK_PERIOD<<" ns");
	STATE("                 min  : "<<(errorRetryMin!=-1 ? errorRetryMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl);

	STATE("           Read count : "<<epochReads);
	STATE("          Write count : "<<epochWrites);
	STATE("         Atomic count : "<<epochAtomics);
	STATE("        Request count : "<<epochReq);
	STATE("       Response count : "<<epochRes);
	STATE("           Flow count : "<<epochFlow);
	STATE("    Transaction count : "<<tranCount);
	STATE("    Error abort count : "<<epochError);
	STATE("    Error retry count : "<<errorCount<<endl);
	
	for(int i=0; i<NUM_LINKS; i++) {
		STATE("  ----------------------  [Link "<<i<<"]");
		STATE("  |               Read per link : "<<readPerLink[i]);
		STATE("  |              Write per link : "<<writePerLink[i]);
		STATE("  |             Atomic per link : "<<atomicPerLink[i]);
		STATE("  |            Request per link : "<<reqPerLink[i]);
		STATE("  |           Response per link : "<<resPerLink[i]);
		STATE("  |               Flow per link : "<<flowPerLink[i]);
		STATE("  |        Error abort per link : "<<errorPerLink[i]);
		if(LINK_POWER != NO_MANAGEMENT) {
			STATE("  |            Sleep mode ratio : "<<linkSleepMode[i]<<" %");
			STATE("  |             Down mode ratio : "<<linkDownMode[i]<<" %");
			STATE("  |  Total low power mode ratio : "<<linkSleepMode[i]+linkDownMode[i]<<" %");
		}
		STATE("  |        Downstream Bandwidth : "<<ALI(7)<<downLinkBandwidth[i]<<" GB/s");
		STATE("  |          Upstream Bandwidth : "<<ALI(7)<<upLinkBandwidth[i]<<" GB/s");
		STATE("  |             Total Bandwidth : "<<ALI(7)<<linkBandwidth[i]<<" GB/s");
		STATE("  |  Downstream effec Bandwidth : "<<ALI(7)<<downLinkEffecBandwidth[i]<<" GB/s");
		STATE("  |    Upstream effec Bandwidth : "<<ALI(7)<<upLinkEffecBandwidth[i]<<" GB/s");
		STATE("  |       Total effec Bandwidth : "<<ALI(7)<<linkEffecBandwidth[i]<<" GB/s");
		STATE("  | Downstream transmitted data : "<<ALI(7)<<DataScaling(downLinkDataSize[i])<<" ("<<downLinkDataSize[i]<<" B)");
		STATE("  |   Upstream transmitted data : "<<ALI(7)<<DataScaling(upLinkDataSize[i])<<" ("<<upLinkDataSize[i]<<" B)");
		STATE("  -----  Total transmitted data : "<<ALI(7)<<DataScaling(linkDataSize[i])<<" ("<<linkDataSize[i]<<" B)"<<endl);
	}
	STATE("  * Effec bandwidth takes data transmission into account regardless of packet header and tail");

	//One epoch simulation statistic results are accumulated
	totalTranCount += tranCount;
	totalErrorCount += errorCount;
	
	totalHmcTransmitSize += hmcTransmitSize;
	hmcTransmitSize = 0;
	for(int i=0; i<NUM_LINKS; i++) {
		totalDownLinkTransmitSize[i] += downLinkTransmitSize[i];
		totalUpLinkTransmitSize[i] += upLinkTransmitSize[i];
		downLinkTransmitSize[i] = 0;
		upLinkTransmitSize[i] = 0;
		totalDownLinkDataSize[i] += downLinkDataSize[i];
		totalUpLinkDataSize[i] += upLinkDataSize[i];
		downLinkDataSize[i] = 0;
		upLinkDataSize[i] = 0;
	}
	
	totalTranFullSum += tranFullSum;
	totalLinkFullSum += linkFullSum;
	totalVaultFullSum += vaultFullSum;
	totalErrorRetrySum += errorRetrySum;
	tranFullSum = 0;
	linkFullSum = 0;
	vaultFullSum = 0;
	errorRetrySum = 0;
	
	for(int i=0; i<NUM_LINKS; i++) {
		totalReadPerLink[i] += readPerLink[i];		readPerLink[i] = 0;
		totalWritePerLink[i] += writePerLink[i];	writePerLink[i] = 0;
		totalAtomicPerLink[i] += atomicPerLink[i];	atomicPerLink[i] = 0;
		totalReqPerLink[i] += reqPerLink[i];		reqPerLink[i] = 0;
		totalResPerLink[i] += resPerLink[i];		resPerLink[i] = 0;
		totalFlowPerLink[i] += flowPerLink[i];		flowPerLink[i] = 0;
		totalErrorPerLink[i] += errorPerLink[i];	errorPerLink[i] = 0;
	}
	
	totalTranFullMax = max(tranFullMax, totalTranFullMax);
	totalTranFullMin = min(tranFullMin, totalTranFullMin);
	totalLinkFullMax = max(linkFullMax, totalLinkFullMax);
	totalLinkFullMin = min(linkFullMin, totalLinkFullMin);
	totalVaultFullMax = max(vaultFullMax, totalVaultFullMax);
	totalVaultFullMin = min(vaultFullMin, totalVaultFullMin);
	totalErrorRetryMax = max(errorRetryMax, totalErrorRetryMax);
	totalErrorRetryMin = min(errorRetryMin, totalErrorRetryMin);
	
	totalTranStdSum += tranStdSum;
	totalLinkStdSum += linkStdSum;
	totalVaultStdSum += vaultStdSum;
	totalErrorStdSum += errorStdSum;

	if(BANDWIDTH_PLOT) {
		hmcTransmitSizeTemp = 0;
		for(int i=0; i<NUM_LINKS; i++) {
			downLinkDataSizeTemp[i] = 0;
			upLinkDataSizeTemp[i] = 0;
		}
	}
}

//
//Print final transaction traced statistic accumulated on every epoch
//
void CasHMCWrapper::PrintFinalStatistic()
{
	time_t now;
    struct tm t;
    time(&now);
    t = *localtime(&now);
	
	string resName = logName + "_result.log";
	resultOut.open(resName.c_str());
	cout<<"\n   === Simulation finished  ( CPU clk:"<<currentClockCycle<<" ) ===   "<<endl;
	cout<<"  [ "<<resName<<" ] is generated"<<endl<<endl;
	
	double elapsedTime = (double)(currentClockCycle*CPU_CLK_PERIOD*1E-9);
	double hmcBandwidth = totalHmcTransmitSize/elapsedTime/(1<<30);
	vector<double> downLinkBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> upLinkBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> linkBandwidth = vector<double>(NUM_LINKS, 0);
	double linkBandwidthSum = 0;
	vector<double> downLinkEffecBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> upLinkEffecBandwidth = vector<double>(NUM_LINKS, 0);
	vector<double> linkEffecBandwidth = vector<double>(NUM_LINKS, 0);
	vector<uint64_t> totalLinkDataSize = vector<uint64_t>(NUM_LINKS, 0);
	for(int i=0; i<NUM_LINKS; i++) {
		downLinkBandwidth[i] = totalDownLinkTransmitSize[i] / elapsedTime / (1<<30);
		upLinkBandwidth[i] = totalUpLinkTransmitSize[i] / elapsedTime / (1<<30);
		linkBandwidth[i] = downLinkBandwidth[i] + upLinkBandwidth[i];
		linkBandwidthSum += linkBandwidth[i];
		//Effective bandwidth is only data bandwith regardless of packet header and tail bits
		downLinkEffecBandwidth[i] = totalDownLinkDataSize[i] / elapsedTime / (1<<30);
		upLinkEffecBandwidth[i] = totalUpLinkDataSize[i] / elapsedTime / (1<<30);
		linkEffecBandwidth[i] = downLinkEffecBandwidth[i] + upLinkEffecBandwidth[i];
		totalLinkDataSize[i] = totalDownLinkDataSize[i] + totalUpLinkDataSize[i];
	}
	
	double tranFullMean = (totalTranCount==0 ? 0 : (double)totalTranFullSum/totalTranCount);
	double linkFullMean = (totalTranCount==0 ? 0 : (double)totalLinkFullSum/totalTranCount);
	double vaultFullMean = (totalTranCount==0 ? 0 : (double)totalVaultFullSum/totalTranCount);
	double errorRetryMean = (totalErrorCount==0 ? 0 : (double)totalErrorRetrySum/totalErrorCount);
	double tranStdDev = sqrt(totalTranCount==0 ? 0 : totalTranStdSum/totalTranCount);
	double linkStdDev = sqrt(totalTranCount==0 ? 0 : totalLinkStdSum/totalTranCount);
	double vaultStdDev = sqrt(totalTranCount==0 ? 0 : totalVaultStdSum/totalTranCount);
	double errorRetryDev = sqrt(totalErrorCount==0 ? 0 : totalErrorStdSum/totalErrorCount);
	
	uint64_t epochReads = 0;
	uint64_t epochWrites = 0;
	uint64_t epochAtomics = 0;
	uint64_t epochReq = 0;
	uint64_t epochRes = 0;
	uint64_t epochFlow = 0;
	unsigned epochError = 0;
	for(int i=0; i<NUM_LINKS; i++) {
		epochReads	+= totalReadPerLink[i];
		epochWrites	+= totalWritePerLink[i];
		epochAtomics+= totalAtomicPerLink[i];
		epochReq	+= totalReqPerLink[i];
		epochRes	+= totalResPerLink[i];
		epochFlow	+= totalFlowPerLink[i];
		epochError	+= totalErrorPerLink[i];
	}
	
	//Link low power statistic
	uint64_t ActModeTotal = 0;
	uint64_t SleepModeTotal = 0;
	uint64_t DownModeTotal = 0;
	double SleepModeAve = 0;
	double DownModeAve = 0;
	vector<double> linkSleepMode = vector<double>(NUM_LINKS, 0);
	vector<double> linkDownMode = vector<double>(NUM_LINKS, 0);
	for(int i=0; i<NUM_LINKS; i++) {
		linkSleepMode[i] = ((double)hmcCont->linkSleepTime[i]/currentClockCycle)*100;
		linkDownMode[i] = ((double)hmcCont->linkDownTime[i]/currentClockCycle)*100;
		SleepModeAve += linkSleepMode[i];
		DownModeAve += linkDownMode[i];
		ActModeTotal += currentClockCycle - (hmcCont->linkSleepTime[i] + hmcCont->linkDownTime[i]);
		SleepModeTotal += hmcCont->linkSleepTime[i];
		DownModeTotal += hmcCont->linkDownTime[i];
	}
	
	double ActPowPerSec = (double)PowPerLane*LINK_SPEED;
	double SlpPowPerSec = (double)PowPerLane*LINK_SPEED*SleepPow/100;
	double DwnPowPerSec = (double)PowPerLane*LINK_SPEED*DownPow/100;
	
	double ActPower = (double)ActModeTotal*ActPowPerSec/currentClockCycle;
	double SleepPower = (double)SleepModeTotal*SlpPowPerSec/currentClockCycle;
	double DownPower = (double)DownModeTotal*DwnPowPerSec/currentClockCycle;

	SleepModeAve /= NUM_LINKS;
	DownModeAve /= NUM_LINKS;


	//Print statistic result
	resultOut<<"****************************************************************"<<endl;
	resultOut<<"*                      CasHMC version 1.3                      *"<<endl;
	resultOut<<"*                       result log file                        *"<<endl;
	resultOut<<"*            Date : "<<t.tm_year + 1900<<"/"<<setw(2)<<setfill('0')<<t.tm_mon+1<<"/"<<setw(2)<<setfill('0')<<t.tm_mday
			<<"      Time : "<<setw(2)<<setfill('0')<<t.tm_hour<<":"<<setw(2)<<setfill('0')<<t.tm_min<<":"<<setw(2)<<setfill('0')<<t.tm_sec
			<<"            *"<<endl;
	resultOut<<"****************************************************************"<<endl<<endl;
	
	resultOut<<"- Trace type : "<<traceType<<endl;
	if(traceType == "random") {
		resultOut<<"- Frequency of requests : "<<memUtil<<endl;
		resultOut<<"- The percentage of reads [%] : "<<rwRatio<<endl;
	}
	else if(traceType == "file") {
		resultOut<<"- Trace file : "<<traceFileName<<endl<<endl;
	}
	
	resultOut<<"  ============= CasHMC statistic result ============="<<endl<<endl;
	resultOut<<"  Elapsed epoch : "<<currentClockCycle/LOG_EPOCH<<endl;
	resultOut<<"  Elapsed clock : "<<currentClockCycle<<endl<<endl;
	
	resultOut<<"        HMC bandwidth : "<<ALI(7)<<hmcBandwidth<<" GB/s  (Considered only data size)"<<endl;
	resultOut<<"       Link bandwidth : "<<ALI(7)<<linkBandwidthSum<<" GB/s  (Included flow packet)"<<endl;
	resultOut<<"     Transmitted data : "<<ALI(7)<<DataScaling(totalHmcTransmitSize)<<" ("<<totalHmcTransmitSize<<" B)"<<endl<<endl;
	
	if(LINK_POWER != NO_MANAGEMENT) {
		resultOut<<"    Active link power : "<<ALI(7)<<ActPower<<" mW"<<endl;
		resultOut<<"     Sleep link power : "<<ALI(7)<<SleepPower<<" mW"<<endl;
		resultOut<<"      Down link power : "<<ALI(7)<<DownPower<<" mW"<<endl;
		resultOut<<"  Total power on link : "<<ALI(7)<<ActPower+SleepPower+DownPower<<" mW"<<endl<<endl;
		resultOut<<"     Sleep mode ratio : "<<ALI(7)<<SleepModeAve<<" %"<<endl;
		resultOut<<"      Down mode ratio : "<<ALI(7)<<DownModeAve<<" %"<<endl;
		resultOut<<"Total low-power ratio : "<<ALI(7)<<SleepModeAve+DownModeAve<<" %"<<endl<<endl;
	}
	else {
		resultOut<<"  Total power on link : "<<ALI(7)<<ActPower+SleepPower+DownPower<<" mW"<<endl<<endl;
	}

	resultOut<<"    Tran latency mean : "<<tranFullMean*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 std  : "<<tranStdDev*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 max  : "<<totalTranFullMax*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 min  : "<<(totalTranFullMin!=-1 ? totalTranFullMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"    Link latency mean : "<<linkFullMean*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 std  : "<<linkStdDev*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 max  : "<<totalLinkFullMax*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 min  : "<<(totalLinkFullMin!=-1 ? totalLinkFullMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"   Vault latency mean : "<<vaultFullMean*tCK<<" ns"<<endl;
	resultOut<<"                 std  : "<<vaultStdDev*tCK<<" ns"<<endl;
	resultOut<<"                 max  : "<<totalVaultFullMax*tCK<<" ns"<<endl;
	resultOut<<"                 min  : "<<(totalVaultFullMin!=-1 ? totalVaultFullMin : 0)*tCK<<" ns"<<endl;
	resultOut<<"   Retry latency mean : "<<errorRetryMean*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 std  : "<<errorRetryDev*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 max  : "<<totalErrorRetryMax*CPU_CLK_PERIOD<<" ns"<<endl;
	resultOut<<"                 min  : "<<(totalErrorRetryMin!=-1 ? totalErrorRetryMin : 0)*CPU_CLK_PERIOD<<" ns"<<endl<<endl;
	
	resultOut<<"           Read count : "<<epochReads<<endl;
	resultOut<<"          Write count : "<<epochWrites<<endl;
	resultOut<<"         Atomic count : "<<epochAtomics<<endl;
	resultOut<<"        Request count : "<<epochReq<<endl;
	resultOut<<"       Response count : "<<epochRes<<endl;
	resultOut<<"           Flow count : "<<epochFlow<<endl;
	resultOut<<"    Transaction count : "<<totalTranCount<<endl;
	resultOut<<"    Error abort count : "<<epochError<<endl;
	resultOut<<"    Error retry count : "<<totalErrorCount<<endl<<endl;
	
	for(int i=0; i<NUM_LINKS; i++) {
		resultOut<<"  ----------------------  [Link "<<i<<"]"<<endl;
		resultOut<<"  |               Read per link : "<<totalReadPerLink[i]<<endl;
		resultOut<<"  |              Write per link : "<<totalWritePerLink[i]<<endl;
		resultOut<<"  |             Atomic per link : "<<totalAtomicPerLink[i]<<endl;
		resultOut<<"  |            Request per link : "<<totalReqPerLink[i]<<endl;
		resultOut<<"  |           Response per link : "<<totalResPerLink[i]<<endl;
		resultOut<<"  |               Flow per link : "<<totalFlowPerLink[i]<<endl;
		resultOut<<"  |        Error abort per link : "<<totalErrorPerLink[i]<<endl;
		if(LINK_POWER != NO_MANAGEMENT) {
			resultOut<<"  |            Sleep mode ratio : "<<linkSleepMode[i]<<" %"<<endl;
			resultOut<<"  |             Down mode ratio : "<<linkDownMode[i]<<" %"<<endl;
			resultOut<<"  |  Total low power mode ratio : "<<linkSleepMode[i]+linkDownMode[i]<<" %"<<endl;
		}
		resultOut<<"  |        Downstream Bandwidth : "<<ALI(7)<<downLinkBandwidth[i]<<" GB/s"<<endl;
		resultOut<<"  |          Upstream Bandwidth : "<<ALI(7)<<upLinkBandwidth[i]<<" GB/s"<<endl;
		resultOut<<"  |             Total Bandwidth : "<<ALI(7)<<linkBandwidth[i]<<" GB/s"<<endl;
		resultOut<<"  |  Downstream effec Bandwidth : "<<ALI(7)<<downLinkEffecBandwidth[i]<<" GB/s"<<endl;
		resultOut<<"  |    Upstream effec Bandwidth : "<<ALI(7)<<upLinkEffecBandwidth[i]<<" GB/s"<<endl;
		resultOut<<"  |       Total effec Bandwidth : "<<ALI(7)<<linkEffecBandwidth[i]<<" GB/s"<<endl;
		resultOut<<"  | Downstream transmitted data : "<<ALI(7)<<DataScaling(totalDownLinkDataSize[i])<<" ("<<totalDownLinkDataSize[i]<<" B)"<<endl;
		resultOut<<"  |   Upstream transmitted data : "<<ALI(7)<<DataScaling(totalUpLinkDataSize[i])<<" ("<<totalUpLinkDataSize[i]<<" B)"<<endl;
		resultOut<<"  -----  Total transmitted data : "<<ALI(7)<<DataScaling(totalLinkDataSize[i])<<" ("<<totalLinkDataSize[i]<<" B)"<<endl<<endl;
	}
	resultOut<<"  * Effec bandwidth takes data transmission into account regardless of packet header and tail"<<endl;
}

//
//Transmitted data size scaling
//
string CasHMCWrapper::DataScaling(double dataScale)
{
	string outStr;
	char unitChar[5] = {' ','K','M','G','T'};
	
	int unitScale = 0;
	while(dataScale > 1024) {
		unitScale++;
		dataScale /= 1024;
		if(unitScale > 4)	break;
	}
	
	stringstream scaledData;
	scaledData<<ALI(7)<<dataScale;
	outStr = scaledData.str() + " " + unitChar[unitScale] + "B";
	return outStr;
}

} //namespace CasHMC


//for autoconf
extern "C"
{
	void libCasHMC_is_present(void)
	{
		;
	}
}