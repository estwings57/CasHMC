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

#include "HMCController.h"

using namespace std;

namespace CasHMC
{
	
HMCController::HMCController(ofstream &debugOut_, ofstream &stateOut_):
	DualVectorObject<Transaction, Packet>(debugOut_, stateOut_, MAX_REQ_BUF, MAX_LINK_BUF),
	readDone(NULL),
	writeDone(NULL)
{
	header = " (HC)";
	
	inServiceLink = -1;
	maxLinkBand = LINK_WIDTH * LINK_SPEED / 8;
	linkEpochCycle = ceil((double)LINK_EPOCH*1000000/CPU_CLK_PERIOD);
	requestAccLNG = 0;
	responseAccLNG = 0;
	sleepLink = 0;
	alloMSHR = 0;
	accuMSHR = 0;
	returnTransCnt = 0;
	quiesceClk = ceil((double)tQUIESCE/CPU_CLK_PERIOD);
	staggerSleep = 0;
	staggerActive = 0;
	staySREF = 0;
	stayActive = vector<uint64_t>(NUM_LINKS, ceil((double)tOP/CPU_CLK_PERIOD));
	quiesceLink = vector<uint64_t>(NUM_LINKS, quiesceClk);
	staggeringSleep = vector<bool>(NUM_LINKS, false);
	staggeringActive = vector<bool>(NUM_LINKS, false);
	modeTransition = vector<uint64_t>(NUM_LINKS, 0);
	
	linkSleepTime = vector<uint64_t>(NUM_LINKS, 0);
	linkDownTime = vector<uint64_t>(NUM_LINKS, 0);
	
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
//Register read and write callback funtions
//
void HMCController::RegisterCallbacks(TransCompCB *readCB, TransCompCB *writeCB)
{
	readDone = readCB;
	writeDone = writeCB;
}

//
//Callback receiving packet result
//
void HMCController::CallbackReceiveDown(Transaction *downEle, bool chkReceive)
{
	if(chkReceive) {
		//Check transaction address
		downEle->address &= ((uint64_t)NUM_VAULTS*NUM_BANKS*NUM_COLS*NUM_ROWS - 1);
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

bool HMCController::CanAcceptTran(void)
{
	if(downBuffers.size() + 1 <= downBufferMax) {
		return true;
	}
	else {
		return false;
	}
}

//
//Update the state of HMC controller
// Link master and slave are separately updated to flow packet from master to slave regardless of downstream or upstream
//
void HMCController::Update()
{
	//Link wakes up from low power mode
	LinkPowerExitManager();
	
	//accumulate the allocated MSHR size
	accuMSHR += alloMSHR;
	
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
				else if(downLinkMasters[link]->currentState != ACTIVE
				&& downLinkMasters[link]->currentState != LINK_RETRY) {
					continue;
					//DEBUG(ALI(18)<<header<<ALI(15)<<*downBuffers[0]<<"Down) link "<<l<<" is not ACTIVE mode ["<<downLinkMasters[link]->powerMode<<"]");
				}
				else {
					Packet *packet = ConvTranIntoPacket(downBuffers[0]);
					if(downLinkMasters[link]->Receive(packet)) {
						DE_CR(ALI(18)<<header<<ALI(15)<<*packet<<"Down) SENDING packet to link mater "<<link<<" (LM_D"<<link<<")");
						if(!((packet->CMD >= P_WR16 && packet->CMD <= P_WR128)
						|| packet->CMD == P_WR256 || packet->CMD == P_2ADD8
						|| packet->CMD == P_ADD16 || packet->CMD == P_INC8
						|| packet->CMD == P_BWR)) {
							returnTransCnt++;
						}
						//Call callback function if posted write packet is tranmitted
						if((packet->CMD >= P_WR16 && packet->CMD <= P_WR128) || packet->CMD == P_WR256) {
							if(writeDone != NULL) {
								(*writeDone)(packet->ADRS, currentClockCycle);
							}
						}
						requestAccLNG += packet->LNG;
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
			returnTransCnt--;
			//Call callback function if it is registered
			if(upBuffers[0]->CMD == WR_RS) {
				if(writeDone != NULL) {
					(*writeDone)(upBuffers[0]->ADRS, currentClockCycle);
				}
			}
			else if(upBuffers[0]->CMD == RD_RS) {
				if(readDone != NULL) {
					(*readDone)(upBuffers[0]->ADRS, currentClockCycle);
				}
			}
			int packetLNG = upBuffers[0]->LNG;
			responseAccLNG += packetLNG;
			delete upBuffers[0]->trace;
			delete upBuffers[0];
			upBuffers.erase(upBuffers.begin(), upBuffers.begin()+packetLNG);
		}
	}
	
	//Link state manager
	LinkPowerStateManager();
	//Link goes to low power mode
	LinkPowerEntryManager();
	
	LinkStatistic();
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
//Link power state entry manager
//
void HMCController::LinkPowerEntryManager()
{
	switch(LINK_POWER) {
		case NO_MANAGEMENT:{
			break;
		}
		case QUIESCE_SLEEP:{
			//Check for last active link
			bool lastActive = false;
			for(int l=0; l<NUM_LINKS; l++) {
				if(downLinkMasters[l]->currentState == ACTIVE) {
					if(!lastActive) {
						lastActive = true;
					}
					else {
						lastActive = false;
						break;
					}
				}
			}
				
			for(int l=0; l<NUM_LINKS; l++) {
				if(downLinkMasters[l]->currentState == ACTIVE) {
					//Setting the quiesce link traffic cycle to enter low power mode
					if(downLinkMasters[l]->Buffers.size() > 0
					|| downLinkMasters[l]->retBufReadP != downLinkMasters[l]->retBufWriteP
					|| downLinkMasters[l]->tokenCount < MAX_LINK_BUF) {
						quiesceLink[l] = currentClockCycle + quiesceClk;
					}
					else if(currentClockCycle >= quiesceLink[l] && currentClockCycle >= stayActive[l]) {
						//In order to prevent from blocking response packet to HMC controller 
						if(!lastActive || returnTransCnt == 0) {
							//Check the opposite link state for entering low power mode
							downLinkMasters[l]->currentState = WAIT;
							downLinkMasters[l]->QuitePacket();
							staySREF = currentClockCycle + ceil((double)(tSREF)/CPU_CLK_PERIOD);
						}
					}
				}
				else if(downLinkMasters[l]->currentState == CONFIRM) {
					//Stagger transition between active mode and sleep modes in order to avoid supply voltage shifts
					if(currentClockCycle >= staggerSleep) {
						//When the last link exits active mode, links are transitioned to down mode to further reduce power consumption
						bool lastExit = true;
						for(int k=0; k<NUM_LINKS; k++) {
							if(l != k && !(downLinkMasters[k]->currentState == SLEEP || downLinkMasters[k]->currentState == TRANSITION_TO_SLEEP)) {
								lastExit = false;
								break;
							}
						}
						
						if(lastExit) {
							uint64_t tran = 0;
							if(staggeringSleep[l]) {
								staggeringSleep[l] = false;
								tran = ceil((double)(tSS+tSD)/CPU_CLK_PERIOD);
							}
							else {
								tran = ceil((double)(tPST+tSD)/CPU_CLK_PERIOD);
							}
							modeTransition[l] = currentClockCycle + tran;
							downLinkMasters[l]->currentState = TRANSITION_TO_DOWN;
							DEBUG(ALI(33)<<header<<"Down) link DOWN MODE transition after "<<modeTransition[l]<<" cycle");
						}
						//Link sleep mode entry
						else {
							uint64_t stag = 0;
							uint64_t tran = 0;
							if(staggeringSleep[l]) {
								staggeringSleep[l] = false;
								stag = ceil((double)tSS/CPU_CLK_PERIOD);
								tran = ceil((double)(tSS+tSME)/CPU_CLK_PERIOD);
							}
							else {
								stag = ceil((double)(tPST+tSS)/CPU_CLK_PERIOD);
								tran = ceil((double)(tPST+tSME)/CPU_CLK_PERIOD);
							}
							staggerSleep = currentClockCycle + stag;
							modeTransition[l] = currentClockCycle + tran;
							downLinkMasters[l]->currentState = TRANSITION_TO_SLEEP;
							DEBUG(ALI(33)<<header<<"Down) link SLEEP MODE transition after "<<modeTransition[l]<<" cycle (stagger : "<<staggerSleep<<") (LM_D"<<l<<")");
						}
					}
					else {
						staggeringSleep[l] = true;
					}
				}
				//Sleep mode entry
				else if(downLinkMasters[l]->currentState == TRANSITION_TO_SLEEP) {
					if(currentClockCycle >= modeTransition[l]) {
						downLinkMasters[l]->currentState = SLEEP;
						DE_CR(ALI(33)<<header<<"Down) entering link SLEEP MODE (LM_D"<<l<<")");
					}
				}
				//Down mode entry
				else if(downLinkMasters[l]->currentState == TRANSITION_TO_DOWN) {
					if(currentClockCycle >= modeTransition[l]) {
						for(int k=0; k<NUM_LINKS; k++) {
							downLinkMasters[k]->currentState = DOWN;
						}
						DE_CR(ALI(33)<<header<<"Down) entering link DOWN MODE");
					}
				}
			}
			break;
		}
		case MSHR:
		case LINK_MONITOR:
		case AUTONOMOUS:{
			//Transition links to sleep mode as many as sleepLink
			for(int l=NUM_LINKS - 1; l>=NUM_LINKS-sleepLink; l--) {
				if(downLinkMasters[l]->currentState == ACTIVE) {
					//The condition to enter sleep mode
					if(downLinkMasters[l]->Buffers.size() == 0
					&& downLinkMasters[l]->retBufReadP == downLinkMasters[l]->retBufWriteP
					&& downLinkMasters[l]->tokenCount == MAX_LINK_BUF
					&& currentClockCycle >= stayActive[l]) {
						//Check the opposite link state for entering low power mode
						downLinkMasters[l]->currentState = WAIT;
						downLinkMasters[l]->QuitePacket();
						staySREF = currentClockCycle + ceil((double)(tSREF)/CPU_CLK_PERIOD);
					}
				}
				else if(downLinkMasters[l]->currentState == CONFIRM) {
					//Stagger transition between active mode and sleep modes in order to avoid supply voltage shifts
					if(currentClockCycle >= staggerSleep) {
						uint64_t stag = 0;
						uint64_t tran = 0;
						if(staggeringSleep[l]) {
							staggeringSleep[l] = false;
							stag = ceil((double)tSS/CPU_CLK_PERIOD);
							tran = ceil((double)tSME/CPU_CLK_PERIOD);
						}
						else {
							stag = ceil((double)(tPST+tSS)/CPU_CLK_PERIOD);
							tran = ceil((double)(tPST+tSME)/CPU_CLK_PERIOD);
						}
						staggerSleep = currentClockCycle + stag;
						modeTransition[l] = currentClockCycle + tran;
						downLinkMasters[l]->currentState = TRANSITION_TO_SLEEP;
						DEBUG(ALI(33)<<header<<"Down) link SLEEP MODE transition after "<<modeTransition[l]<<" cycle (stagger : "<<staggerSleep<<") (LM_D"<<l<<")");
					}
					else {
						staggeringSleep[l] = true;
					}
				}
				//Sleep mode entry
				else if(downLinkMasters[l]->currentState == TRANSITION_TO_SLEEP) {
					if(currentClockCycle >= modeTransition[l]) {
						downLinkMasters[l]->currentState = SLEEP;
						DE_CR(ALI(33)<<header<<"Down) entering link SLEEP MODE (LM_D"<<l<<")");
					}
				}
			}
			break;
		}
		default:{
			ERROR(header<<"   == Error - WRONG LINK_POWER parameter  (LINK_POWER : "<<LINK_POWER<<")");
			exit(0);
			break;
		}
	}
}

//
//Link power state manager
//
void HMCController::LinkPowerStateManager()
{
	switch(LINK_POWER) {
		case NO_MANAGEMENT:
		case QUIESCE_SLEEP:{
			break;
		}
		case MSHR:{
			linkEpochCycle--;
			//control the number of active link in every one epoch
			if(linkEpochCycle == 0) {
				//The average allocated MSHR size
				double aveMSHR = accuMSHR/((double)LINK_EPOCH*1000000/CPU_CLK_PERIOD);
				aveMSHR *= MSHR_SCALING;
				
				if(aveMSHR >= 4) {
					sleepLink = 0;
				}
				else if(aveMSHR >= 3) {
					sleepLink = 1;
				}
				else if(aveMSHR >= 2) {
					sleepLink = 2;
				}
				else {
					sleepLink = 3;
				}
				DEBUG(ALI(33)<<header<<"Down) epoch average MSHR size : "<<aveMSHR);
				
				//link epoch cycle reset
				accuMSHR = 0;
				linkEpochCycle = ceil((double)LINK_EPOCH*1000000/CPU_CLK_PERIOD);
			}
			break;
		}
		case LINK_MONITOR:{
			linkEpochCycle--;
			//control the number of active link in every one epoch
			if(linkEpochCycle == 0) {
				//( ( total FLIT length * 16 bytes) / one epoch time ) * 1000 ms / 1000000000 B = [GB/s]
				double epochDownBandwidth = LINK_SCALING*((requestAccLNG*16)/LINK_EPOCH)/1000000;
				double epochUpBandwidth = LINK_SCALING*((responseAccLNG*16)/LINK_EPOCH)/1000000;
				if(epochDownBandwidth >= maxLinkBand*3 || epochUpBandwidth >= maxLinkBand*3) {
					sleepLink = 0;
				}
				else if(epochDownBandwidth >= maxLinkBand*2 || epochUpBandwidth >= maxLinkBand*2) {
					sleepLink = 1;
				}
				else if(epochDownBandwidth >= maxLinkBand || epochUpBandwidth >= maxLinkBand) {
					sleepLink = 2;
				}
				else {
					sleepLink = 3;
				}

				DEBUG(ALI(33)<<header<<"Down) epoch link BANDWIDTH downstream : "<<epochDownBandwidth<<" GB/s | upstream : "<<epochUpBandwidth<<" GB/s (sleepLink : "<<sleepLink<<")");
				
				//packet accumulated length and link epoch cycle reset
				requestAccLNG = 0;
				responseAccLNG = 0;
				linkEpochCycle = ceil((double)LINK_EPOCH*1000000/CPU_CLK_PERIOD);
			}
			break;
		}
		case AUTONOMOUS:{
			linkEpochCycle--;
			//control the number of active link in every one epoch
			if(linkEpochCycle == 0) {
				//The average MSHR size
				double aveMSHR = accuMSHR/((double)LINK_EPOCH*1000000/CPU_CLK_PERIOD);
				aveMSHR *= MSHR_SCALING;
				//( ( total FLIT length * 16 bytes) / one epoch time ) * 1000 ms / 1000000000 B = [GB/s]
				double epochDownBandwidth = LINK_SCALING*((requestAccLNG*16)/LINK_EPOCH)/1000000;
				double epochUpBandwidth = LINK_SCALING*((responseAccLNG*16)/LINK_EPOCH)/1000000;
				if(aveMSHR >= 4 || epochDownBandwidth >= maxLinkBand*3 || epochUpBandwidth >= maxLinkBand*3) {
					sleepLink = 0;
				}
				else if(aveMSHR >= 3 || epochDownBandwidth >= maxLinkBand*2 || epochUpBandwidth >= maxLinkBand*2) {
					sleepLink = 1;
				}
				else if(aveMSHR >= 2 || epochDownBandwidth >= maxLinkBand || epochUpBandwidth >= maxLinkBand) {
					sleepLink = 2;
				}
				else {
					sleepLink = 3;
				}
				
				DEBUG(ALI(33)<<header<<"Down) epoch average MSHR size : "<<aveMSHR);
				DEBUG(ALI(33)<<header<<"Down) epoch link BANDWIDTH downstream : "<<epochDownBandwidth<<" GB/s | upstream : "<<epochUpBandwidth<<" GB/s (sleepLink : "<<sleepLink<<")");

				//packet accumulated length and link epoch cycle reset
				accuMSHR = 0;
				requestAccLNG = 0;
				responseAccLNG = 0;
				linkEpochCycle = ceil((double)LINK_EPOCH*1000000/CPU_CLK_PERIOD);
			}
			break;
		}
		default:{
			ERROR(header<<"   == Error - WRONG LINK_POWER parameter  (LINK_POWER : "<<LINK_POWER<<")");
			exit(0);
			break;
		}
	}
}

//
//Link power state exit manager
//
void HMCController::LinkPowerExitManager()
{
	switch(LINK_POWER) {
		case NO_MANAGEMENT:{
			break;
		}
		case QUIESCE_SLEEP:{
			//Check the current link state (sleep or down)
			int link = 0;
			int linkState = 0;
			for(link=0; link<NUM_LINKS; link++) {
				if(downLinkMasters[link]->currentState == DOWN) {
					linkState = 1;
					break;
				}
				else if(downLinkMasters[link]->currentState == SLEEP) {
					linkState = 2;
					break;
				}
			}
			//Link is already waking up
			for(int l=0; l<NUM_LINKS; l++) {
				if(downLinkMasters[l]->currentState == TRANSITION_TO_RETRAIN
				|| downLinkMasters[l]->currentState == RETRAIN1
				|| downLinkMasters[l]->currentState == RETRAIN2) {
					linkState = 0;
					break;
				}
			}
			
			//If link is on down mode
			if(linkState == 1) {
				if(downBuffers.size() > 0 && currentClockCycle >= staySREF) {
					modeTransition[link] = currentClockCycle + ceil((double)(tPST+tTXD+tPSC)/CPU_CLK_PERIOD);
					downLinkMasters[link]->currentState = TRANSITION_TO_RETRAIN;
					DEBUG(ALI(33)<<header<<"Down) RETRAINING MODE transition after "<<modeTransition[link]<<" cycle (LM_D"<<link<<")");
				}
			}
			//If link is on sleep mode
			else if(linkState == 2) {
				if(downBuffers.size() >= AWAKE_REQ) {
					modeTransition[link] = currentClockCycle + ceil((double)(tPST+tTXD)/CPU_CLK_PERIOD);
					downLinkMasters[link]->currentState = TRANSITION_TO_RETRAIN;
					DEBUG(ALI(33)<<header<<"Down) RETRAINING MODE transition after "<<modeTransition[link]<<" cycle (LM_D"<<link<<")");
				}
			}
			
			//Retraining mode entry
			for(int l=0; l<NUM_LINKS; l++) {
				if(downLinkMasters[l]->currentState == TRANSITION_TO_RETRAIN) {
					if(currentClockCycle >= modeTransition[l]) {
						for(int k=0; k<NUM_LINKS; k++) {
							if(downLinkMasters[k]->currentState == DOWN) {
								downLinkMasters[k]->currentState = SLEEP;
							}
						}
						downLinkMasters[l]->firstNull = true;
						downLinkMasters[l]->currentState = RETRAIN1;
						stayActive[l] = currentClockCycle + ceil((double)tOP/CPU_CLK_PERIOD);
						DE_CR(ALI(33)<<header<<"Down) entering RETRAINING MODE (LM_D"<<l<<")");
					}
				}
			}
			break;
		}
		case MSHR:
		case LINK_MONITOR:
		case AUTONOMOUS:{
			//awake up links as many as sleepLink
			for(int l=0; l<NUM_LINKS-sleepLink; l++) {
				if(downLinkMasters[l]->currentState == SLEEP) {
					//Stagger transition between active mode and sleep modes in order to avoid supply voltage shifts
					if(currentClockCycle >= staggerActive) {
						uint64_t stag = 0;
						uint64_t tran = 0;
						if(staggeringActive[l]) {
							staggeringActive[l] = false;
							stag = ceil((double)tSS/CPU_CLK_PERIOD);
							tran = ceil((double)tTXD/CPU_CLK_PERIOD);
						}
						else {
							stag = ceil((double)(tPST+tSS)/CPU_CLK_PERIOD);
							tran = ceil((double)(tPST+tTXD)/CPU_CLK_PERIOD);
						}
						staggerActive = currentClockCycle + stag;
						modeTransition[l] = currentClockCycle + tran;
						downLinkMasters[l]->currentState = TRANSITION_TO_RETRAIN;
						DEBUG(ALI(33)<<header<<"Down) RETRAINING MODE transition after "<<modeTransition[l]<<" cycle (stagger : "<<staggerActive<<") (LM_D"<<l<<")");
					}
					else {
						staggeringActive[l] = true;
					}
				}
			}
			
			//Retraining mode entry
			for(int l=0; l<NUM_LINKS; l++) {
				if(downLinkMasters[l]->currentState == TRANSITION_TO_RETRAIN) {
					if(currentClockCycle >= modeTransition[l]) {
						for(int k=0; k<NUM_LINKS; k++) {
							if(downLinkMasters[k]->currentState == DOWN) {
								downLinkMasters[k]->currentState = SLEEP;
							}
						}
						downLinkMasters[l]->firstNull = true;
						downLinkMasters[l]->currentState = RETRAIN1;
						stayActive[l] = currentClockCycle + ceil((double)tOP/CPU_CLK_PERIOD);
						DE_CR(ALI(33)<<header<<"Down) entering RETRAINING MODE (LM_D"<<l<<")");
					}
				}
			}
			break;
		}
		default:{
			ERROR(header<<"   == Error - WRONG LINK_POWER parameter  (LINK_POWER : "<<LINK_POWER<<")");
			exit(0);
			break;
		}
	}
}

//
//Update the allocated MSHR size (It's for MSHR and AUTONOMOUS link power management)
//
void HMCController::UpdateMSHR(unsigned mshr)
{
	alloMSHR = mshr;
}

//
//Link power state exit manager
//
void HMCController::LinkStatistic()
{
	for(int l=0; l<NUM_LINKS; l++) {
		if(downLinkMasters[l]->currentState == SLEEP) {
			linkSleepTime[l]++;
		}
		else if(downLinkMasters[l]->currentState == DOWN) {
			linkDownTime[l]++;
		}
	}
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