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

#include "Packet.h"

using namespace std;

namespace CasHMC
{
//Request packet
Packet::Packet(PacketType packet, PacketCommandType cmd, uint64_t addr, unsigned cub, unsigned lng, TranTrace *lat):
	packetType(packet),
	CMD(cmd),
	ADRS(addr),
	CUB(cub),
	LNG(lng),
	trace(lat)
{
	bufPopDelay=1;
	CRCtable[256]=0;	
	Pb=0;
	CRC=0; RTC=0; SLID=0;
	SEQ=0; FRP=0; RRP=0;
	AF=0; ERRSTAT=0; DINV=0;
	chkCRC = false;
	chkRRP = false;
	segment = false;
	reqDataSize=16;	//The minimum size is 16-Byte
	
	if(packet != FLOW) {
		TAG = (packetGlobalTAG++)%2048;
	}
	else {
		TAG = 0;
	}
	
	ADRS = (ADRS<<30)>>30;	//Address length is 34 bits
	
	if(CRC_CHECK) {
		if(LNG>1) {
			DATA = new uint64_t[(LNG-1)*2];
			uint64_t tempData;
			for(int i=0; i<(LNG-1)*2; i++) {
				tempData = rand();
				tempData = (tempData<<32)|rand();
				DATA[i] = tempData;
			}
		}
		else {
			DATA = NULL;
		}
		MakeCRCtable(CRCtable, 0xEB31D82E);
	}
	else {
		DATA = NULL;
	}
}
//Response packet
Packet::Packet(PacketType packet, PacketCommandType cmd, unsigned tag, unsigned lng, TranTrace *lat):
	packetType(packet),
	CMD(cmd),
	TAG(tag),
	LNG(lng),
	trace(lat)
{
	bufPopDelay=1;
	CRCtable[256]=0;	
	CUB=0; Pb=0;
	CRC=0; RTC=0; SLID=0;
	SEQ=0; FRP=0; RRP=0;
	AF=0; ERRSTAT=0; DINV=0;
	chkCRC = false;
	chkRRP = false;
	segment = false;
	reqDataSize=16;	//The minimum size is 16-Byte
	
	if(CRC_CHECK) {
		if(LNG>1) {
			DATA = new uint64_t[(LNG-1)*2];
			uint64_t tempData;
			for(int i=0; i<(LNG-1)*2; i++) {
				tempData = rand();
				tempData = (tempData<<32)|rand();
				DATA[i] = tempData;
			}
		}
		else {
			DATA = NULL;
		}
		MakeCRCtable(CRCtable, 0xEB31D82E);
	}
	else {
		DATA = NULL;
	}
}

Packet::~Packet()
{
	if(DATA != NULL) {
		delete[] DATA;
	}
}

Packet::Packet(const Packet &f)
{
	trace = f.trace;
	packetType = f.packetType;
	bufPopDelay = f.bufPopDelay;
	chkCRC = f.chkCRC;
	chkRRP = f.chkRRP;
	segment = f.segment;
	reqDataSize = f.reqDataSize;
	
	CUB = f.CUB;	TAG = f.TAG;
	LNG = f.LNG;	CMD = f.CMD;
	
	CRC = f.CRC;	RTC = f.RTC;
	SLID = f.SLID;	SEQ = f.SEQ;
	FRP = f.FRP;	RRP = f.RRP;
	
	ADRS = f.ADRS;	Pb = f.Pb;
	
	AF = f.AF;		ERRSTAT = f.ERRSTAT;
	DINV = f.DINV;
	
	if(CRC_CHECK) {
		if(LNG>1) {
			DATA = new uint64_t[(LNG-1)*2];
			uint64_t tempData;
			for(int i=0; i<(LNG-1)*2; i++) {
				tempData = rand();
				tempData = (tempData<<32)|rand();
				DATA[i] = tempData;
			}
		}
		else {
			DATA = NULL;
		}
		MakeCRCtable(CRCtable, 0xEB31D82E);
	}
	else {
		DATA = NULL;
	}
}

//
//Set Packet data included header, data, and tail for CRC generation
//
unsigned long Packet::GetCRC()
{
	uint64_t packetData[LNG*2];

	if(packetType == REQUEST) {
		packetData[0] <<= 3;	packetData[0] |= (CUB&0x7);
		packetData[0] <<= 3;	packetData[0] |= (RES&0x7);
		packetData[0] <<= 34;	packetData[0] |= (ADRS&0x3FFFFFFFF);
		packetData[0] <<= 1;	packetData[0] |= (RES&0x1);
		packetData[0] <<= 11;	packetData[0] |= (TAG&0x7FF);
		packetData[0] <<= 5;	packetData[0] |= (LNG&0x1F);
		packetData[0] <<= 7;	packetData[0] |= (CMD&0x7F);
	}
	else {
		packetData[0] <<= 3;	packetData[0] |= (CUB&0x7);
		packetData[0] <<= 19;	packetData[0] |= (RES&0x7FFFF);
		packetData[0] <<= 3;	packetData[0] |= (SLID&0x7);
		packetData[0] <<= 5;	packetData[0] |= (RES&0x1F);
		packetData[0] <<= 1;	packetData[0] |= (AF&0x1);
		packetData[0] <<= 10;	packetData[0] |= (RES&0x3FF);
		packetData[0] <<= 11;	packetData[0] |= (TAG&0x7FF);
		packetData[0] <<= 5;	packetData[0] |= (LNG&0x1F);
		packetData[0] <<= 7;	packetData[0] |= (CMD&0x7F);
	}
	
	for(int i=1; i<=(LNG-1)*2; i++){
		packetData[i]=DATA[i-1];
	}
	
	if(packetType == REQUEST) {
		packetData[LNG*2-1] <<= 32;		packetData[LNG*2-1] |= (0&0xFFFFFFFF);	//CRC initial value is '0' before CRC calculation
		packetData[LNG*2-1] <<= 3;		packetData[LNG*2-1] |= (RTC&0x7);
		packetData[LNG*2-1] <<= 3;		packetData[LNG*2-1] |= (SLID&0x7);
		packetData[LNG*2-1] <<= 4;		packetData[LNG*2-1] |= (RES&0xF);
		packetData[LNG*2-1] <<= 1;		packetData[LNG*2-1] |= (Pb&0x1);
		packetData[LNG*2-1] <<= 3;		packetData[LNG*2-1] |= (SEQ&0x7);
		packetData[LNG*2-1] <<= 9;		packetData[LNG*2-1] |= (FRP&0x1FF);
		packetData[LNG*2-1] <<= 9;		packetData[LNG*2-1] |= (RRP&0x1FF);
	}
	else {
		packetData[LNG*2-1] <<= 32;		packetData[LNG*2-1] |= (0&0xFFFFFFFF);	//CRC initial value is '0' before CRC calculation
		packetData[LNG*2-1] <<= 3;		packetData[LNG*2-1] |= (RTC&0x7);
		packetData[LNG*2-1] <<= 7;		packetData[LNG*2-1] |= (ERRSTAT&0x7F);
		packetData[LNG*2-1] <<= 1;		packetData[LNG*2-1] |= (DINV&0x1);
		packetData[LNG*2-1] <<= 3;		packetData[LNG*2-1] |= (SEQ&0x7);
		packetData[LNG*2-1] <<= 9;		packetData[LNG*2-1] |= (FRP&0x1FF);
		packetData[LNG*2-1] <<= 9;		packetData[LNG*2-1] |= (RRP&0x1FF);
	}
	
	unsigned char buf[8];
	uint32_t crc = 0;
	
	for(int j=0; j<LNG*2; j++){
		for(int i=0; i<8; i++){
			buf[i] = packetData[j];
			packetData[j] >>= 8;
		}
		crc = CalcCRC(buf, 8, crc, CRCtable);
	}
	return crc;
}

//
//Makes remainder table diveded by polynomial (CRC-32K)
//
void Packet::MakeCRCtable(uint32_t *table, uint32_t id) 
{
	uint32_t k;
	for(int i=0; i<256; ++i){
		k = i;
		for(int j=0; j<8; ++j){
			if(k&1)	k = (k >> 1) ^ id;
			else	k >>= 1;
		}
		table[i] = k;
	}
}
	
//
//Calculates CRC value
//
uint32_t Packet::CalcCRC(const unsigned char *mem, signed int size, uint32_t crc, uint32_t *table)
{
	crc = ~crc;
	while(size--){
		crc = table[(crc ^ *(mem++)) & 0xFF] ^ (crc >> 8);
	}
	return ~crc;
}

//
//Reduction packetGlobalTAG
//
void Packet::ReductGlobalTAG()
{
	packetGlobalTAG--;
}	
	
//
//Defines "<<" operation for printing
//
ostream& operator<<(ostream &out, const Packet &f)
{
	string header;
	stringstream id;
	id << f.TAG;
	
	switch(f.CMD) {
		//Request commands
		case WR16:	header = "[P" + id.str() + "-WR16]";	break;		case WR32:	header = "[P" + id.str() + "-WR32]";	break;
		case WR48:	header = "[P" + id.str() + "-WR48]";	break;		case WR64:	header = "[P" + id.str() + "-WR64]";	break;
		case WR80:	header = "[P" + id.str() + "-WR80]";	break;		case WR96:	header = "[P" + id.str() + "-WR96]";	break;
		case WR112:	header = "[P" + id.str() + "-WR112]";	break;		case WR128:	header = "[P" + id.str() + "-WR128]";	break;
		case MD_WR:	header = "[P" + id.str() + "-MD_WR]";	break;		case WR256:	header = "[P" + id.str() + "-WR256]";	break;
		case P_WR16:	header = "[P" + id.str() + "-P_WR16]";	break;		case P_WR32:	header = "[P" + id.str() + "-P_WR32]";	break;
		case P_WR48:	header = "[P" + id.str() + "-P_WR48]";	break;		case P_WR64:	header = "[P" + id.str() + "-P_WR64]";	break;
		case P_WR80:	header = "[P" + id.str() + "-P_WR80]";	break;		case P_WR96:	header = "[P" + id.str() + "-P_WR96]";	break;
		case P_WR112:	header = "[P" + id.str() + "-P_WR112]";	break;		case P_WR128:	header = "[P" + id.str() + "-P_WR128]";	break;
		case P_WR256:	header = "[P" + id.str() + "-P_WR256]";	break;		
		case RD16:	header = "[P" + id.str() + "-RD16]";	break;		case RD32:	header = "[P" + id.str() + "-RD32]";	break;
		case RD48:	header = "[P" + id.str() + "-RD48]";	break;		case RD64:	header = "[P" + id.str() + "-RD64]";	break;
		case RD80:	header = "[P" + id.str() + "-RD80]";	break;		case RD96:	header = "[P" + id.str() + "-RD96]";	break;
		case RD112:	header = "[P" + id.str() + "-RD112]";	break;		case RD128:	header = "[P" + id.str() + "-RD128]";	break;
		case RD256:	header = "[P" + id.str() + "-RD256]";	break;		case MD_RD:	header = "[P" + id.str() + "-MD_RD]";	break;
		//ATOMICS commands
		case _2ADD8:	header = "[P" + id.str() + "-2ADD8]";	break;		case ADD16:		header = "[P" + id.str() + "-ADD16]";	break;
		case P_2ADD8:	header = "[P" + id.str() + "-P_2ADD8]";	break;		case P_ADD16:	header = "[P" + id.str() + "-P_ADD16]";	break;
		case _2ADDS8R:	header = "[P" + id.str() + "-2ADDS8R]";	break;		case ADDS16R:	header = "[P" + id.str() + "-ADDS16R]";	break;
		case INC8:		header = "[P" + id.str() + "-INC8]";	break;		case P_INC8:	header = "[P" + id.str() + "-P_INC8]";	break;
		case XOR16:		header = "[P" + id.str() + "-XOR16]";	break;		case OR16:		header = "[P" + id.str() + "-OR16]";	break;
		case NOR16:		header = "[P" + id.str() + "-NOR16]";	break;		case AND16:		header = "[P" + id.str() + "-AND16]";	break;
		case NAND16:	header = "[P" + id.str() + "-NAND16]";	break;		case CASGT8:	header = "[P" + id.str() + "-CASGT8]";	break;
		case CASLT8:	header = "[P" + id.str() + "-CASLT8]";	break;		case CASGT16:	header = "[P" + id.str() + "-CASGT16]";	break;
		case CASLT16:	header = "[P" + id.str() + "-CASLT16]";	break;		case CASEQ8:	header = "[P" + id.str() + "-CASEQ8]";	break;
		case CASZERO16:	header = "[P" + id.str() + "-CASZR16]";	break;		case EQ16:		header = "[P" + id.str() + "-EQ16]";	break;
		case EQ8:		header = "[P" + id.str() + "-EQ8]";		break;		case BWR:		header = "[P" + id.str() + "-BWR]";		break;
		case P_BWR:		header = "[P" + id.str() + "-P_BWR]";	break;		case BWR8R:		header = "[P" + id.str() + "-BWR8R]";	break;
		case SWAP16:	header = "[P" + id.str() + "-SWAP16]";	break;
		//Flow Commands
		case NULL_:		header = "[P" + id.str() + "-NULL]";		break;		case PRET:		header = "[P" + id.str() + "-PRET]";		break;
		case TRET:		header = "[P" + id.str() + "-TRET]";		break;		
		case IRTRY:		if(f.FRP==1)		header = "[P" + id.str() + "-IRTRY1]";
						else if(f.FRP==2)	header = "[P" + id.str() + "-IRTRY2]";						
						else				header = "[P" + id.str() + "-IRTRY]";	break;
		//Respond commands
		case RD_RS:		header = "[P" + id.str() + "-RD_RS]";		break;		case WR_RS:		header = "[P" + id.str() + "-WR_RS]";		break;
		case MD_RD_RS:	header = "[P" + id.str() + "-MD_RD_RS]";	break;		case MD_WR_RS:	header = "[P" + id.str() + "-MD_WR_RS]";	break;
		case ERROR:		header = "[P" + id.str() + "-ERROR]";		break;
		default:
			ERROR(" (FL) == Error - Trying to print unknown kind of Packet CMD type");
			ERROR("         F-"<<f.TAG<<" [?"<<f.CMD<<"?] [TAG:"<<f.TAG<<"] [SEQ:"<<f.SEQ<<"] [LNG:"<<f.LNG<<"]");
			exit(0);
	}
	out<<header;
	return out;
}

} //namespace CasHMC