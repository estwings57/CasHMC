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

#include "Link.h"
#include "LinkMaster.h"

namespace CasHMC
{
	
Link::Link(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down, TranStatistic *statisP):
	SimulatorObject(debugOut_, stateOut_),
	linkID(id),
	downstream(down),
	statis(statisP)
{
	classID << linkID;
	header = "   (LK_";
	header += downstream ? "D" : "U";
	header += classID.str() + ")";
	
	linkMasterP = NULL;
	linkSlaveP = NULL;
	inFlightPacket = NULL;
	inFlightCountdown = 0;
	
	//The probability of at least one bit error
	//  (pow(10, -LINK_BER) - 1)/pow(10, -LINK_BER) : Probability of no error in one bit
	//  (1 - pow((pow(10, -LINK_BER) - 1)/pow(10, -LINK_BER), 128)) : Probability of at least one bit error in 128 bits
	//  errorProba is square root of bits size per one bit error, because errorProba is over than variable capacity.
	//  Link error will be calculated two times by errorProba respectively
	errorProba = sqrt(1 / (1 - pow((pow(10, -LINK_BER) - 1)/pow(10, -LINK_BER), 128)));
}

Link::~Link()
{
}

//
//Update the state of link
//
void Link::Update(bool lastUpdate)
{
	//Transmit packet from buffer
	if(linkMasterP->linkRxTx.size()>0 && linkMasterP->linkRxTx[0]->bufPopDelay==0 && inFlightPacket==NULL) {
		if(linkMasterP->currentState == NORMAL || linkMasterP->currentState == LINK_RETRY ||
		(linkMasterP->currentState == START_RETRY && linkMasterP->linkRxTx[0]->CMD == IRTRY && linkMasterP->linkRxTx[0]->FRP == 1)) {
			inFlightPacket = linkMasterP->linkRxTx[0];
			UpdateStatistic(inFlightPacket);
			inFlightCountdown = (inFlightPacket->LNG * 128) / LINK_WIDTH;
			DEBUG(ALI(18)<<header<<ALI(15)<<*inFlightPacket<<(downstream ? "Down) " : "Up)   ")<<"START transmission packet");
			linkMasterP->linkRxTx.erase(linkMasterP->linkRxTx.begin());
		}
	}

	if(inFlightCountdown > 0) {
		inFlightCountdown--;
		//Packet transmission done
		if(inFlightCountdown == 0) {
			NoisePacket(inFlightPacket);
			DEBUG(ALI(18)<<header<<ALI(15)<<*inFlightPacket<<(downstream ? "Down) " : "Up)   ")<<"DONE transmission packet");
			inFlightPacket->bufPopDelay = 1;
			linkSlaveP->linkRxTx.push_back(inFlightPacket);
			if(inFlightPacket->packetType == RESPONSE)
				inFlightPacket->trace->linkFullLat = linkSlaveP->currentClockCycle - inFlightPacket->trace->linkTransmitTime;
			inFlightPacket = NULL;
		}
		else if(lastUpdate) {
			DEBUG(ALI(18)<<header<<ALI(15)<<*inFlightPacket<<(downstream ? "Down) " : "Up)   ")<<"Link transmiting countdown : "<<ALI(2)<<inFlightCountdown);
		}
	}

	if(lastUpdate && linkMasterP->linkRxTx.size()>0) {
		for(int i=0; i<linkMasterP->linkRxTx.size(); i++) {
			linkMasterP->linkRxTx[i]->bufPopDelay = (linkMasterP->linkRxTx[i]->bufPopDelay>0) ? linkMasterP->linkRxTx[i]->bufPopDelay-1 : 0;
		}
	}
	Step();
}

//
//Update the statistic of transmitting FILTs
//
void Link::UpdateStatistic(Packet *packet)
{
	if(downstream) {
		statis->downLinkTransmitSize[linkID] += packet->LNG * 16;
		if(packet->LNG > 1) {
			statis->downLinkDataSize[linkID] += (packet->LNG-1) * 16;
		}
	}
	else {
		statis->upLinkTransmitSize[linkID] += packet->LNG * 16;
		if(packet->LNG > 1) {
			statis->upLinkDataSize[linkID] += (packet->LNG-1) * 16;
		}
	}
	
	switch(packet->packetType) {
		case REQUEST:
			statis->reqPerLink[linkID]++;
			packet->trace->linkTransmitTime = linkMasterP->currentClockCycle;

			if((WR16 <= packet->CMD && packet->CMD <= MD_WR) || packet->CMD == WR256
			|| (P_WR16 <= packet->CMD && packet->CMD <= P_WR128) || packet->CMD == P_WR256) {
				statis->writePerLink[linkID]++;
			}
			else if((RD16 <= packet->CMD && packet->CMD <= RD128) || packet->CMD == RD256 || packet->CMD == MD_RD) {
				statis->readPerLink[linkID]++;
			}
			else {
				statis->atomicPerLink[linkID]++;
			}
			break;
		case RESPONSE:
			statis->resPerLink[linkID]++;
			break;
		case FLOW:
			statis->flowPerLink[linkID]++;
			break;
		default:
			ERROR(header<<"  == Error - Unknown packet type");
			ERROR("         Type : "<<packet->packetType);
			exit(0);
	}
}

//
//Make link noise on transmitting packet
//
void Link::NoisePacket(Packet *packet)
{
	for(int i=0; i<packet->LNG; i++) {
		unsigned ranNum1 = rand();
		unsigned ranNum2 = rand();	
		if(ranNum1%errorProba == 0 && ranNum2%errorProba == 0) {
			DE_CR(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"====> Link ERROR is occurred <====");
			packet->CRC = ~packet->CRC;
			break;
		}
	}
}

//
//Print current state in state log file
//
void Link::PrintState()
{
	if(inFlightPacket != NULL) {
		STATEN(ALI(17)<<header);
		STATEN((downstream ? "Down " : " Up  "));
		STATEN(*inFlightPacket);
		STATE("*"<<inFlightPacket->LNG);
	}
}

} //namespace CasHMC