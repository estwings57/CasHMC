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

#include "CommandQueue.h"
#include "VaultController.h"

namespace CasHMC
{

CommandQueue::CommandQueue(ofstream &debugOut_, ofstream &stateOut_, VaultController *parent, unsigned id):
	SimulatorObject(debugOut_, stateOut_),
	vaultContP(parent),
	cmdQueID(id)
{
	classID << cmdQueID;
	header = "        (CQ_" + classID.str();
	
	refreshWaiting = false;
	issuedBank = 0;
	
	tFAWCountdown.reserve(4);
	rowAccessCounter = vector<unsigned>(NUM_BANKS,0);
	if(QUE_PER_BANK) {
		bufPopDelayPerBank = vector<int>(NUM_BANKS, 0);
		queue = vector< vector<DRAMCommand *> >(NUM_BANKS, vector<DRAMCommand *>());
		for(int b=0; b<NUM_BANKS; b++) {
			queue[b].reserve(MAX_CMD_QUE);
		}
	}
	else {
		bufPopDelayPerBank = vector<int>(1, 0);
		queue = vector< vector<DRAMCommand *> >(1, vector<DRAMCommand *>());
		queue[0].reserve(MAX_CMD_QUE);
	}
}

CommandQueue::~CommandQueue()
{
	for(int b=0; b<NUM_BANKS; b++) {
		ACCESSQUE(b).clear();
		if(!QUE_PER_BANK)	break;
	}
	queue.clear();
	bufPopDelayPerBank.clear();
	tFAWCountdown.clear();
	rowAccessCounter.clear();
}

//
//Check cmdN available space in command queue
//
bool CommandQueue::AvailableSpace(unsigned bank, unsigned cmdN)
{
	if(ACCESSQUE(bank).size()+cmdN <= MAX_CMD_QUE) {
		return true;
	}
	else {
/*		if(QUE_PER_BANK) {
			classID.str( string() );	classID.clear();
			classID << bank;
			DEBUG(ALI(39)<<(header+"-"+classID.str()+")")<<"Down) Command queue FULL");
		}
		else {
			DEBUG(ALI(39)<<(header+")")<<"Down) Command queue FULL");
		}*/
		return false;
	}
}

//
//Push received command into buffer to be transmitted to DRAM device
//
void CommandQueue::Enqueue(unsigned bank, DRAMCommand *enqCMD)
{
	if(QUE_PER_BANK) {
		classID.str( string() );	classID.clear();
		classID << bank;
		DE_CR(ALI(18)<<(header+"-"+classID.str()+")")<<ALI(15)<<*enqCMD<<"Down) PUSHING command into command queue["<<ACCESSQUE(bank).size()<<"/"<<MAX_CMD_QUE-1<<"]");
	}
	else {
		DE_CR(ALI(18)<<(header+")")<<ALI(15)<<*enqCMD<<"Down) PUSHING command into command queue["<<ACCESSQUE(bank).size()<<"/"<<MAX_CMD_QUE-1<<"]");
	}
	if(ACCESSQUE(bank).size() == 0) {
		POPCYCLE(bank) = (POPCYCLE(bank)>0) ? POPCYCLE(bank) : 1;
	}
	ACCESSQUE(bank).push_back(enqCMD);
}

//
//Pop command from command queue by HMC contoller arbitration
//
bool CommandQueue::CmdPop(DRAMCommand **popedCMD)
{
	//Decrement all the counters we have going
	for(int i=0; i<tFAWCountdown.size(); i++) {
		tFAWCountdown[i]--;
	}
	//The head will always be the smallest counter, so check if it has reached 0
	if(tFAWCountdown.size()>0 && tFAWCountdown[0]==0) {
		tFAWCountdown.erase(tFAWCountdown.begin());
	}

	//HMC command scheduling policy
	//Close page
	if(OPEN_PAGE == false) {
		if(refreshWaiting) {
			//Look into all bank
			bool refreshPossible = true;
			for(int b=0; b<NUM_BANKS; b++) {	//Need to check whether bank is open or not
				if(BANKSTATE(b)->currentBankState == ROW_ACTIVE) {
					refreshPossible = false;
					break;
				}
				//The next ACT and next REF can be issued at the same. nextActivate is considered as nextRefresh
				else if(BANKSTATE(b)->nextActivate > currentClockCycle) {
					refreshPossible = false;
					break;
				}
			}
			if(refreshPossible && BANKSTATE(0)->currentBankState!=POWERDOWN) {
				*popedCMD = new DRAMCommand(REFRESH, 0, 0, 0, 0, 0, false, NULL, true);
				refreshWaiting = false;
			}
			else {
				return false;
			}
		}
		//Normal command scheduling
		else {
			bool foundIssuable = false;
			int bankQueChk = 0;
			
			while(1) {
				if(POPCYCLE(issuedBank) == 0) {
					//Search from beginning to find first issuable command
					for(int i=0; i<ACCESSQUE(issuedBank).size(); i++) {
						if(isIssuable(ACCESSQUE(issuedBank)[i])) {
							//Check to make sure not removing a read/write that is paired with an activate
							if(i>0 && ACCESSQUE(issuedBank)[i-1]->commandType==ACTIVATE
								 &&	ACCESSQUE(issuedBank)[i-1]->packetTAG==ACCESSQUE(issuedBank)[i]->packetTAG)
								continue;
							
							*popedCMD = ACCESSQUE(issuedBank)[i];
							ACCESSQUE(issuedBank).erase(ACCESSQUE(issuedBank).begin()+i);
							foundIssuable = true;
							break;
						}
					}
				}
				bankQueChk++;
				issuedBank = (issuedBank<NUM_BANKS-1) ? issuedBank+1 : 0;

				if(foundIssuable)	break;
				else if(bankQueChk == NUM_BANKS)	break;
			}
			if(!foundIssuable)	return false;
		}
	}
	//Open page
	else {
		bool sendingREForPRE = false;
		if(refreshWaiting) {
			//Look into all bank
			bool refreshPossible = true;
			for(int b=0; b<NUM_BANKS; b++) {	//Need to check whether bank is open or not
				if(BANKSTATE(b)->currentBankState == ROW_ACTIVE) {
					refreshPossible = false;
					bool closeRow = true;

					for(int i=0; i<ACCESSQUE(b).size(); i++) {
						DRAMCommand *tempCMD = ACCESSQUE(b)[i];
						//If a command in the queue is going to the same bank and row
						if(b==tempCMD->bank && BANKSTATE(b)->openRowAddress==tempCMD->row) {
							//and is not an activate
							if(tempCMD->commandType != ACTIVATE) {
								closeRow = false;
								//and can be issued
								if(isIssuable(tempCMD)) {
									*popedCMD = tempCMD;
									ACCESSQUE(b).erase(ACCESSQUE(b).begin()+i);
									sendingREForPRE = true;
								}
								break;
							}
							else {
								break;
							}
						}
					}
					if(closeRow && BANKSTATE(b)->nextPrecharge<=currentClockCycle) {
						rowAccessCounter[b]=0;
						*popedCMD = new DRAMCommand(PRECHARGE, 0, b, 0, 0, 0, false, NULL, true);
						sendingREForPRE = true;
					}
					break;
				}
				//The next ACT and next REF can be issued at the same. nextActivate is considered as nextRefresh
				else if(BANKSTATE(b)->nextActivate > currentClockCycle) {
					refreshPossible = false;
					break;
				}
			}
			if(refreshPossible && BANKSTATE(0)->currentBankState!=POWERDOWN) {
				*popedCMD = new DRAMCommand(REFRESH, 0, 0, 0, 0, 0, false, NULL, true);
				sendingREForPRE = true;
				refreshWaiting = false;
			}
		}
		//Normal command scheduling
		if(!sendingREForPRE) {
			bool foundIssuable = false;
			int bankQueChk = 0;
		
			while(1) {
				if(POPCYCLE(issuedBank) == 0) {
					//Search from beginning to find first issuable command
					for(int i=0; i<ACCESSQUE(issuedBank).size(); i++) {
						if(isIssuable(ACCESSQUE(issuedBank)[i])) {
							//Check for dependencies
							int j;
							bool dependencyFound = false;
							for(j=0; j<i; j++) {
								DRAMCommand *prevCMD = ACCESSQUE(issuedBank)[j];
								if(prevCMD->commandType != ACTIVATE &&
								prevCMD->bank == ACCESSQUE(issuedBank)[i]->bank &&
								prevCMD->row == ACCESSQUE(issuedBank)[i]->row) {
									dependencyFound = true;
									break;
								}
							}
							if(dependencyFound) continue;
							*popedCMD = ACCESSQUE(issuedBank)[i];
							//If the previous bus packet is an activate, that activate have to be removed
							//(check i>0 because if i==0 then theres nothing before it)
							if(i>0 && ACCESSQUE(issuedBank)[i-1]->commandType==ACTIVATE) {
								rowAccessCounter[(*popedCMD)->bank]++;
								delete ACCESSQUE(issuedBank)[i-1];
								ACCESSQUE(issuedBank).erase(ACCESSQUE(issuedBank).begin()+i-1, ACCESSQUE(issuedBank).begin()+i+1);
							}
							else{
								ACCESSQUE(issuedBank).erase(ACCESSQUE(issuedBank).begin()+i);
							}
							foundIssuable = true;
							break;
						}
					}
				}
				bankQueChk++;
				issuedBank = (issuedBank<NUM_BANKS-1) ? issuedBank+1 : 0;
				
				if(foundIssuable)	break;
				else if(bankQueChk == NUM_BANKS)	break;
			}
		
			//If nothing was issuable to open bank, issue PRE command
			if(!foundIssuable) {
				bool sendingPRE = false;
				
				for(int b=0; b<NUM_BANKS; b++) {
					bool found = false;
					if(BANKSTATE(b)->currentBankState == ROW_ACTIVE) {
						for(int i=0; i<ACCESSQUE(b).size(); i++) {
							//If there is something going to opened bank and row, don't send PRE command
							if(ACCESSQUE(b)[i]->bank==b
								&& ACCESSQUE(b)[i]->row == BANKSTATE(b)->openRowAddress) {
								found = true;
								break;
							}
						}
						//Too many accesses have happend, close it
						if(!found || rowAccessCounter[b]>=MAX_ROW_ACCESSES) {
							if(BANKSTATE(b)->nextPrecharge <= currentClockCycle) {
								rowAccessCounter[b]=0;
								*popedCMD = new DRAMCommand(PRECHARGE, 0, b, 0, 0, 0, false, NULL, true);
								sendingPRE = true;
								break;
							}
						}
					}
				}

				if(!sendingPRE)	return false;
			}
		}
	}
	
	if((*popedCMD)->commandType == ACTIVATE) {
		tFAWCountdown.push_back(tFAW);
	}

	if(QUE_PER_BANK) {
		classID.str( string() );	classID.clear();
		classID << (*popedCMD)->bank;
		DEBUG(ALI(18)<<(header+"-"+classID.str()+")")<<ALI(15)<<**popedCMD<<"Down) POP command from command queue");
	}
	else {
		DEBUG(ALI(18)<<(header+")")<<(*popedCMD)->bank<<")"<<ALI(15)<<**popedCMD<<"Down) POP command from command queue");
	}
	return true;
}

//
//Check command able to be issued
//
bool CommandQueue::isIssuable(DRAMCommand *issueCMD)
{
	switch(issueCMD->commandType) {
		case REFRESH:
			break;
		case ACTIVATE:
			if((BANKSTATE(issueCMD->bank)->currentBankState == IDLE || BANKSTATE(issueCMD->bank)->currentBankState == REFRESHING) 
			&& BANKSTATE(issueCMD->bank)->nextActivate <= currentClockCycle 
			&& tFAWCountdown.size() < 4) {
				return true;
			}
			else {
				return false;
			}
			break;
		case WRITE:
		case WRITE_P:
			if(BANKSTATE(issueCMD->bank)->currentBankState == ROW_ACTIVE 
			&& BANKSTATE(issueCMD->bank)->nextWrite <= currentClockCycle
			&& BANKSTATE(issueCMD->bank)->openRowAddress == issueCMD->row
			&& rowAccessCounter[issueCMD->bank] < MAX_ROW_ACCESSES) {
				//Check the available buffer space of the vault controller with regard to read/write return data
				if(vaultContP->pendingDataSize+(issueCMD->dataSize/16)+1 <= (vaultContP->upBufferMax)-(vaultContP->upBuffers.size())) {
					return true;
				}
				else{
					return false;
				}
			}
			else {
				return false;
			}
			break;
		case READ:
		case READ_P:
			if(BANKSTATE(issueCMD->bank)->currentBankState == ROW_ACTIVE
			&& BANKSTATE(issueCMD->bank)->nextRead <= currentClockCycle
			&& BANKSTATE(issueCMD->bank)->openRowAddress == issueCMD->row
			&& rowAccessCounter[issueCMD->bank] < MAX_ROW_ACCESSES) {
				//Check the available buffer space of the vault controller with regard to read/write return data
				if(vaultContP->pendingDataSize+(issueCMD->dataSize/16)+1 <= (vaultContP->upBufferMax)-(vaultContP->upBuffers.size())) {
					return true;
				}
				else{
					return false;
				}
			}
			else {
				return false;
			}
			break;
		case PRECHARGE:
			if(BANKSTATE(issueCMD->bank)->currentBankState == ROW_ACTIVE
			&& BANKSTATE(issueCMD->bank)->nextPrecharge <= currentClockCycle) {
				return true;
			}
			else {
				return false;
			}
			break;
		default:
			ERROR(header<<"  == Error - Trying to issue a crazy bus packet type : "<<issueCMD<<"  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
	}
	return false;
}

//
//Check whether command queue is empty or not
//
bool CommandQueue::isEmpty()
{
	for(int b=0; b<NUM_BANKS; b++) {
		if(!ACCESSQUE(b).empty()) {
			return false;
		}
	}
	return true;
}

//
//Update the state of vault controller
//
void CommandQueue::Update()
{
	for(int b=0; b<NUM_BANKS; b++){
		POPCYCLE(b) = (POPCYCLE(b)>0) ? POPCYCLE(b)-1 : 0;
		if(!QUE_PER_BANK)	break;	
	}

	Step();
}

//
//Print current state in state log file
//
void CommandQueue::PrintState()
{
	for(int b=0; b<NUM_BANKS; b++) {
		if(ACCESSQUE(b).size()>0) {
			classID.str( string() );	classID.clear();
			classID << b;
			STATEN(ALI(17)<<(header + (QUE_PER_BANK ? ("-"+classID.str()+")") : (")"))));
			STATEN(" Que ");
			for(int i=0; i<MAX_CMD_QUE; i++) {
				if(i>0 && i%8==0) {
					STATEN(endl<<"                      ");
				}
				if(i < ACCESSQUE(b).size()) {
					STATEN(*ACCESSQUE(b)[i]);
				}
				else if(i == MAX_CMD_QUE-1) {
					STATEN("[ - ]");
				}
				else {
					STATEN("[ - ]...");
					break;
				}
			}
			STATEN(endl);
		}
		if(!QUE_PER_BANK)	break;
	}
}

} //namespace CasHMC
