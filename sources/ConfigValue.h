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

#ifndef CONFIGVALUE_H
#define CONFIGVALUE_H

//ConfigValue.h

#include <stdint.h>		//uint64_t
#include <iostream>		//cerr

//
//Debug setting
//
#ifdef DEBUG_LOG
	#define DEBUG(str)	if(DEBUG_SIM && !ONLY_CR) {debugOut<<str<<endl;}
	#define DEBUGN(str)	if(DEBUG_SIM && !ONLY_CR) {debugOut<<str;}
	#define DE_CR(str)	if(DEBUG_SIM) {debugOut<<str<<endl;}
	#define DEN_CR(str)	if(DEBUG_SIM) {debugOut<<str;}
	#define STATE(str)	if(STATE_SIM) {stateOut<<str<<endl;}
	#define STATEN(str)	if(STATE_SIM) {stateOut<<str;}
	#define DE_ST(str)	if(DEBUG_SIM) {debugOut<<str<<endl;}	if(STATE_SIM) {stateOut<<str<<endl;}
	#define DE_STN(str)	if(DEBUG_SIM) {debugOut<<str;}				if(STATE_SIM) {stateOut<<str;}
#else
	#define DEBUG(str)	if(DEBUG_SIM && !ONLY_CR) {std::cout<<str<<endl;}
	#define DEBUGN(str)	if(DEBUG_SIM && !ONLY_CR) {std::cout<<str;}
	#define DE_CR(str)	if(DEBUG_SIM) {std::cout<<str<<endl;}
	#define DEN_CR(str)	if(DEBUG_SIM) {std::cout<<str;}
	#define STATE(str)	if(STATE_SIM) {std::cout<<str<<endl;}
	#define STATEN(str)	if(STATE_SIM) {std::cout<<str;}
	#define DE_ST(str)	if(DEBUG_SIM || STATE_SIM) {std::cout<<str<<endl;}
	#define DE_STN(str)	if(DEBUG_SIM || STATE_SIM) {std::cout<<str;}
#endif	
#define ERROR(str)	std::cerr<<"[ERROR ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<endl;
#define ALI(n)		setw(n)<<setfill(' ')


enum LINK_PRIORITY_SCHEME			//If you want to make new sceme, it is necessary to modify FindAvailableLink function
{
	ROUND_ROBIN,
	BUFFER_AWARE
};

enum LINK_POWER_MANAGEMENT			//Link power state management
{
	NO_MANAGEMENT,
	QUIESCE_SLEEP,
	MSHR,
	LINK_MONITOR,
	AUTONOMOUS
};

enum MAPPING_SCHEME
{
	MAX_BLOCK_32B=32,
	MAX_BLOCK_64B=64,
	MAX_BLOCK_128B=128,
	MAX_BLOCK_256B=256
};


//Configure values that are extern vaules
//
//SimConfig.ini
//
extern uint64_t LOG_EPOCH;
extern bool DEBUG_SIM;
extern bool ONLY_CR;
extern bool STATE_SIM;
extern int PLOT_SAMPLING;
extern bool BANDWIDTH_PLOT;

extern double CPU_CLK_PERIOD;
extern int TRANSACTION_SIZE;
extern int MAX_REQ_BUF;

extern int NUM_LINKS;
extern int LINK_WIDTH;
extern double LINK_SPEED;
extern int MAX_LINK_BUF;
extern int MAX_RETRY_BUF;
extern int MAX_VLT_BUF;
extern int MAX_CROSS_BUF;
extern int MAX_CMD_QUE;
extern bool CRC_CHECK;
extern double CRC_CAL_CYCLE;
extern int NUM_OF_IRTRY;
extern int RETRY_ATTEMPT_LIMIT;
extern int LINK_BER;
extern LINK_PRIORITY_SCHEME LINK_PRIORITY;
extern LINK_POWER_MANAGEMENT LINK_POWER;
extern int AWAKE_REQ;
extern double LINK_EPOCH;
extern double MSHR_SCALING;
extern double LINK_SCALING;

extern double PowPerLane;
extern double SleepPow;
extern double DownPow;

extern double tPST;
extern double tSME;
extern double tSS;
extern double tSD;
extern double tSREF;
extern double tOP;
extern double tQUIESCE;

extern double tTXD;
extern double tRESP1;
extern double tRESP2;
extern double tPSC;

//
//DRAMConfig.ini
//
extern int MEMORY_DENSITY;
extern int NUM_VAULTS;
extern int NUM_BANKS;
extern int NUM_ROWS;
extern int NUM_COLS;
extern MAPPING_SCHEME ADDRESS_MAPPING;
extern bool QUE_PER_BANK;
extern bool OPEN_PAGE;
extern int MAX_ROW_ACCESSES;
extern bool USE_LOW_POWER;

extern int REFRESH_PERIOD;

extern double tCK;
extern unsigned CWL;
extern unsigned CL;
extern unsigned AL;
extern unsigned tRAS;
extern unsigned tRCD;
extern unsigned tRRD;
extern unsigned tRC;
extern unsigned tRP;
extern unsigned tCCD;
extern unsigned tRTP;
extern unsigned tWTR;
extern unsigned tWR;
extern unsigned tRTRS;
extern unsigned tRFC;
extern unsigned tFAW;
extern unsigned tCKE;
extern unsigned tXP;
extern unsigned tCMD;

#define RL (AL+CL)
#define WL (AL+CWL)
#define BL ((unsigned)ADDRESS_MAPPING/32)

#define READ_TO_PRE_DELAY (AL+BL/2+max(tRTP,tCCD)-tCCD)
#define WRITE_TO_PRE_DELAY (WL+BL/2+tWR)
#define READ_TO_WRITE_DELAY (RL+tCCD+2-WL)
#define READ_AUTOPRE_DELAY (AL+tRTP+tRP)
#define WRITE_AUTOPRE_DELAY (WL+BL/2+tWR+tRP)
#define WRITE_TO_READ_DELAY_B (WL+BL/2+tWTR)
//#define WRITE_TO_READ_DELAY_R (WL+BL/2+tRTRS-RL)


namespace CasHMC
{

unsigned inline _log2(unsigned value)
{
	unsigned logbase2 = 0;
	unsigned orig = value;
	value >>= 1;
	while(value > 0)
	{
		value >>= 1;
		logbase2++;
	}
	if(1<<logbase2 < orig)	logbase2++;
	return logbase2;
}

}

#endif