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

#include "DRAM.h"
#include "VaultController.h"

namespace CasHMC
{
	
DRAM::DRAM(ofstream &debugOut_, ofstream &stateOut_, unsigned id, VaultController *contP):
	SimulatorObject(debugOut_, stateOut_),
	DRAMID(id),
	vaultContP(contP)
{
	classID << DRAMID;
	header = "        (DR_" + classID.str() + ")";
	
	readData = NULL;
	dataCyclesLeft = 0;

	bankStates.reserve(NUM_BANKS);
	for(int b=0; b<NUM_BANKS; b++) {
		bankStates.push_back(new BankState(b));
	}
	previousBankState = vector<BankStateType>(NUM_BANKS, IDLE);
}

DRAM::~DRAM()
{
	for(int i=0; i<readReturnDATA.size(); i++) {
		delete readReturnDATA[i];
	}
	readReturnDATA.clear();
	readReturnCountdown.clear();
	
	for(int b=0; b<NUM_BANKS; b++) {
		delete bankStates[b];
	}
	bankStates.clear();
}

//
//Receive command from vault controller
//
void DRAM::receiveCMD(DRAMCommand *recvCMD)
{
	switch(recvCMD->commandType) {
		case ACTIVATE:
			bankStates[recvCMD->bank]->currentBankState = ROW_ACTIVE;
			bankStates[recvCMD->bank]->lastCommand = ACTIVATE;
			bankStates[recvCMD->bank]->openRowAddress = recvCMD->row;
			bankStates[recvCMD->bank]->nextActivate = max(currentClockCycle + tRC, bankStates[recvCMD->bank]->nextActivate);
			bankStates[recvCMD->bank]->nextPrecharge = max(currentClockCycle + tRAS, bankStates[recvCMD->bank]->nextPrecharge);
			bankStates[recvCMD->bank]->nextRead = max(currentClockCycle + (tRCD-AL), bankStates[recvCMD->bank]->nextRead);
			bankStates[recvCMD->bank]->nextWrite = max(currentClockCycle + (tRCD-AL), bankStates[recvCMD->bank]->nextWrite);
			for(int b=0; b<NUM_BANKS; b++) {
				if(recvCMD->bank != b) {
					bankStates[b]->nextActivate = max(currentClockCycle + tRRD, bankStates[b]->nextActivate);
				}
			}
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      next READ or WRITE time : "<<bankStates[recvCMD->bank]->nextRead<<" [HMC clk]");
			delete recvCMD;
			break;
		case READ:
			bankStates[recvCMD->bank]->nextPrecharge = max(currentClockCycle + READ_TO_PRE_DELAY, bankStates[recvCMD->bank]->nextPrecharge);
			bankStates[recvCMD->bank]->lastCommand = READ;
			for(int b=0; b<NUM_BANKS; b++) {
				bankStates[b]->nextRead = max(currentClockCycle + max(tCCD, BL), bankStates[b]->nextRead);
				bankStates[b]->nextWrite = max(currentClockCycle + READ_TO_WRITE_DELAY, bankStates[b]->nextWrite);
			}
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      READ DATA return time : "<<currentClockCycle+RL+BL<<" [HMC clk]"
													<<" / next PRECHARGE time : "<<bankStates[recvCMD->bank]->nextPrecharge<<" [HMC clk]");
			recvCMD->commandType = READ_DATA;
			readReturnDATA.push_back(recvCMD);
			readReturnCountdown.push_back(RL);
			break;
		case READ_P:
			bankStates[recvCMD->bank]->nextActivate = max(currentClockCycle + READ_AUTOPRE_DELAY, bankStates[recvCMD->bank]->nextActivate);
			bankStates[recvCMD->bank]->lastCommand = READ_P;
			bankStates[recvCMD->bank]->stateChangeCountdown = READ_TO_PRE_DELAY;
			for(int b=0; b<NUM_BANKS; b++) {
				bankStates[b]->nextRead = max(currentClockCycle + max(tCCD, BL), bankStates[b]->nextRead);
				bankStates[b]->nextWrite = max(currentClockCycle + READ_TO_WRITE_DELAY, bankStates[b]->nextWrite);
			}
			if(recvCMD->commandType == READ_P) {
				bankStates[recvCMD->bank]->nextRead = bankStates[recvCMD->bank]->nextActivate;
				bankStates[recvCMD->bank]->nextWrite = bankStates[recvCMD->bank]->nextActivate;
			}	
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      READ DATA return time : "<<currentClockCycle+RL+BL<<" [HMC clk]"
													<<" / next ACTIVATE time : "<<bankStates[recvCMD->bank]->nextActivate<<" [HMC clk]");
			recvCMD->commandType = READ_DATA;
			readReturnDATA.push_back(recvCMD);
			readReturnCountdown.push_back(RL);
			break;
		case WRITE:
			bankStates[recvCMD->bank]->nextPrecharge = max(currentClockCycle + WRITE_TO_PRE_DELAY, bankStates[recvCMD->bank]->nextPrecharge);
			bankStates[recvCMD->bank]->lastCommand = WRITE;
			for(int b=0; b<NUM_BANKS; b++) {
				bankStates[b]->nextWrite = max(currentClockCycle + max(BL, tCCD), bankStates[b]->nextWrite);
				bankStates[b]->nextRead = max(currentClockCycle + WRITE_TO_READ_DELAY_B, bankStates[b]->nextRead);
			}
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      WRTIE DATA time : "<<currentClockCycle+WL+BL<<" [HMC clk]"
													<<" / next PRECHARGE time : "<<bankStates[recvCMD->bank]->nextPrecharge<<" [HMC clk]");
			delete recvCMD;
			break;
		case WRITE_P:
			bankStates[recvCMD->bank]->nextActivate = max(currentClockCycle + WRITE_AUTOPRE_DELAY, bankStates[recvCMD->bank]->nextActivate);
			bankStates[recvCMD->bank]->lastCommand = WRITE_P;
			bankStates[recvCMD->bank]->stateChangeCountdown = WRITE_TO_PRE_DELAY;
			for(int b=0; b<NUM_BANKS; b++) {
				bankStates[b]->nextWrite = max(currentClockCycle + max(BL, tCCD), bankStates[b]->nextWrite);
				bankStates[b]->nextRead = max(currentClockCycle + WRITE_TO_READ_DELAY_B, bankStates[b]->nextRead);
			}
			bankStates[recvCMD->bank]->nextRead = bankStates[recvCMD->bank]->nextActivate;
			bankStates[recvCMD->bank]->nextWrite = bankStates[recvCMD->bank]->nextActivate;
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      WRTIE DATA time : "<<currentClockCycle+WL+BL<<" [HMC clk]"
													<<" / next ACTIVATE time : "<<bankStates[recvCMD->bank]->nextActivate<<" [HMC clk]");
			delete recvCMD; 
			break;
		case WRITE_DATA:
			delete recvCMD;
			break;
		case PRECHARGE:
			bankStates[recvCMD->bank]->currentBankState = PRECHARGING;
			bankStates[recvCMD->bank]->lastCommand = PRECHARGE;
			bankStates[recvCMD->bank]->openRowAddress = 0;
			bankStates[recvCMD->bank]->stateChangeCountdown = tRP;
			bankStates[recvCMD->bank]->nextActivate = max(currentClockCycle + tRP, bankStates[recvCMD->bank]->nextActivate);
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      next ACTIVATE time : "<<bankStates[recvCMD->bank]->nextActivate<<" [HMC clk]");
			delete recvCMD;
			break;	
		case REFRESH:
			for(int b=0; b<NUM_BANKS; b++) {
				bankStates[b]->nextActivate = currentClockCycle + tRFC;
				bankStates[b]->currentBankState = REFRESHING;
				bankStates[b]->lastCommand = REFRESH;
				bankStates[b]->stateChangeCountdown = tRFC;
			}
			DEBUG(ALI(18)<<header<<ALI(15)<<*recvCMD<<"      next ACTIVATE time : "<<bankStates[recvCMD->bank]->nextActivate<<" [HMC clk]");
			delete recvCMD;
			break;
		default:
			ERROR(header<<"  (DR) == Error - Wrong command type    popped command : "<<*recvCMD<<"  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
			break;
	}
}

