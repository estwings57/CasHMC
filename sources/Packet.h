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

#ifndef PACKET_H
#define PACKET_H

//Packet.h

#include <stdint.h>		//uint64_t
#include <stdlib.h>		//exit(0)
#include <iomanip>		//setw()
#include <iostream> 	//ostream
#include <sstream>		//stringstream

#include "SimConfig.h"
#include "TranTrace.h"

#define RES 0

using namespace std;

namespace CasHMC
{
	
static unsigned packetGlobalTAG=0;

enum PacketType
{
	REQUEST,
	RESPONSE,
	FLOW
};

enum PacketCommandType
{
	//Request commands
	WR16=8, WR32, WR48, WR64, WR80, WR96, WR112, WR128, MD_WR, WR256=79, 				//WRITE Requests
	P_WR16=24, P_WR32, P_WR48, P_WR64, P_WR80, P_WR96, P_WR112, P_WR128, P_WR256=95, 	//POSTED WRITE Requests
	RD16=48, RD32, RD48, RD64, RD80, RD96, RD112, RD128, RD256=119, MD_RD=40, 			//READ Requests
	//ATOMICS commands
	_2ADD8=18, ADD16, P_2ADD8=34, P_ADD16, _2ADDS8R=82, ADDS16R, INC8=80, P_INC8=84, 	//ARITHMETIC ATOMICS
	XOR16=64, OR16, NOR16, AND16, NAND16, 												//BOOLEAN ATOMICS
	CASGT8=96, CASLT8, CASGT16, CASLT16, CASEQ8, CASZERO16, EQ16=104, EQ8, 				//COMPARISON ATOMICS
	BWR=17, P_BWR=33, BWR8R=81, SWAP16=106, 											//BITWISE ATOMICS
	//Flow Commands
	NULL_=0, PRET=1, TRET=2, IRTRY=3,
	
	//Respond commands
	RD_RS=56, WR_RS, MD_RD_RS, MD_WR_RS, ERROR=62
};

class Packet
{
public:
	//Functions
	Packet(PacketType packet, PacketCommandType cmd, uint64_t addr, unsigned cub, unsigned lng, TranTrace *lat);	//Request Packet
	Packet(PacketType packet, PacketCommandType cmd, unsigned tag, unsigned lng, TranTrace *lat);				//Response Packet
	virtual ~Packet();
	Packet(const Packet &f);
	unsigned long GetCRC();
	void MakeCRCtable(uint32_t *table, uint32_t id);
	uint32_t CalcCRC(const unsigned char *mem, signed int size, uint32_t crc, uint32_t *table);
	void ReductGlobalTAG();
	
	//Fields
	TranTrace *trace;
	PacketType packetType;	//Type of transaction (defined above)
	int bufPopDelay;
	uint64_t *DATA;
	uint32_t CRCtable[256];
	bool chkCRC;
	bool chkRRP;
	bool segment;
	unsigned reqDataSize;
	
	//Packet Common Fields
	unsigned CUB, TAG, LNG;
	PacketCommandType CMD;
	
	unsigned CRC, RTC, SLID;
	unsigned SEQ, FRP, RRP;
	
	//Request Packet Fields
	uint64_t ADRS, Pb;
	
	//Response Packet Fields
	unsigned AF, ERRSTAT, DINV;
};

ostream& operator<<(ostream &out, const Packet &t);
}

#endif