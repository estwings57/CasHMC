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

#include "HMCController.h"

using namespace std;

namespace CasHMC
{
	
HMCController::HMCController(ofstream &debugOut_, ofstream &stateOut_):
	DualVectorObject<Transaction, Packet>(debugOut_, stateOut_, MAX_REQ_BUF, MAX_LINK_BUF)
{
	header = " (HC)";
	
	inServiceLink = -1;
	
	//Make class objects
	downLinkMasters.reserve(NUM_LINKS);
	upLinkSlaves.reserve(NUM_LINKS);
	for(int l=0; l<NUM_LINKS; l++) {
		downLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, true));
		upLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, false));
	}
}

HMCController::~HMCController()
{
	for(int l=0; l<NUM_LINKS; l++) {
		delete downLinkMasters[l];
		delete upLinkSlaves[l];
	}
	downLinkMasters.clear();
	upLinkSlaves.clear();
}

//
//Callback receiving packet result
//
void HMCController::CallbackReceiveDown(Transaction *downEle, bool chkReceive)
{
	if(chkReceive) {
		downEle->trace->tranTransmitTime = currentClockCycle;
		if(downEle->transactionType == DATA_WRITE)	downEle->trace->statis->hmcTransmitSize += downEle->dataSize;
		//DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING transaction");
	}
	else {
		//DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) Transaction buffer FULL");
	}
}

void HMCController::CallbackReceiveUp(Packet *upEle, bool chkReceive)
{
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   PUSHING packet into buffer["<<upBuffers.size()<<"/"<<MAX_REQ_BUF-1<<"]");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
	}*/
}

