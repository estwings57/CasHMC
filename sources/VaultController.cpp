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

#include "VaultController.h"

namespace CasHMC
{
	
VaultController::VaultController(ofstream &debugOut_, ofstream &stateOut_, unsigned id):
	DualVectorObject<Packet, Packet>(debugOut_, stateOut_, MAX_VLT_BUF, MAX_VLT_BUF),
	vaultContID(id)
{	
	classID << vaultContID;
	header = "        (VC_" + classID.str() + ")";
	
	refreshCountdown = 0;
	powerDown = false;
	poppedCMD = NULL;
	pendingDataSize = 0;
	
	cmdBus = NULL;
	cmdCyclesLeft = 0;
	dataBus = NULL;
	dataCyclesLeft = 0;
	
	//Make class objects
	commandQueue = new CommandQueue(debugOut, stateOut, this, vaultContID);
}

VaultController::~VaultController()
{
	pendingReadData.clear(); 
	writeDataToSend.clear(); 
	writeDataCountdown.clear(); 

	delete commandQueue;
}

//
//Callback receiving packet result
//
void VaultController::CallbackReceiveDown(Packet *downEle, bool chkReceive)
{
/*	if(chkReceive) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) RECEIVING packet");
	}
	else {
		DEBUG(ALI(18)<<header<<ALI(15)<<*downEle<<"Down) packet buffer FULL");
	}*/
}

void VaultController::CallbackReceiveUp(Packet *upEle, bool chkReceive)
{
	if(chkReceive) {
		switch(upEle->CMD) {
			case RD_RS:	DE_CR(ALI(18)<<dramP->header<<ALI(15)<<*upEle<<"Up)   RETURNING read data response packet");	break;
			case WR_RS:	DE_CR(ALI(18)<<dramP->header<<ALI(15)<<*upEle<<"Up)   RETURNING write response packet");		break;
			default:
				ERROR(header<<"  == Error - WRONG response packet command type  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
				exit(0);
				break;
		}
	}
	else {
		ERROR(header<<"  == Error - Vault controller upstream packet buffer FULL  "<<*upEle<<"  (CurrentClock : "<<currentClockCycle<<")");
		ERROR(header<<"             Vault buffer max size : "<<upBufferMax<<", current size : "<<upBuffers.size()<<", "<<*upEle<<" size : "<<upEle->LNG);
		exit(0);
	}
}	

//
//Return read commands from DRAM
//
void VaultController::ReturnCommand(DRAMCommand *retCMD)
{
	//Read and write data commands share one data bus
	if(dataBus != NULL) {
		ERROR(header<<"  == Error - Data Bus Collision  (dataBus:"<<*dataBus<<", retCMD:"<<*retCMD<<")  (CurrentClock : "<<currentClockCycle<<")");
		exit(0);
	}
	
	bool foundMatch = false;
	for(int i=0; i<pendingReadData.size(); i++) {
		if(retCMD->packetTAG == pendingReadData[i]) {
			if(retCMD->lastCMD == true) {
				MakeRespondPacket(retCMD);
			}
			delete retCMD;
			pendingReadData.erase(pendingReadData.begin()+i);
			foundMatch = true;
			break;
		}
	}
	if(!foundMatch) {
		ERROR(header<<"  == Error - Can't find a matching transaction  "<<*retCMD<<" 0x"<<hex<<retCMD->packetTAG<<dec<<"  (CurrentClock : "<<currentClockCycle<<")");
		exit(0);
	}
}

//
//Make response packet from request
//
void VaultController::MakeRespondPacket(DRAMCommand *retCMD)
{
	retCMD->trace->vaultFullLat = currentClockCycle - retCMD->trace->vaultIssueTime;
	Packet *newPacket;
	if(retCMD->commandType == WRITE_DATA) {
		//packet, cmd, tag, lng, *lat
		newPacket = new Packet(RESPONSE, WR_RS, retCMD->packetTAG, 1, retCMD->trace);
		pendingDataSize -= 1;
		//DEBUG(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   pendingDataSize 1 decreased   (current pendingDataSize : "<<pendingDataSize<<")");
	}
	else if(retCMD->commandType == READ_DATA) {
		//packet, cmd, tag, lng, *lat
		newPacket = new Packet(RESPONSE, RD_RS, retCMD->packetTAG, (retCMD->dataSize/16)+1, retCMD->trace);
		pendingDataSize -= (retCMD->dataSize/16)+1;
		//DEBUG(ALI(18)<<header<<ALI(15)<<*retCMD<<"Up)   pendingDataSize "<<(retCMD->dataSize/16)+1<<" decreased   (current pendingDataSize : "<<pendingDataSize<<")");
	}
	else {
		ERROR(header<<"  == Error - Unknown response packet command  cmd : "<<retCMD->commandType<<"  (CurrentClock : "<<currentClockCycle<<")");
		exit(0);
	}
	ReceiveUp(newPacket);
}

//
//Update the state of vault controller
//
void VaultController::Update()
{
	//Update DRAM state and various countdown
	UpdateCountdown();

	//Convert request packet into DRAM commands
	if(bufPopDelay == 0) {
		for(int i=0; i<downBuffers.size(); i++) {
			//Make sure that buffer[0] is not virtual tail packet.
			if(downBuffers[i] != NULL) {
				if(ConvPacketIntoCMDs(downBuffers[i])) {
					int tempLNG = downBuffers[i]->LNG;
					delete downBuffers[i];
					downBuffers.erase(downBuffers.begin()+i, downBuffers.begin()+i+tempLNG);
				}
			}
		}
	}
	
	//Send response packet to crossbar switch
	if(upBuffers.size() > 0) {
		//Make sure that buffer[0] is not virtual tail packet.
		if(upBuffers[0] == NULL) {
			ERROR(header<<"  == Error - Vault controller up buffer[0] is NULL (It could be one of virtual tail packet occupying packet length  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
		else{
			if(upBufferDest->ReceiveUp(upBuffers[0])) {
				DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   SENDING packet to crossbar switch (CS)");
				upBuffers.erase(upBuffers.begin(), upBuffers.begin()+upBuffers[0]->LNG);
			}
			else {
				//DEBUG(ALI(18)<<header<<ALI(15)<<*upBuffers[0]<<"Up)   Crossbar switch buffer FULL");	
			}
		}
	}
	
	//Pop command from command queue
	if(commandQueue->CmdPop(&poppedCMD)) {
		//Write data command will be issued after countdown
		if(poppedCMD->commandType == WRITE || poppedCMD->commandType == WRITE_P) {
			writeDataToSend.push_back(new DRAMCommand(WRITE_DATA, poppedCMD->packetTAG, poppedCMD->bank, poppedCMD->column, 
														poppedCMD->row, poppedCMD->dataSize, poppedCMD->posted, poppedCMD->trace, poppedCMD->lastCMD));
			writeDataCountdown.push_back(WL);
		}
		
		if(poppedCMD->lastCMD == true) {
			if(poppedCMD->commandType == WRITE || poppedCMD->commandType == WRITE_P) {
				pendingDataSize += 1;
				//DEBUG(ALI(18)<<header<<ALI(15)<<*poppedCMD<<"Down) pendingDataSize 1 increased   (current pendingDataSize : "<<pendingDataSize<<")");
			}
			else if(poppedCMD->commandType == READ || poppedCMD->commandType == READ_P) {
				pendingDataSize += (poppedCMD->dataSize/16)+1;
				//DEBUG(ALI(18)<<header<<ALI(15)<<*poppedCMD<<"Down) pendingDataSize "<<(poppedCMD->dataSize/16)+1<<" increased   (current pendingDataSize : "<<pendingDataSize<<")");
			}
		}
		
		//Check for collision on bus
		if(cmdBus != NULL) {
			ERROR(header<<"  == Error - Command bus collision  (cmdBus:"<<*cmdBus<<", poppedCMD:"<<*poppedCMD<<")  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
		}
		cmdBus = poppedCMD;
		cmdCyclesLeft = tCMD;
		poppedCMD = NULL;
	}

	//Power-down mode setting
	EnablePowerdown();

	commandQueue->Update();
	Step();
}

//
//Update DRAM state and various countdown
//
void VaultController::UpdateCountdown()
{
	//Check for outgoing command and handle countdowns
	if(cmdBus != NULL) {
		cmdCyclesLeft--;
		if(cmdCyclesLeft == 0) {	//command is ready to be transmitted
			DE_CR(ALI(18)<<header<<ALI(15)<<*cmdBus<<"Down) ISSUING command to DRAM");
			if(cmdBus->trace != NULL && cmdBus->trace->vaultIssueTime == 0) {
				cmdBus->trace->vaultIssueTime = currentClockCycle;
			}
			dramP->receiveCMD(cmdBus);
			cmdBus = NULL;
		}
	}
	//Check for outgoing write data packets and handle countdowns
	if(dataBus != NULL) {
		dataCyclesLeft--;
		if(dataCyclesLeft == 0) {
			DE_CR(ALI(18)<<header<<ALI(15)<<*dataBus<<"Down) ISSUING data corresponding to previous write command");
			if(dataBus->lastCMD == true) {
				if(dataBus->posted == false) {
					MakeRespondPacket(dataBus);
				}
				else {
					dataBus->trace->tranFullLat = ceil(currentClockCycle * (double)tCK/CPU_CLK_PERIOD) - dataBus->trace->tranTransmitTime;
					dataBus->trace->linkFullLat = ceil(currentClockCycle * (double)tCK/CPU_CLK_PERIOD) - dataBus->trace->linkTransmitTime;
					dataBus->trace->vaultFullLat = currentClockCycle - dataBus->trace->vaultIssueTime;
					delete dataBus->trace;
				}
			}
			dramP->receiveCMD(dataBus);
			dataBus = NULL;
		}
	}
	//Check write data to be sent to DRAM and handle countdowns
	if(writeDataCountdown.size() > 0) {
		if(writeDataCountdown[0] == 0) {
			if(dataBus != NULL) {
				ERROR(header<<"   == Error - Data Bus Collision  "<<*dataBus<<"  (CurrentClock : "<<currentClockCycle<<")");
				exit(0);
			}
			dataBus = writeDataToSend[0];
			dataCyclesLeft = BL;	//block size according to address mapping / DATA_WIDTH

			writeDataCountdown.erase(writeDataCountdown.begin());
			writeDataToSend.erase(writeDataToSend.begin());
		}
		
		for(int i=0; i<writeDataCountdown.size(); i++) {
			writeDataCountdown[i]--;
		}
	}
	
	//Time for a refresh issue a refresh
	refreshCountdown--;
	if(refreshCountdown == 0) {
		commandQueue->refreshWaiting = true;
		refreshCountdown = REFRESH_PERIOD/tCK;
		DEBUG(ALI(39)<<header<<"REFRESH countdown is over");
	}
	//If a rank is powered down, make sure we power it up in time for a refresh
	else if(powerDown && refreshCountdown<=tXP)	{
	}
}

//
//Convert packet into DRAM commands
//
bool VaultController::ConvPacketIntoCMDs(Packet *packet)
{
	unsigned bankAdd, colAdd, rowAdd;
	AddressMapping(packet->ADRS, bankAdd, colAdd, rowAdd);
	
	DRAMCommandType tempCMD;
	unsigned tempSize;		//one unit - 64 bits
	bool tempPosted = false;
	switch(packet->CMD) {
		//Write
		case WR16:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 16;	break;
		case WR32:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 32;	break;
		case WR48:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 48;	break;
		case WR64:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 64;	break;
		case WR80:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 80;	break;
		case WR96:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 96;	break;
		case WR112:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 112;	break;
		case WR128:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 128;	break;
		case WR256:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 256;	break;
		case MD_WR:		tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 16;	break;
		//Poseted Write
		case P_WR16:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 16;	tempPosted = true;	break;
		case P_WR32:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 32;	tempPosted = true;	break;
		case P_WR48:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 48;	tempPosted = true;	break;
		case P_WR64:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 64;	tempPosted = true;	break;
		case P_WR80:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 80;	tempPosted = true;	break;
		case P_WR96:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 96;	tempPosted = true;	break;
		case P_WR112:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 112;	tempPosted = true;	break;
		case P_WR128:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 128;	tempPosted = true;	break;
		case P_WR256:	tempCMD = OPEN_PAGE ? WRITE : WRITE_P;	tempSize = 256;	tempPosted = true;	break;
		//Read
		case RD16:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 16;	pendingReadData.push_back(packet->TAG);	break;
		case RD32:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 32;	pendingReadData.push_back(packet->TAG);	break;
		case RD48:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 48;	pendingReadData.push_back(packet->TAG);	break;
		case RD64:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 64;	pendingReadData.push_back(packet->TAG);	break;
		case RD80:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 80;	pendingReadData.push_back(packet->TAG);	break;
		case RD96:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 96;	pendingReadData.push_back(packet->TAG);	break;
		case RD112:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 112;	pendingReadData.push_back(packet->TAG);	break;
		case RD128:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 128;	pendingReadData.push_back(packet->TAG);	break;
		case RD256:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 256;	pendingReadData.push_back(packet->TAG);	break;
		case MD_RD:		tempCMD = OPEN_PAGE ? READ : READ_P;	tempSize = 16;	pendingReadData.push_back(packet->TAG);	break;
		default:
			ERROR(header<<"  == Error - WRONG packet command type  (CurrentClock : "<<currentClockCycle<<")");
			exit(0);
	}
		
	if(commandQueue->AvailableSpace(bankAdd, ceil((double)tempSize/ADDRESS_MAPPING)+1)) {
		DEBUG(ALI(18)<<header<<ALI(15)<<*packet<<"Down) phyAdd : 0x"<<hex<<setw(9)<<setfill('0')<<packet->ADRS<<dec<<"  bankAdd : "<<bankAdd<<"  colAdd : "<<colAdd<<"  rowAdd : "<<rowAdd);
		//cmdtype, tag, bnk, col, rw, *dt, dSize, pst, *lat
		DRAMCommand *actCMD = new DRAMCommand(ACTIVATE, packet->TAG, bankAdd, colAdd, rowAdd, 0, false, packet->trace, true);
		commandQueue->Enqueue(bankAdd, actCMD);
		
		for(int i=0; i<ceil((double)tempSize/ADDRESS_MAPPING); i++) {
			DRAMCommand *rwCMD;
			if(i < ceil((double)tempSize/ADDRESS_MAPPING)-1) {
				rwCMD = new DRAMCommand(tempCMD, packet->TAG, bankAdd, colAdd, rowAdd, tempSize, tempPosted, packet->trace, false);
			}
			else {
				rwCMD = new DRAMCommand(tempCMD, packet->TAG, bankAdd, colAdd, rowAdd, tempSize, tempPosted, packet->trace, true);
			}
			commandQueue->Enqueue(bankAdd, rwCMD);
			if(tempCMD == READ || tempCMD == READ_P) {
				pendingReadData.push_back(packet->TAG);
			}
		}
		return true;
	}
	else{
		return false;
	}
}

//
//Mapping physical address to bank, column, and row
//
void VaultController::AddressMapping(uint64_t physicalAddress, unsigned &bankAdd, unsigned &colAdd, unsigned &rowAdd)
{
	unsigned maxBlockBit = _log2(ADDRESS_MAPPING);
	physicalAddress >>= maxBlockBit/*max block bits*/ + _log2(NUM_VAULTS)/*vault address bits*/;
	
	//Extract bank address
	bankAdd = physicalAddress & (NUM_BANKS-1);
	physicalAddress >>= _log2(NUM_BANKS);
	
	//Extract column address
	colAdd = physicalAddress & ((NUM_COLS-1)>>maxBlockBit);
	colAdd <<= maxBlockBit;
	physicalAddress >>= (_log2(NUM_COLS)-maxBlockBit);
	
	//Extract row address
	rowAdd = physicalAddress & (NUM_ROWS-1);
}

//
//Power-down mode setting
//
void VaultController::EnablePowerdown()
{
	if(USE_LOW_POWER) {
		if(commandQueue->isEmpty() && !commandQueue->refreshWaiting) {
			if(dramP->powerDown()) {
				powerDown = true;
			}
		}
		else if(powerDown && dramP->bankStates[0]->nextPowerUp <= currentClockCycle) {
			dramP->powerUp();
			powerDown = false;
		}
	}
}

//
//Print current state in state log file
//
void VaultController::PrintState()
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
	
	commandQueue->PrintState();
}

} //namespace CasHMC