//
//Switch DRAM to power down mode
//
bool DRAM::powerDown()
{
	//check to make sure all banks are idle
	bool allIdle = true;
	for(int b=0; b<NUM_BANKS; b++) {
		if(bankStates[b]->currentBankState != IDLE) {
			allIdle = false;
			break;
		}
	}
	if(allIdle) {
		for(int b=0; b<NUM_BANKS; b++) {
			bankStates[b]->currentBankState = POWERDOWN;
			bankStates[b]->nextPowerUp = currentClockCycle + tCKE;
		}
		return true;
	}
	else {
		return false;
	}
}

//
//Awake DRAM in power down mode
//
void DRAM::powerUp()
{
	for(int b=0; b<NUM_BANKS; b++) {
		bankStates[b]->lastCommand = POWERDOWN_EXIT;
		bankStates[b]->currentBankState = AWAKING;
		bankStates[b]->stateChangeCountdown = tXP;
		bankStates[b]->nextActivate = currentClockCycle + tXP;
	}
	DEBUG(ALI(39)<<header<<"AWAKE DRAM power down mode   nextActivate time : "<<bankStates[0]->nextActivate);
}

//
//Updates the state of DRAM
//
void DRAM::Update()
{
	//update bank states
	for(int b=0; b<NUM_BANKS; b++) {
		if(bankStates[b]->stateChangeCountdown>0) {
			//decrement counters
			bankStates[b]->stateChangeCountdown--;

			//if counter has reached 0, change state
			if(bankStates[b]->stateChangeCountdown == 0) {
				switch(bankStates[b]->lastCommand) {
					//only these commands have an implicit state change
					case WRITE_P:
					case READ_P:
						bankStates[b]->currentBankState = PRECHARGING;
						bankStates[b]->lastCommand = PRECHARGE;
						bankStates[b]->stateChangeCountdown = tRP;
						break;
					case REFRESH:
					case PRECHARGE:
					case POWERDOWN_EXIT:
						bankStates[b]->currentBankState = IDLE;
						break;
					default:
						break;
				}
			}
		}
	}
	
	UpdateState();
	Step();
}

