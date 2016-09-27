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

#ifndef DRAMCOMMAND_H
#define DRAMCOMMAND_H

//DRAMCommand.h

#include <stdint.h>		//uint64_t
#include <stdlib.h>		//exit(0)
#include <iomanip>		//setw()
#include <iostream> 	//ostream
#include <sstream>		//stringstream

#include "SimConfig.h"
#include "TranTrace.h"
#include "Packet.h"

using namespace std;

namespace CasHMC
{
	
enum DRAMCommandType
{
	ACTIVATE,
	READ,
	READ_P,
	WRITE,
	WRITE_P,
	READ_DATA,
	WRITE_DATA,
	PRECHARGE,
	REFRESH,
	POWERDOWN_ENTRY,
	POWERDOWN_EXIT
};
	
class DRAMCommand
{
public:
	//
	//Functions
	//
	DRAMCommand(DRAMCommandType cmdtype, unsigned tag, unsigned bnk, unsigned col, unsigned rw, unsigned dSize,
					bool pst, TranTrace *lat, bool last, PacketCommandType pktCMD, bool atm, bool seg);
	DRAMCommand(const DRAMCommand &dc);
	virtual ~DRAMCommand();

	//
	//Fields
	//
	TranTrace *trace;
	DRAMCommandType commandType;
	unsigned packetTAG;
	unsigned bank;
	unsigned column;
	unsigned row;
	unsigned dataSize;		//[Byte]
	bool posted;
	bool lastCMD;
	PacketCommandType packetCMD;
	bool atomic;
	bool segment;
};

ostream& operator<<(ostream &out, const DRAMCommand &dc);
}

#endif