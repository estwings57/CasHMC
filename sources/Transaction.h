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

#ifndef TRANSACTION_H
#define TRANSACTION_H

//Transaction.h

#include <stdint.h>		//uint64_t
#include <stdlib.h>		//exit(0)
#include <iomanip>		//setw()
#include <iostream> 	//ostream
#include <sstream>		//stringstream

#include "ConfigValue.h"
#include "TranTrace.h"
#include "TranStatistic.h"

using namespace std;

namespace CasHMC
{

enum TransactionType
{
	DATA_READ,
	DATA_WRITE,
	RETURN_DATA,
	//ATOMICS commands for HMC
	ATM_2ADD8, ATM_ADD16, ATM_P_2ADD8, ATM_P_ADD16, ATM_2ADDS8R, ATM_ADDS16R, ATM_INC8, ATM_P_INC8, //ARITHMETIC ATOMICS
	ATM_XOR16, ATM_OR16, ATM_NOR16, ATM_AND16, ATM_NAND16, 											//BOOLEAN ATOMICS
	ATM_CASGT8, ATM_CASLT8, ATM_CASGT16, ATM_CASLT16, ATM_CASEQ8, ATM_CASZERO16, ATM_EQ16, ATM_EQ8, //COMPARISON ATOMICS
	ATM_BWR, ATM_P_BWR, ATM_BWR8R, ATM_SWAP16 														//BITWISE ATOMICS
};

class Transaction
{
public:
	//
	//Functions
	//
	Transaction(TransactionType tranType, uint64_t addr, unsigned size, TranStatistic *statis);
	virtual ~Transaction();
	void ReductGlobalID();

	//
	//Fields
	//
	TranTrace *trace;
	TransactionType transactionType;	//Type of transaction (defined above)
	uint64_t address;					//Physical address of request
	unsigned dataSize;					//[byte] Size of data
	unsigned transactionID;				//Unique identifier
	unsigned LNG;
};

ostream& operator<<(ostream &out, const Transaction &t);
}

#endif