//
//Decrease countdown and send back return command to vault controller
//
void DRAM::UpdateState()
{
	if(readData != NULL) {
		dataCyclesLeft--;
		if(dataCyclesLeft == 0) {
			vaultContP->ReturnCommand(readData);
			readData = NULL;
		}
	}

	//Send back return command to vault controller
	if(readReturnCountdown.size() > 0 && readReturnCountdown[0]==0) {
		readData = readReturnDATA[0];
		dataCyclesLeft = BL;
		readReturnDATA.erase(readReturnDATA.begin());
		readReturnCountdown.erase(readReturnCountdown.begin());
	}
	
	for(int i=0; i<readReturnCountdown.size(); i++) {
		readReturnCountdown[i]--;
	}
}

//previousBankState
//Print current state in state log file
//
void DRAM::PrintState()
{
	bool printDR = false;
	for(int b=0; b<NUM_BANKS; b++) {
		if(previousBankState[b] != bankStates[b]->currentBankState) {
			printDR = true;
			break;
		}
	}
	
	if(printDR) {
		STATEN(ALI(17)<<header);
		STATEN("State");
		for(int b=0; b<NUM_BANKS; b++) {
			previousBankState[b] = bankStates[b]->currentBankState;
			STATEN("[");
			STATEN(*bankStates[b]);
			if(bankStates[b]->currentBankState == ROW_ACTIVE) {
				STATEN("-"<<bankStates[b]->openRowAddress);
			}
			STATEN("]");
		}
		STATEN(endl);
	}
}

} //namespace CasHMC