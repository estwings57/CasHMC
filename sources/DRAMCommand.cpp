/*********************************************************************************
*  CasHMC v1.0 - 2016.05.07
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright (c) 2016, Dong-Ik Jeon
*                      Ki-Seok Chung
*                      Hanyang University
*                      estwings57 [at] gmail [dot] com
*  All rights reserved.
*********************************************************************************/

#include "DRAMCommand.h"

namespace CasHMC
{
	
DRAMCommand::DRAMCommand(DRAMCommandType cmdtype, unsigned tag, unsigned bnk, unsigned col, unsigned rw, 
							unsigned dSize, bool pst, TranTrace *lat, bool last):
	commandType(cmdtype),
	packetTAG(tag),
	bank(bnk),
	column(col),
	row(rw),
	dataSize(dSize),
	posted(pst),
	trace(lat),
	lastCMD(last)
{
}

DRAMCommand::~DRAMCommand()
{
}

//
//Defines "<<" operation for printing
//
ostream& operator<<(ostream &out, const DRAMCommand &dc)
{
	string header;
	stringstream id;
	id << dc.packetTAG;
	
	switch(dc.commandType) {
		case ACTIVATE:
			header = "[C" + id.str() + "-ACT]";
			break;
		case READ:
			header = "[C" + id.str() + "-READ]";
			break;
		case READ_P:
			header = "[C" + id.str() + "-READ_P]";
			break;
		case WRITE:
			header = "[C" + id.str() + "-WRITE]";
			break;
		case WRITE_P:
			header = "[C" + id.str() + "-WRITE_P]";
			break;
		case PRECHARGE:
			header = "[C" + id.str() + "-PRE]";
			break;
		case REFRESH:
			header = "[C" + id.str() + "-REF]";
			break;
		case READ_DATA:
			header = "[C" + id.str() + "-R_DATA]";
			break;
		case WRITE_DATA:
			header = "[C" + id.str() + "-W_DATA]";
			break;
		default:
			ERROR(" (DC) == Error - Trying to print unknown kind of bus packet");
			ERROR("         Type : "<<dc.commandType);
			exit(0);
	}
	out<<header;
	return out;
}

} //namespace CasHMC
