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

#include "HMC.h"

namespace CasHMC
{
	
HMC::HMC(ofstream &debugOut_, ofstream &stateOut_):
	SimulatorObject(debugOut_, stateOut_)
{
	clockTuner = 1;
	
	//Make class objects
	downLinkSlaves.reserve(NUM_LINKS);
	upLinkMasters.reserve(NUM_LINKS);
	for(int l=0; l<NUM_LINKS; l++) {
		downLinkSlaves.push_back(new LinkSlave(debugOut, stateOut, l, true));
		upLinkMasters.push_back(new LinkMaster(debugOut, stateOut, l, false));
	}
	crossbarSwitch = new CrossbarSwitch(debugOut, stateOut);
	vaultControllers.reserve(NUM_VAULTS);
	drams.reserve(NUM_VAULTS);
	for(int v=0; v<NUM_VAULTS; v++) {
		vaultControllers.push_back(new VaultController(debugOut, stateOut, v));
		drams.push_back(new DRAM(debugOut, stateOut, v, vaultControllers[v]));
	}
	
	//initialize refresh counters
	for(unsigned v=0; v<NUM_VAULTS; v++) {
		vaultControllers[v]->refreshCountdown = ((REFRESH_PERIOD/tCK)/NUM_VAULTS)*(v+1);
	}
	
	//Crossbar switch, and vault are linked each other by respective lanes
	for(int v=0; v<NUM_VAULTS; v++) {
		//Downstream
		crossbarSwitch->downBufferDest[v] = vaultControllers[v];
		vaultControllers[v]->dramP = drams[v];
		//Upstream
		vaultControllers[v]->upBufferDest = crossbarSwitch;
	}
}

HMC::~HMC()
{
	for(int l=0; l<NUM_LINKS; l++) {
		delete downLinkSlaves[l];
		delete upLinkMasters[l];
	}
	downLinkSlaves.clear();
	upLinkMasters.clear();

	delete crossbarSwitch;
	crossbarSwitch = NULL;

	for(int v=0; v<NUM_VAULTS; v++) {
		delete vaultControllers[v];
		delete drams[v];
	}
	vaultControllers.clear();
	drams.clear();
}

//
//Update the state of HMC
//
void HMC::Update()
{	
	for(int l=0; l<NUM_LINKS; l++) {
		downLinkSlaves[l]->Update();
	}
	crossbarSwitch->Update();
	for(int v=0; v<NUM_VAULTS; v++) {
		vaultControllers[v]->Update();
	}
	for(int v=0; v<NUM_VAULTS; v++) {
		drams[v]->Update();
	}
	for(int l=0; l<NUM_LINKS; l++) {
		upLinkMasters[l]->Update();
	}
	clockTuner++;
	Step();
}

//
//Print current state in state log file
//
void HMC::PrintState()
{
	for(int l=0; l<NUM_LINKS; l++) {
		downLinkSlaves[l]->PrintState();
	}
	crossbarSwitch->PrintState();
	for(int v=0; v<NUM_VAULTS; v++) {
		vaultControllers[v]->PrintState();
	}
	for(int v=0; v<NUM_VAULTS; v++) {
		drams[v]->PrintState();
	}
	for(int l=0; l<NUM_LINKS; l++) {
		upLinkMasters[l]->PrintState();
	}
}

} //namespace CasHMC