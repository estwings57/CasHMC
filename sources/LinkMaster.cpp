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

#include "LinkMaster.h"

namespace CasHMC
{

LinkMaster::LinkMaster(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down):
	SingleVectorObject<Packet>(debugOut_, stateOut_, MAX_LINK_BUF, down),
	linkMasterID(id)
{
	classID << linkMasterID;
	header = "   (LM_";
	header += downstream ? "D" : "U";
	header += classID.str() + ")";
	
	tokenCount = MAX_LINK_BUF;
	lastestRRP = 0;
	linkP = NULL;
	localLinkSlave = NULL;
	masterSEQ = 0;
	countdownCRC = 0;
	startCRC = false;
	retryStartPacket = NULL;
	readyStartRetry = false;
	startRetryTimer = false;
	retryTimer = 0;
	retryAttempts = 1;
	retBufReadP = 0;
	retBufWriteP = 0;
	retrainTransit = 0;
	firstNull = true;
	
	retryBuffers = vector<Packet *>(MAX_RETRY_BUF, NULL);
}

LinkMaster::~LinkMaster()
{
	backupBuffers.clear(); 
	retryBuffers.clear(); 
}

//
//Callback receiving packet result
//
void LinkMaster::CallbackReceive(Packet *packet, bool chkReceive)
{
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"packet buffer FULL");
	}*/
}

//
//Retry read pointer is updated as extracted RRP that means successful transmission of the associated packet
//
void LinkMaster::UpdateRetryPointer(Packet *packet)
{
	if(retBufReadP != packet->RRP) {
		do {
			if(retryBuffers[retBufReadP] != NULL) {
				delete retryBuffers[retBufReadP];
			}
			retryBuffers[retBufReadP] = NULL;
			retBufReadP++;
			retBufReadP = (retBufReadP < MAX_RETRY_BUF) ? retBufReadP : retBufReadP - MAX_RETRY_BUF;
		} while(retBufReadP != packet->RRP);
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer READ POINTER is updated ("<<retBufReadP<<")");
	}
}

//
//Extracted FRP is transmitted to the opposite side of the link
//
void LinkMaster::ReturnRetryPointer(Packet *packet)
{
	lastestRRP = packet->FRP;
	if(Buffers.size() != 0) {
		Buffers[0]->RRP = lastestRRP;
		DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"RRP ("<<lastestRRP<<") is embedded in this packet");
	}
	else {
		//packet, cmd, addr, cub, lng, *lat
		Packet *packetPRET = new Packet(FLOW, PRET, 0, 0, 1, NULL);
		packetPRET->TAG = packet->TAG;
		packetPRET->RRP = lastestRRP;
		if(downstream)	packetPRET->bufPopDelay = 0;
		linkRxTx.push_back(packetPRET);
		DEBUG(ALI(18)<<header<<ALI(15)<<*packetPRET<<(downstream ? "Down) " : "Up)   ")<<"MAKING PRET packet to be embedded RRP ("<<lastestRRP<<")");
	}
}

//
//Extracted RTC is added to the current value of the token count register
//
void LinkMaster::UpdateToken(Packet *packet)
{
	if(packet->RTC != 0) {
		tokenCount += packet->RTC;
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"Extracted RTC : "<<packet->RTC<<"  TOKEN COUNT register : "<<tokenCount);
		if(tokenCount > MAX_LINK_BUF) {
			ERROR(header<<"  == Error - TOKEN COUNT register is over the maximum  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
	}
}	

//
//Token count is returned in the return token count (RTC) field of the next possible packet traveling
//
void LinkMaster::ReturnTocken(Packet *packet)
{
	int i, extRTC = packet->LNG;
	bool findPacket = false;
	for(i=0; i<Buffers.size(); i++) {
		if(Buffers[i] != NULL && Buffers[i]->CMD != PRET && Buffers[i]->CMD != IRTRY) {
			if(Buffers[i]->LNG <= tokenCount) {
				findPacket = true;
			}
			break;
		}
	}
	if(findPacket) {
		if(Buffers[i]->RTC == 0) {
			DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[i]<<(downstream ? "Down) " : "Up)   ")<<"RTC ("<<extRTC<<") is embedded in this packet (from "<<*packet<<")");
		}
		else {
			DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[i]<<(downstream ? "Down) " : "Up)   ")<<"RTC ("<<Buffers[i]->RTC<<" + "<<extRTC<<") is embedded in this packet (from "<<*packet<<")");
		}
		Buffers[i]->RTC += extRTC;
	}
	else {
		//packet, cmd, addr, cub, lng, *lat
		Packet *packetTRET = new Packet(FLOW, TRET, 0, 0, 1, NULL);
		packetTRET->TAG = packet->TAG;
		packetTRET->RTC = extRTC;
		if(downstream)	packetTRET->bufPopDelay = 0;
		
		if(currentState == LINK_RETRY) {
			backupBuffers.push_back(packetTRET);
		}
		else {
			if(startCRC == true) {
				Buffers.insert(Buffers.begin()+Buffers[0]->LNG, packetTRET);
			}
			else {
				Buffers.insert(Buffers.begin(), packetTRET);
			}
		}
		DEBUG(ALI(18)<<header<<ALI(15)<<*packetTRET<<(downstream ? "Down) " : "Up)   ")<<"MAKING TRET packet to be embedded RTC ("<<extRTC<<")");
	}
}

