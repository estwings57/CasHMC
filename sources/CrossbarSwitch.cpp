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

#include "CrossbarSwitch.h"

namespace CasHMC
{
	
CrossbarSwitch::CrossbarSwitch(ofstream &debugOut_, ofstream &stateOut_):
	DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_CROSS_BUF, MAX_CROSS_BUF)
{
	header = "        (CS)";
	
	inServiceLink = -1;
	bufOverflow = false;

	downBufferDest = vector<DualVectorObject<Packet, Packet> *>(NUM_VAULTS, NULL);
	upBufferDest = vector<LinkMaster *>(NUM_LINKS, NULL);
}

CrossbarSwitch::~CrossbarSwitch()
{	
	if(bufOverflow) {
		ERROR(" (CS) ## WARNING ## Crossbar switch buffer overflowed  (max size : "<<maxSize<<")"<<endl);
	}
	downBufferDest.clear();
	upBufferDest.clear();
}

//
//Callback Adding packet
//
void CrossbarSwitch::CallbackReceiveDown(Packet *downEle, bool chkReceive)
{
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
	}*/
}
void CrossbarSwitch::CallbackReceiveUp(Packet *upEle, bool chkReceive)
{
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*upEle<<"Up)   packet buffer FULL");
	}*/
}

//
//Update the state of crossbar switch
//
void CrossbarSwitch::Update()
{	
	//Downstream buffer state
	if(bufPopDelay == 0) {
		for(int i=0; i<downBuffers.size(); i++) {
			if(downBuffers[i] != NULL) {
				int vaultMap = (downBuffers[i]->ADRS >> _log2(ADDRESS_MAPPING)) & (NUM_VAULTS-1);;
				if(downBufferDest[vaultMap]->ReceiveDown(downBuffers[i])) {
					DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[i]<<"Down) SENDING packet to vault controller "<<vaultMap<<" (VC_"<<vaultMap<<")");
					int tempLNG = downBuffers[i]->LNG;
					downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
					i--;
				}
				else {
					//DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[i]<<"Down) Vault controller buffer FULL");	
				}
			}
		}
	}
	
	//Upstream buffer state
	for(int i=0; i<upBuffers.size(); i++) {
		if(upBuffers[i] != NULL) {
			for(int l=0; l<NUM_LINKS; l++) {
				int link = FindAvailableLink(inServiceLink, upBufferDest);
				if(link == -1) {
					//DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<"Up)   all packet buffer FULL");
				}
				else if(upBufferDest[link]->currentState != LINK_RETRY) {
					if(upBufferDest[link]->Receive(upBuffers[i])) {
						DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<"Up)   SENDING packet to link master "<<link<<" (LM_U"<<link<<")");
						upBuffers.erase(upBuffers.begin()+i, upBuffers.begin()+i+upBuffers[i]->LNG);
						i--;
						break;
					}
					else {
						//DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[i]<<"Up)   Link master buffer FULL");
					}
				}
			}
		}
	}
	if(bufOverflow) {
		unsigned bufSize = upBuffers.size();
		maxSize = max(maxSize, bufSize);
	}
	
	Step();
}

//
//Print current state in state log file
//
void CrossbarSwitch::PrintState()
{
	int realInd = 0;
	if(downBuffers.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN("Down ");
		for(int i=0; i<downBufferMax; i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(i < downBuffers.size()) {
				if(downBuffers[i] != NULL)	realInd = i;
				STATEN(*downBuffers[realInd]);
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
		if(upBuffers.size() < upBufferMax) {
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
		}
		else {
			for(int i=0; i<upBuffers.size(); i++) {
				if(i>0 && i%8==0) {
					STATEN(endl<<"                      ");
				}
				if(upBuffers[i] != NULL)	realInd = i;
				STATEN(*upBuffers[realInd]);
			}
		}
		STATEN(endl);
	}
}

} //namespace CasHMC
