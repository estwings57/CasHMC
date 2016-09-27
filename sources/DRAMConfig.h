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

#ifndef DRAMCONFIG_H
#define DRAMCONFIG_H

//DRAMConfig.h

#include <math.h>		//ceil()

static int MEMORY_DENSITY = 4;	//GB
static int NUM_VAULTS = 32;
static int NUM_BANKS = 8;		//The number of banks per a vault(4GB:8 banks, 8GB:16 banks)
static int NUM_ROWS = 16384;	//one bank size is 16MB
static int NUM_COLS = 1024;		//Byte addressing

enum MAPPING_SCHEME
{
	MAX_BLOCK_32B=32,
	MAX_BLOCK_64B=64,
	MAX_BLOCK_128B=128,
	MAX_BLOCK_256B=256
};
static MAPPING_SCHEME ADDRESS_MAPPING = MAX_BLOCK_32B;

static bool QUE_PER_BANK = true;	//Command queue structure (If true, every bank has respective command queue [Bank-Level parallelism])
static bool OPEN_PAGE = false;		//Whether open page policy or close page policy
static int MAX_ROW_ACCESSES = 8;	//The number of consecutive access to identical row address (It should be bigger than max block(ADDRESS_MAPPING) / 32)
static bool USE_LOW_POWER = true;	//Power-down mode setting

//
//DRAM Timing  (HMC_2500_x32(DDR3_1600_x64) - Gem5 HMC modeling)
//
static int REFRESH_PERIOD = 7800;

//CLOCK PERIOD
static double tCK = 0.8;				//1250 MHz	//ns

static unsigned CWL = 3.2;				//(CAS WRITE latency (CWL))
static unsigned CL = ceil(9.9/tCK);		//(CAS (READ) latency)
static unsigned AL = ceil(0/tCK);		//(Additive Latency)
static unsigned tRAS = ceil(21.6/tCK);	//(ACTIVE-to-PRECHARGE command)
static unsigned tRCD = ceil(10.2/tCK);	//(ACTIVE-to-READ or WRITE delay)
static unsigned tRRD = ceil(3.2/tCK);	//(ACTIVE bank a to ACTIVE bank b command)
static unsigned tRC = ceil(32/tCK);		//(ACTIVE-to-ACTIVE/AUTO REFRESH command period)
static unsigned tRP = ceil(7.7/tCK);	//(PRECHARGE command period)
static unsigned tCCD = ceil(3.2/tCK);	//(CAS#-to-CAS# delay)
static unsigned tRTP = ceil(4.9/tCK);	//(Internal READ-to-PRECHARGE delay)
static unsigned tWTR = ceil(4.9/tCK);	//(Internal WRITE-to-READ command delay)
static unsigned tWR = ceil(8/tCK);		//(Write recovery time)
static unsigned tRTRS = ceil(0.8/tCK);	//(rank to rank switching time [consecutive read commands to different ranks])
static unsigned tRFC = ceil(59/tCK);	//(AUTO REFRESH command period)
static unsigned tFAW = ceil(19.2/tCK);	//(4-bank activate period)
static unsigned tCKE = ceil(3.6/tCK);	//(CKE MIN HIGH/LOW time)
static unsigned tXP = ceil(3.2/tCK);
static unsigned tCMD = ceil(tCK/tCK);


static unsigned RL = AL+CL;
static unsigned WL = AL+CWL;
static unsigned BL = ADDRESS_MAPPING/32;	//the internal 32-byte granularity of the DRAM data bus within each vault

#define READ_TO_PRE_DELAY (AL+BL/2+max(tRTP,tCCD)-tCCD)
#define WRITE_TO_PRE_DELAY (WL+BL/2+tWR)
#define READ_TO_WRITE_DELAY (RL+tCCD+2-WL)
#define READ_AUTOPRE_DELAY (AL+tRTP+tRP)
#define WRITE_AUTOPRE_DELAY (WL+BL/2+tWR+tRP)
#define WRITE_TO_READ_DELAY_B (WL+BL/2+tWTR)
//#define WRITE_TO_READ_DELAY_R (WL+BL/2+tRTRS-RL)

#endif