//
//When local link slave detects an error, StartRetry sequence is initiated
// (If link master is in LinkRetry state, finish the LinkRetry state and allow all packets to be retransmitted 
//  before sending a stream of IRTRY packets with the StartRetry flag set.)
//
void LinkMaster::StartRetry(Packet *packet)
{
	startRetryTimer = true;
	retryTimer = 0;
	for(int i=0; i<NUM_OF_IRTRY; i++) {
		if(currentState == LINK_RETRY) {
			readyStartRetry = true;
		}
		else {
			//packet, cmd, addr, cub, lng, *lat
			Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL);
			packetIRTRY->TAG = packet->TAG;
			packetIRTRY->RRP = lastestRRP;
			packetIRTRY->FRP = 1;		//StartRetry flag is set with FRP[0] = 1
			if(downstream)	packetIRTRY->bufPopDelay = 0;
			linkRxTx.insert(linkRxTx.begin(), packetIRTRY);
			currentState = START_RETRY;
			header.erase(header.find(")"));
			header += ") ST_RT";
		}
	}
	if(currentState == LINK_RETRY) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"START RETRY sequence is delayed until LINK RETRY");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"START RETRY sequence is initiated");
	}
}

//
//It is an indication that the link slave on the other end of the link detected an error 
// and send the packets currently being saved for retransmission.
//
void LinkMaster::LinkRetry(Packet *packet)
{
	//linkRxTx and token count reset (packets in linkRxTx are already saved in retry buffer)
	startCRC = false;
	for(int i=0; i<NUM_OF_IRTRY; i++) {
		//packet, cmd, addr, cub, lng, *lat
		Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL);
		packetIRTRY->TAG = packet->TAG;
		packetIRTRY->RRP = lastestRRP;
		packetIRTRY->FRP = 2;		//ClearErrorAbort flag is set with FRP[1] = 1
		if(downstream)	packetIRTRY->bufPopDelay = 0;
		linkRxTx.push_back(packetIRTRY);
	}
	
	//Initialize sequence number and backup buffer packets
	masterSEQ = 0;
	for(int i=0; i<Buffers.size(); i++) {
		backupBuffers.push_back(Buffers[i]);
	}
	Buffers.clear();
	
	//Transmits the packets currently being saved for retransmission
	unsigned retryToken = 0;
	unsigned tempReadP = retBufReadP;
	if(retryBuffers[retBufReadP] == NULL) {
		ERROR(header<<"  == Error - The first retry packet is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
		exit(0);
	}
	if(retBufReadP != retBufWriteP) {
		do {
			Packet *retryPacket = NULL;
			if(retryBuffers[tempReadP] != NULL) {
				retryPacket = new Packet(*retryBuffers[tempReadP]);
				if(downstream)	retryPacket->bufPopDelay = 0;
				else			retryPacket->bufPopDelay = 1;
				if(retryPacket->packetType != FLOW) {
					retryToken += retryPacket->LNG;
				}
				delete retryBuffers[tempReadP];
				retryBuffers[tempReadP] = NULL;
			}
			else {
				retryPacket = NULL;
			}
			Buffers.push_back(retryPacket);
			tempReadP++;
			tempReadP = (tempReadP < MAX_RETRY_BUF) ? tempReadP : tempReadP - MAX_RETRY_BUF;
		} while(tempReadP != retBufWriteP);
		tokenCount += retryToken;
		retBufWriteP = retBufReadP;
		currentState = LINK_RETRY;
		header.erase(header.find(")"));
		header += ") LK_RT";
	}
	else {
		ERROR(header<<"  == Error - Retry buffer has no packets to be transmitted for LINK RETRY sequence  (CurrentClock : "<<currentClockCycle<<")");
		exit(0);
	}
	DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"LINK RETRY sequence is initiated");
}