//
//Update the state of HMC controller
// Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
//
void HMCController::Update()
{
	//Downstream buffer state
	if(bufPopDelay==0 && downBuffers.size() > 0) {
		//Check packet dependency
		int link = -1;
		for(int l=0; l<NUM_LINKS; l++) {
			for(int i=0; i<downLinkMasters[l]->Buffers.size(); i++) {
				if(downLinkMasters[l]->Buffers[i] != NULL) {
					unsigned maxBlockBit = _log2(ADDRESS_MAPPING);
					if((downLinkMasters[l]->Buffers[i]->ADRS >> maxBlockBit) == (downBuffers[0]->address >> maxBlockBit)) {
						link = l;
						DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) This transaction has a DEPENDENCY with "<<*downLinkMasters[l]->Buffers[i]);
						break;
					}
				}
			}
		}
		
		if(link == -1) {
			for(int l=0; l<NUM_LINKS; l++) {
				link = FindAvailableLink(inServiceLink, downLinkMasters);
				if(link == -1) {
					//DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) all link buffer FULL");
				}
				else if(downLinkMasters[link]->currentState != LINK_RETRY) {
					Packet *packet = ConvTranIntoPacket(downBuffers[0]);
					if(downLinkMasters[link]->Receive(packet)) {
						DE_CR(ALI(18)<<header<<ALI(15)<<*packet<<"Down) SENDING packet to link mater "<<link<<" (LM_D"<<link<<")");
						delete downBuffers[0];
						downBuffers.erase(downBuffers.begin());
						break;
					}
					else {
						packet->ReductGlobalTAG();
						delete packet;
						//DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<"Down) Link "<<link<<" buffer FULL");	
					}
				}
			}
		}
		else {
			Packet *packet = ConvTranIntoPacket(downBuffers[0]);
			if(downLinkMasters[link]->Receive(packet)) {
				DE_CR(ALI(18)<<header<<ALI(15)<<*packet<<"Down) SENDING packet to link mater "<<link<<" (LM_D"<<link<<")");
				delete downBuffers[0];
				downBuffers.erase(downBuffers.begin());
			}
			else {
				packet->ReductGlobalTAG();
				delete packet;
				//DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<"Down) Link "<<link<<" buffer FULL");	
			}
		}
	}
	
	//Upstream buffer state
	if(upBuffers.size() > 0) {
		//Make sure that buffer[0] is not virtual tail packet.
		if(upBuffers[0] == NULL) {
			ERROR(header<<"  == Error - HMC controller up buffer[0] is NULL (It could be one of virtual tail packet occupying packet length  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
		else {
			DE_CR(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   RETURNING transaction to system bus");
			upBuffers[0]->trace->tranFullLat = currentClockCycle - upBuffers[0]->trace->tranTransmitTime;
			if(upBuffers[0]->CMD == RD_RS) {
				upBuffers[0]->trace->statis->hmcTransmitSize += (upBuffers[0]->LNG - 1)*16;
			}
			int packetLNG = upBuffers[0]->LNG;
			delete upBuffers[0]->trace;
			delete upBuffers[0];
			upBuffers.erase(upBuffers.begin(), upBuffers.begin()+packetLNG);
		}
	}

	Step();
}

//
//Convert transaction into packet-based protocol (FLITs) where the packets consist of 128-bit flow units
//
Packet *HMCController::ConvTranIntoPacket(Transaction *tran)
{
	unsigned packetLength;
	unsigned reqDataSize=16;
	PacketCommandType cmdtype;
	
	switch(tran->transactionType) {
		case DATA_READ:
			packetLength = 1; //header + tail
			reqDataSize = tran->dataSize;
			switch(tran->dataSize) {
				case 16:	cmdtype = RD16;		break;
				case 32:	cmdtype = RD32;		break;
				case 48:	cmdtype = RD48;		break;
				case 64:	cmdtype = RD64;		break;
				case 80:	cmdtype = RD80;		break;
				case 96:	cmdtype = RD96;		break;
				case 112:	cmdtype = RD112;	break;
				case 128:	cmdtype = RD128;	break;
				case 256:	cmdtype = RD256;	break;
				default:
					ERROR(header<<"  == Error - WRONG transaction data size  (CurrentClock : "<<currentClockCycle<<")");
					exit(0);
			}
			break;
		case DATA_WRITE:		
			packetLength = tran->dataSize /*[byte] Size of data*/ / 16 /*packet 16-byte*/ + 1 /*header + tail*/;
			reqDataSize = tran->dataSize;
			switch(tran->dataSize) {
				case 16:	cmdtype = WR16;		break;
				case 32:	cmdtype = WR32;		break;
				case 48:	cmdtype = WR48;		break;
				case 64:	cmdtype = WR64;		break;
				case 80:	cmdtype = WR80;		break;
				case 96:	cmdtype = WR96;		break;
				case 112:	cmdtype = WR112;	break;
				case 128:	cmdtype = WR128;	break;
				case 256:	cmdtype = WR256;	break;
				default:
					ERROR(header<<"  == Error - WRONG transaction data size  (CurrentClock : "<<currentClockCycle<<")");
					exit(0);
			}
			break;
		//Arithmetic atomic
		case ATM_2ADD8:		cmdtype = _2ADD8;	packetLength = 2;	break;
		case ATM_ADD16:		cmdtype = ADD16;	packetLength = 2;	break;
		case ATM_P_2ADD8:	cmdtype = P_2ADD8;	packetLength = 2;	break;
		case ATM_P_ADD16:	cmdtype = P_ADD16;	packetLength = 2;	break;
		case ATM_2ADDS8R:	cmdtype = _2ADDS8R;	packetLength = 2;	break;
		case ATM_ADDS16R:	cmdtype = ADDS16R;	packetLength = 2;	break;
		case ATM_INC8:		cmdtype = INC8;		packetLength = 1;	break;
		case ATM_P_INC8:	cmdtype = P_INC8;	packetLength = 1;	break;
		//Boolean atomic
		case ATM_XOR16:		cmdtype = XOR16;	packetLength = 2;	break;
		case ATM_OR16:		cmdtype = OR16;		packetLength = 2;	break;
		case ATM_NOR16:		cmdtype = NOR16;	packetLength = 2;	break;
		case ATM_AND16:		cmdtype = AND16;	packetLength = 2;	break;
		case ATM_NAND16:	cmdtype = NAND16;	packetLength = 2;	break;
		//Comparison atomic
		case ATM_CASGT8:	cmdtype = CASGT8;	packetLength = 2;	break;
		case ATM_CASLT8:	cmdtype = CASLT8;	packetLength = 2;	break;
		case ATM_CASGT16:	cmdtype = CASGT16;	packetLength = 2;	break;
		case ATM_CASLT16:	cmdtype = CASLT16;	packetLength = 2;	break;
		case ATM_CASEQ8:	cmdtype = CASEQ8;	packetLength = 2;	break;
		case ATM_CASZERO16:	cmdtype = CASZERO16;packetLength = 2;	break;
		case ATM_EQ16:		cmdtype = EQ16;		packetLength = 2;	break;
		case ATM_EQ8:		cmdtype = EQ8;		packetLength = 2;	break;
		//Bitwise atomic
		case ATM_BWR:		cmdtype = BWR;		packetLength = 2;	break;
		case ATM_P_BWR:		cmdtype = P_BWR;	packetLength = 2;	break;
		case ATM_BWR8R:		cmdtype = BWR8R;	packetLength = 2;	break;
		case ATM_SWAP16:	cmdtype = SWAP16;	packetLength = 2;	break;
			
		default:
			ERROR(header<<"   == Error - WRONG transaction type  (CurrentClock : "<<currentClockCycle<<")");
			ERROR(*tran);
			exit(0);
			break;
	}
	//packet, cmd, addr, cub, lng, *lat
	Packet *newPacket = new Packet(REQUEST, cmdtype, tran->address, 0, packetLength, tran->trace);
	newPacket->reqDataSize = reqDataSize;
	return newPacket;
}

//
//Print current state in state log file
//
void HMCController::PrintState()
{
	if(downBuffers.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN("Down ");
		for(int i=0; i<downBufferMax; i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(i < downBuffers.size()) {
				STATEN(*downBuffers[i]);
			}
			else if(i == downBufferMax-1) {
				STATEN("[ - ]");
			}
			else {
				STATEN("[ - ]...");
				break;
			}
		}
		STATEN(endl);
	}
	
	if(upBuffers.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN(" Up  ");
		int realInd = 0;
		for(int i=0; i<upBufferMax; i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(i < upBuffers.size()) {
				if(upBuffers[i] != NULL)	realInd = i;
				STATEN(*upBuffers[realInd]);
			}
			else if(i == upBufferMax-1) {
				STATEN("[ - ]");
			}
			else {
				STATEN("[ - ]...");
				break;
			}
		}
		STATEN(endl);
	}
}

} //namespace CasHMC