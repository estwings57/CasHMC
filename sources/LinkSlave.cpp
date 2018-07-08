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

#include "LinkSlave.h"
#include "LinkMaster.h"

namespace CasHMC
{
	
LinkSlave::LinkSlave(ofstream &debugOut_, ofstream &stateOut_, unsigned id, bool down):
	SingleVectorObject<Packet>(debugOut_, stateOut_, MAX_LINK_BUF, down),
	linkSlaveID(id)
{
	classID << linkSlaveID;
	header = "   (LS_";
	header += downstream ? "D" : "U";
	header += classID.str() + ")";
	
	slaveSEQ = 0;
	countdownCRC = 0;
	startCRC = false;
	downBufferDest = NULL;
	upBufferDest = NULL;
	localLinkMaster = NULL;
}

LinkSlave::~LinkSlave()
{
	linkRxTx.clear(); 
}

//
//Callback receiving packet result
//
void LinkSlave::CallbackReceive(Packet *packet, bool chkReceive)
{
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<(downstream ? "Down) " : "Up)   ")<<"packet buffer FULL");
	}*/
	
	if(packet->packetType == FLOW) {
		ERROR(header<<"  == Error - flow packet "<<*packet<<" is not meant to be here");
		exit(0);
	}
	else {
		if(!chkReceive) {
			ERROR(header<<"  == Error - packet "<<*packet<<" is missed, because link slave buffer is FULL (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
	}
}

//
//Update the state of link slave
//
void LinkSlave::Update()
{
	//Extracting flow control and checking CRC, SEQ from linkRxTx packet 
	if(linkRxTx.size() > 0) {
		//Make sure that linkRxTx[0] is not virtual tail packet.
		if(linkRxTx[0] == NULL) {
			ERROR(header<<"  == Error - linkRxTx[0] is NULL (It could be one of virtual tail packet  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
		else {
			for(int i=0; i<linkRxTx.size(); i++) {
				if(linkRxTx[i]->bufPopDelay == 0) {
					//Link retraining sequence
					if(linkRxTx[i]->CMD == NULL_) {
						//[Responder descrambler initializing]
						//The responder descrambler sync should occur within tRESP1 of the PLL locking
						if(downstream && localLinkMaster->currentState == SLEEP) {
							localLinkMaster->firstNull = true;
							localLinkMaster->currentState = RETRAIN1;
							uint64_t tran = ceil((double)(tRESP1)/tCK);
							localLinkMaster->retrainTransit = currentClockCycle + tran;
						}
						//[Requester descrambler synchronization]
						else if(!downstream && localLinkMaster->currentState == RETRAIN1) {
							localLinkMaster->firstNull = true;
							localLinkMaster->currentState = RETRAIN2;
						}
						//[Responder FLIT synchrony]
						//Responder link lock should occur within tRESP2
						else if(downstream && localLinkMaster->currentState == RETRAIN1) {
							localLinkMaster->firstNull = true;
							localLinkMaster->currentState = RETRAIN2;
							uint64_t tran = ceil((double)(tRESP2)/tCK);
							localLinkMaster->retrainTransit = currentClockCycle + tran;
						}
						//[Requester FLIT synchrony and sending a minimum of 32 NULL FLITs before entering ACTIVE]
						else if(!downstream && localLinkMaster->currentState == RETRAIN2) {
							currentState = ACTIVE;
							localLinkMaster->FinishRetrain();
						}
						//[sending a minimum of 32 NULL FLITs before entering ACTIVE]
						else if(downstream && localLinkMaster->currentState == RETRAIN2) {
							currentState = ACTIVE;
							localLinkMaster->FinishRetrain();
						}
						delete linkRxTx[i];
						linkRxTx.erase(linkRxTx.begin()+i);
						continue;
					}
					
					//Retry control
					if(linkRxTx[i]->CMD == IRTRY) {
						if(linkRxTx[i]->bufPopDelay == 0) {
							linkRxTx[i]->chkRRP = true;
							localLinkMaster->UpdateRetryPointer(linkRxTx[i]);
							if(linkRxTx[i]->FRP == 1) {
								if(localLinkMaster->currentState != LINK_RETRY) {
									localLinkMaster->LinkRetry(linkRxTx[i]);
								}
							}
							else if(linkRxTx[i]->FRP == 2) {
								if(localLinkMaster->currentState == START_RETRY
								|| localLinkMaster->currentState == LINK_RETRY) {
									localLinkMaster->FinishRetry();
									currentState = ACTIVE;
									slaveSEQ = 0;
								}
							}
							delete linkRxTx[i];
							linkRxTx.erase(linkRxTx.begin()+i);
							break;
						}
					}
					else if(currentState == START_RETRY) {
						delete linkRxTx[i];
						linkRxTx.erase(linkRxTx.begin()+i);
						break;
					}
				
					//Flow control
					if(linkRxTx[i]->chkRRP == false) {
						linkRxTx[i]->chkRRP = true;
						localLinkMaster->UpdateRetryPointer(linkRxTx[i]);

						//PRET packet is not saved in the retry buffer (No need to check CRC)
						if(linkRxTx[i]->CMD == PRET) {
							delete linkRxTx[i];
							linkRxTx.erase(linkRxTx.begin()+i);
							break;
						}
					}
					
					//Link low power mode control
					if(linkRxTx[i]->CMD == QUIET) {
						//(upstream) all-way links are checked to enter sleep mode
						if(localLinkMaster->currentState == WAIT) {
							localLinkMaster->currentState = CONFIRM;
							DEBUG(ALI(18)<<header<<ALI(15)<<*linkRxTx[i]<<(downstream ? "Down) " : "Up)   ")
								<<"link is ready to enter LOW POWER mode");
						}
						//(downstream) link is checking the link state to enter sleep mode
						else {
							currentState = WAIT;
							DEBUG(ALI(18)<<header<<ALI(15)<<*linkRxTx[i]<<(downstream ? "Down) " : "Up)   ")
								<<"link is checking the link state to enter sleep mode");
						}
						delete linkRxTx[i];
						linkRxTx.erase(linkRxTx.begin()+i);
						break;
					}
					
					//Count CRC calculation time
					if(linkRxTx[i]->chkRRP == true && linkRxTx[i]->chkCRC == false) {
						if(CRC_CHECK && !startCRC) {
							startCRC = true;
							countdownCRC = ceil((double)CRC_CAL_CYCLE * linkRxTx[i]->LNG);
						}
							
						if(CRC_CHECK && countdownCRC > 0) {
							DEBUG(ALI(18)<<header<<ALI(15)<<*linkRxTx[i]<<(downstream ? "Down) " : "Up)   ")
									<<"WAITING CRC calculation ("<<countdownCRC<<"/"<<ceil((double)CRC_CAL_CYCLE*linkRxTx[i]->LNG)<<")");
						}
						else {
							//Error check
							linkRxTx[i]->chkCRC = true;
							if(CheckNoError(linkRxTx[i])) {
								localLinkMaster->ReturnRetryPointer(linkRxTx[i]);
								localLinkMaster->UpdateToken(linkRxTx[i]);
								if(linkRxTx[i]->CMD != TRET) {
									Receive(linkRxTx[i]);
								}
								else {
									delete linkRxTx[i];
								}
								linkRxTx.erase(linkRxTx.begin()+i);
								break;
							}
							//Error abort mode
							else {
								if(localLinkMaster->retryStartPacket == NULL) {
									localLinkMaster->retryStartPacket = new Packet(*linkRxTx[i]);
								}
								else {
									ERROR(header<<"  == Error - localLinkMaster->retryStartPacket is NOT NULL  (CurrentClock : "<<currentClockCycle<<")");
									exit(0);
								}
								localLinkMaster->StartRetry(linkRxTx[i]);
								localLinkMaster->linkP->statis->errorPerLink[linkSlaveID]++;
								for(int j=0; j<linkRxTx.size(); j++) {
									if(linkRxTx[j] != NULL) {
										delete linkRxTx[j];
									}
									linkRxTx.erase(linkRxTx.begin()+j);
									j--;
								}
								linkRxTx.clear();
								currentState = START_RETRY;
							}
							startCRC = false;
						}
						countdownCRC = (countdownCRC>0) ? countdownCRC-1 : 0;
					}
				}
			}
		}
	}
	
	//Checking the link state to enter sleep mode
	if(currentState == WAIT) {
		//Link is ready for low power mode
		if(localLinkMaster->Buffers.size() == 0
		&& localLinkMaster->retBufReadP == localLinkMaster->retBufWriteP
		&& localLinkMaster->tokenCount == MAX_LINK_BUF) {
			currentState = SLEEP;
			localLinkMaster->currentState = SLEEP;
			localLinkMaster->QuitePacket();
		}
	}

	//Sending packet
	if(Buffers.size() > 0) {
		bool chkRcv = (downstream ? downBufferDest->ReceiveDown(Buffers[0]) : upBufferDest->ReceiveUp(Buffers[0]));
		if(chkRcv) {
			localLinkMaster->ReturnTocken(Buffers[0]);
			DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<(downstream ? "Down) " : "Up)   ")
						<<"SENDING packet to "<<(downstream ? "crossbar switch (CS)" : "HMC controller (HC)"));
			Buffers.erase(Buffers.begin(), Buffers.begin()+Buffers[0]->LNG);
		}
		else {
			//DEBUG(ALI(18)<<header<<ALI(15)<<*Buffers[0]<<"(downstream ? "Down) Crossbar switch" : "Up)   HMC controller")<<" buffer FULL");	
		}
	}

	Step();
	for(int i=0; i<linkRxTx.size(); i++) {
		if(linkRxTx[i] != NULL) {
			linkRxTx[i]->bufPopDelay = (linkRxTx[i]->bufPopDelay>0) ? linkRxTx[i]->bufPopDelay-1 : 0;
		}
	}
}

//
//Check error in receiving packet by CRC and SEQ
//
bool LinkSlave::CheckNoError(Packet *chkPacket)
{
	unsigned tempSEQ = (slaveSEQ<7) ? slaveSEQ++ : 0;
	if(CRC_CHECK) {
		unsigned tempCRC = chkPacket->GetCRC();
		if(chkPacket->CRC == tempCRC) {
			if(chkPacket->SEQ == tempSEQ) {
				DEBUG(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
							<<"CRC(0x"<<hex<<setw(9)<<setfill('0')<<tempCRC<<dec<<"), SEQ("<<tempSEQ<<") are checked (NO error)");
				return true;
			}
			else {
				DE_CR(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
							<<"= Error abort mode =  (packet SEQ : "<<chkPacket->SEQ<<" / Slave SEQ : "<<tempSEQ<<")");
				cout<<ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
							<<"= Error abort mode =  (packet SEQ : "<<chkPacket->SEQ<<" / Slave SEQ : "<<tempSEQ<<")"<<endl;
				return false;
			}
		}
		else {
			DE_CR(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
						<<"= Error abort mode =  (packet CRC : 0x"<<hex<<setw(9)<<setfill('0')<<chkPacket->CRC<<dec
						<<" / Slave CRC : 0x"<<hex<<setw(9)<<setfill('0')<<tempCRC<<dec<<")");
			return false;
		}
	}
	else {
		if(chkPacket->SEQ == tempSEQ) {
			DEBUG(ALI(18)<<header<<ALI(15)<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
						<<"SEQ("<<tempSEQ<<") is checked (NO error)");
			return true;
		}
		else {
			DE_CR("    (LS_"<<linkSlaveID<<ALI(7)<<")"<<*chkPacket<<(downstream ? "Down) " : "Up)   ")
						<<"= Error abort mode =  (packet SEQ : "<<chkPacket->SEQ<<" / Slave SEQ : "<<tempSEQ<<")");
			return false;
		}
	}
	return false;
}

//
//Print current state in state log file
//
void LinkSlave::PrintState()
{
	if(Buffers.size()>0) {
		STATEN(ALI(17)<<header);
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
	
	//Print linkRx buffer
	if(linkRxTx.size()>0) {
		STATEN(ALI(17)<<header);
		STATEN("LKRX ");
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
}

} //namespace CasHMC