//
//Finish retry state. It is asserted when local link slave receive IRTRY2 packet.
//
void LinkMaster::FinishRetry()
{
	DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"RETRY sequence is finished");
	currentState = ACTIVE;
	header.erase(header.find(")"));
	header += ")";
	delete retryStartPacket;
	retryStartPacket = NULL;
	retryAttempts = 1;
	startRetryTimer = false;
	uint64_t retryTime;
	if(downstream) {
		retryTime = retryTimer;
	}
	else {
		retryTime = ceil(retryTimer * (double)tCK/CPU_CLK_PERIOD);
	}
	linkP->statis->errorRetryLat.push_back(retryTime);
	retryTimer = 0;
}

//
//Update the state of link master
//
void LinkMaster::Update()
{
	if(Buffers.size() > 0) {
		//Make sure that buffer[0] is not virtual tail packet.
		if(Buffers[0] == NULL) {
			ERROR(header<<"  == Error - Buffers[0] is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
		else if(Buffers[0]->bufPopDelay == 0) {
			//Token count register represents the available space in link slave input buffer
			if(linkRxTx.size() == 0 && !(Buffers[0]->packetType != FLOW && tokenCount < Buffers[0]->LNG)) {
				int tempWriteP = retBufWriteP + Buffers[0]->LNG;
				if(retBufWriteP	>= retBufReadP) {
					if(tempWriteP - (int)retBufReadP < MAX_RETRY_BUF) {
						CRCCountdown(tempWriteP, Buffers[0]);
					}
					else {
						//DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
					}
				}
				else {
					if((int)retBufReadP - tempWriteP > 0) {
						CRCCountdown(tempWriteP, Buffers[0]);
					}
					else {
						//DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"Retry buffer is near-full condition");
					}
				}
			}
			else {
				//DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")<<"packet length ("<<Buffers[0]->LNG<<") is BIGGER than token count register ("<<tokenCount<<")");
			}
		}
	}

	//Finish LinkRetry state, and then backup packets return to buffers 
	if(currentState == LINK_RETRY && Buffers.empty() && linkRxTx.empty()) {
		//If StartRetry state is waiting, start StartRetry state
		if(readyStartRetry == true) {
			readyStartRetry = false;
			for(int i=0; i<NUM_OF_IRTRY; i++) {
				//packet, cmd, addr, cub, lng, *lat
				Packet *packetIRTRY = new Packet(FLOW, IRTRY, 0, 0, 1, NULL);
				packetIRTRY->TAG = retryStartPacket->TAG;
				packetIRTRY->RRP = lastestRRP;
				packetIRTRY->FRP = 1;		//StartRetry flag is set with FRP[0] = 1
				if(downstream)	packetIRTRY->bufPopDelay = 0;
				linkRxTx.push_back(packetIRTRY);
				currentState = START_RETRY;
				header.erase(header.find(")"));
				header += ") ST_RT";
			}
		}
		else {
			currentState = ACTIVE;
			header.erase(header.find(")"));
			header += ")";
		}
		//Restore backup packets to Buffers
		for(int i=0; i<backupBuffers.size(); i++) {
			Buffers.push_back(backupBuffers[i]);
		}
		backupBuffers.clear();
	}
	
	if(startRetryTimer)		retryTimer++;
	//Retry timer should be set to at least 3 times the retry buffer full period
	if(retryTimer > MAX_RETRY_BUF*4) {
		DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"Retry timer TIME-OUT  (retry attempt : "<<retryAttempts<<")");
		if(retryAttempts == RETRY_ATTEMPT_LIMIT) {
			FinishRetry();
		}
		else {
			retryAttempts++;
			StartRetry(retryStartPacket);
		}
	}
	
	//Requester/Responder issue continuous scrambled NULL packet and TS1 training sequence for link retraining
	if(currentState == RETRAIN1 && currentClockCycle >= retrainTransit && firstNull) {
		//Upon descrambler sync, the responder will begin to transmit scrambled NULL packet
		Packet *packetNULL = new Packet(FLOW, NULL_, 0, 0, 1, NULL);
		linkRxTx.push_back(packetNULL);
		DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"issue NULL packet for descrambler initializing");
		firstNull = false;
	}
	//Requester/Responder enters the TS1 state 
	// and issues the scrambled TS1 training sequence continuously to achieve FLIT synchrony
	else if(currentState == RETRAIN2 && currentClockCycle >= retrainTransit && firstNull) {
		//Because TS1 is 16-bit character, we just employ NULL packet instead of TS1
		Packet *packetTS1 = new Packet(FLOW, NULL_, 0, 0, 1, NULL);
		linkRxTx.push_back(packetTS1);
		DEBUG(ALI(33)<<header<<(downstream ? "Down) " : "Up)   ")<<"issue TS1 sequence to achieve FLIT synchrony");
		firstNull = false;
	}
	
	Step();
}

