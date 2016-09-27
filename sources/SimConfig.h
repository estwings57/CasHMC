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

#ifndef SIMCONFIG_H
#define SIMCONFIG_H

//SimConfig.h

#include <fstream>		//ofstream
#include <stdint.h>		//uint64_t

//
//Debug setting
//
#ifdef DEBUG_LOG
	#define DEBUG(str)	if(DEBUG_SIM && !ONLY_CR) {debugOut<<str<<std::endl;}
	#define DEBUGN(str)	if(DEBUG_SIM && !ONLY_CR) {debugOut<<str;}
	#define DE_CR(str)	if(DEBUG_SIM) {debugOut<<str<<std::endl;}
	#define DEN_CR(str)	if(DEBUG_SIM) {debugOut<<str;}
	#define STATE(str)	if(STATE_SIM) {stateOut<<str<<std::endl;}
	#define STATEN(str)	if(STATE_SIM) {stateOut<<str;}
	#define DE_ST(str)	if(DEBUG_SIM) {debugOut<<str<<std::endl;}	if(STATE_SIM) {stateOut<<str<<std::endl;}
	#define DE_STN(str)	if(DEBUG_SIM) {debugOut<<str;}				if(STATE_SIM) {stateOut<<str;}
#else
	#define DEBUG(str)	if(DEBUG_SIM && !ONLY_CR) {std::cout<<str<<std::endl;}
	#define DEBUGN(str)	if(DEBUG_SIM && !ONLY_CR) {std::cout<<str;}
	#define DE_CR(str)	if(DEBUG_SIM) {std::cout<<str<<std::endl;}
	#define DEN_CR(str)	if(DEBUG_SIM) {std::cout<<str;}
	#define STATE(str)	if(STATE_SIM) {std::cout<<str<<std::endl;}
	#define STATEN(str)	if(STATE_SIM) {std::cout<<str;}
	#define DE_ST(str)	if(DEBUG_SIM || STATE_SIM) {std::cout<<str<<std::endl;}
	#define DE_STN(str)	if(DEBUG_SIM || STATE_SIM) {std::cout<<str;}
#endif	
#define ERROR(str)	std::cerr<<"[ERROR ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<std::endl;
#define ALI(n)		setw(n)<<setfill(' ')

static uint64_t LOG_EPOCH = 1000000;
static bool DEBUG_SIM = false;		//Debug log file generation (true / false)
static bool ONLY_CR = false;		//Print only critical debug
static bool STATE_SIM = false;		//State log file generation (true / false)
static int PLOT_SAMPLING = 10000;	//(cycle)Bandwidth graph data time unit
static bool BANDWIDTH_PLOT = true;	//Bandwidth graph files generation


//
//Memory transaction setting
//
static double CPU_CLK_PERIOD = 0.5;	//(ns) CPU clock period in nanoseconds
static int TRANSACTION_SIZE = 32; 	//(byte) Data size of DRAM request (the internal 32-byte granularity of the DRAM data bus within each vault in the HMC)
static int MAX_REQ_BUF = 4;			//(transaction) Request buffer size in HMC controller

//
//Link(SerDes) setting
//
static int NUM_LINKS = 4;			//The number of links
static int LINK_WIDTH = 16;			//Full-Width(16-lane), Half-width(8-lane), and quarter-width link(4-lane)
static double LINK_SPEED = 30;		//(Gb/s) (12.5, 15, 25, 28, 30)

static int MAX_LINK_BUF = 32;		//(packet) Link master output buffer and Link slave input buffer size (one buffer space is for 1 unit packet (128 bits) )
									//// the minimum packet length is 17 FLITs (256-byte WRITE request)
static int MAX_RETRY_BUF = 32;		//(packet) Retry buffer size in link mater (one buffer space is for 1 unit packet (128 bits) )
									////  retry buffer that can hold up to 512 FLITs [Hybrid Memory Cube Specification 2.1 p.93]
static int MAX_VLT_BUF = 32;		//(packet) Vault controller buffer size (one buffer space is for 1 unit packet (128 bits) )
static int MAX_CROSS_BUF = 64;		
									//(packet) Crossbar switch buffer size (one buffer space is for 1 unit packet (128 bits) )
static int MAX_CMD_QUE = 16;		//(Command) Command queue size (the minimum is 8 due to 256-byte request [max data size(256) / min address mapping block size(32)])

static bool CRC_CHECK = true;		//CRC checking enable
static double CRC_CAL_CYCLE = 0.01;	//(clock) CPU clock cycles to calculate CRC value of one packet (128 bits)
									////  '0' of CRC_CAL_CYCLE means that CRC calculation delay is completely hidden in transmitting packet header
									////  (the beginning of the packet may have been forwarded before the CRC was performed at the tail, to minimize latency )

static int NUM_OF_IRTRY = 2;		//The number of single-FLIT IRTRY packets to be transmitted during StartRetry sequence
static int RETRY_ATTEMPT_LIMIT = 2;	//The number of retry attempts when the retry timer is time-out
static int LINK_BER = -10;			//(the power of 10) High speed serial links (SerDes) require the Bit Error Rate (BER) to be at the level of 10^−12 or lower. 
									////  For a standard like Gigabit Ethernet (Data rate = 1.25Gbps), that specifies a bit error rate of less than 10^-12

									
//If you want to make new sceme, it is necessary to modify FindAvailableLink function
enum LINK_PRIORITY_SCHEME
{
	ROUND_ROBIN,
	BUFFER_AWARE
};
static LINK_PRIORITY_SCHEME LINK_PRIORITY = ROUND_ROBIN;


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

#endif