//
//CRC countdown to update packet filed
//
void LinkMaster::CRCCountdown(int writeP, Packet *packet)
{
	if(CRC_CHECK && !startCRC) {
		startCRC = true;
		countdownCRC = ceil((double)CRC_CAL_CYCLE * packet->LNG);
	}
	
	if(CRC_CHECK && countdownCRC > 0) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
				<<"WAITING CRC calculation ("<<countdownCRC<<"/"<<ceil((double)CRC_CAL_CYCLE*packet->LNG)<<")");
	}
	else {
		UpdateField(writeP, packet);
	}
	countdownCRC = (countdownCRC>0) ? countdownCRC-1 : 0;
}

//
//packet in link master downstream buffer is embedded SEQ, FRP, CRC and stored to retry buffer.
// (CRC calculation delay is hidden, because the beginning of the packet may have been forwarded 
//  before the CRC was performed at the tail to minimize latency.)
//
void LinkMaster::UpdateField(int nextWriteP, Packet *packet)
{
	//Sequence number updated
	packet->SEQ = (masterSEQ<7) ? masterSEQ++ : 0;
	//Update retry buffer pointer field
	packet->RRP = lastestRRP;
	packet->FRP = (nextWriteP < MAX_RETRY_BUF) ? nextWriteP : nextWriteP - MAX_RETRY_BUF;
	//CRC field updated
	startCRC = false;
	if(CRC_CHECK)	packet->CRC = packet->GetCRC();
	//Decrement the token count register by the number of packet in the transmitted packet
	if(packet->packetType != FLOW) {
		tokenCount -= packet->LNG;
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
			<<"Decreased TOKEN : "<<packet->LNG<<" (remaining : "<<tokenCount<<"/"<<MAX_LINK_BUF<<")");
	}
	//Save packet in retry buffer
	Packet *retryPacket = new Packet(*packet);
	if(retryBuffers[retBufWriteP] == NULL) {
		retryBuffers[retBufWriteP] = retryPacket;
	}
	else {
		ERROR(header<<"  == Error - retryBuffers["<<retBufWriteP<<"] is not NULL  (CurrentClock : "<<currentClockCycle<<")");
		exit(0);
	}
	for(int i=1; i<packet->LNG; i++) {
		retryBuffers[(retBufWriteP+i<MAX_RETRY_BUF ? retBufWriteP+i : retBufWriteP+i-MAX_RETRY_BUF)] = NULL;
	}
	DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")
			<<"RETRY BUFFER["<<retBufWriteP<<"] (FRP : "<<packet->FRP<<")");
	retBufWriteP = packet->FRP;
	//Send packet to standby buffer where the packet is ready to be transmitted
	packet->bufPopDelay = 1;
	linkRxTx.push_back(packet);
	//DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")
	//			<<"SENDING packet to link "<<linkMasterID<<" (LK_"<<(downstream ? "D" : "U")<<linkMasterID<<")");
	Buffers.erase(Buffers.begin(), Buffers.begin()+packet->LNG);
}

//
//Send a request/response QUITE packet for checking the low power mode 
//
void LinkMaster::QuitePacket()
{
	//packet, cmd, addr, cub, lng, *lat
	Packet *packetQUIET = new Packet(FLOW, QUIET, 0, 0, 1, NULL);
	packetQUIET->RRP = lastestRRP;
	DEBUG(ALI(18)<<header<<ALI(15)<<*packetQUIET<<(downstream ? "Down) " : "Up)   ")<<"sending QUITE packet");
	linkRxTx.push_back(packetQUIET);
}

//
//Finish link retraining
//
void LinkMaster::FinishRetrain()
{
	currentState = ACTIVE;
	//The requester must send a minimum of 32 NULL FLITs before sending the first nonNULL flow packet
	Packet *packetNULL = NULL;
	for(int i=0; i<32; i++) {
		packetNULL = new Packet(FLOW, NULL_, 0, 0, 1, NULL);
		linkRxTx.push_back(packetNULL);
	}
	DEBUG(ALI(18)<<header<<ALI(15)<<*packetNULL<<(downstream ? "Down) " : "Up)   ")
			<<"sending a minimum of 32 NULL FLITs before entering ACTIVE mode");
}

//
//Print current state in state log file
//
void LinkMaster::PrintState()
{
	if(Buffers.size()>0) {
		if(currentState == ACTIVE) {
			STATEN(ALI(11)<<header<<"tk:"<<ALI(3)<<tokenCount);
		//	STATEN(ALI(17)<<header);
		}
		else {
			STATEN(ALI(17)<<header);
		}
		STATEN((downstream ? "Down " : " Up  "));
		int realInd = 0;
		for(int i=0; i<bufferMax; i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(i < Buffers.size()) {
				if(Buffers[i] != NULL)	realInd = i;
				STATEN(*Buffers[realInd]);
			}
			else if(i == bufferMax-1) {
				STATEN("[ - ]");
			}
			else {
				STATEN("[ - ]...");
				break;
			}
		}
		STATEN(endl);
	}

	//Print linkTx buffer
	if(linkRxTx.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN("LKTX ");
		int realInd = 0;
		for(int i=0; i<linkRxTx.size(); i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(linkRxTx[i] != NULL)	realInd = i;
			STATEN(*linkRxTx[realInd]);
		}
		STATEN(endl);
	}

	//If retry buffer has something, print it
	if(retBufWriteP != retBufReadP) {
	//	STATEN(ALI(11)<<header<<"tk:"<<ALI(3)<<tokenCount);
		STATEN(ALI(17)<<header);
		STATEN("RTRY ");
		int retryInd = retBufReadP;
		int realInd = retBufReadP, i=0;
		while(retryInd != retBufWriteP) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(retryBuffers[retryInd] != NULL) {
				realInd = retryInd;
				STATEN("("<<retryInd<<")");
			}
			STATEN(*retryBuffers[realInd]);
			retryInd++;
			retryInd = (retryInd < MAX_RETRY_BUF) ? retryInd : retryInd - MAX_RETRY_BUF;
			i++;
		}
		STATEN(endl);
	}

	//Print backup buffer during LinkRetry state
	if(backupBuffers.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN("BACK ");
		int realInd = 0;
		for(int i=0; i<backupBuffers.size(); i++) {
			if(i>0 && i%8==0) {
				STATEN(endl<<"                      ");
			}
			if(backupBuffers[i] != NULL)	realInd = i;
			STATEN(*backupBuffers[realInd]);
		}
		STATEN(endl);
	}
}


//
//Determines which link should be used to receive a request
//
int FindAvailableLink(int &link, vector<LinkMaster *> &LM)
{
	switch(LINK_PRIORITY) {
		case ROUND_ROBIN:
			if(++link >= NUM_LINKS)
				link=0;
			return link;
			break;
		case BUFFER_AWARE:
			unsigned minBufferSize = MAX_LINK_BUF;
			unsigned minBufferLink = 0;
			for(int l=0; l<NUM_LINKS; l++) {
				if(LM[l]->currentState != ACTIVE && LM[l]->currentState != LINK_RETRY) {
					continue;
				}
				else {
					int bufSizeTemp = LM[l]->Buffers.size();
					for(int i=0; i<LM[l]->linkRxTx.size(); i++) {
						if(LM[l]->linkRxTx[i] != NULL) {
							bufSizeTemp += LM[l]->linkRxTx[i]->LNG;
						}
					}
					if(bufSizeTemp < minBufferSize) {
						minBufferSize = bufSizeTemp;
						minBufferLink = l;
					}
				}
			}
			return minBufferLink;
			break;
	}
	return -1;
}

} //namespace CasHMC