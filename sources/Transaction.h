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

#ifndef TRANSACTION_H
#define TRANSACTION_H

//Transaction.h

#include <stdint.h>		//uint64_t
#include <stdlib.h>		//exit(0)
#include <iomanip>		//setw()
#include <iostream> 	//ostream
#include <sstream>		//stringstream

#include "SimConfig.h"
#include "TranTrace.h"
#include "TranStatistic.h"

using namespace std;

namespace CasHMC
{

static unsigned tranGlobalID=0;

enum TransactionType
{
	DATA_READ,
	DATA_WRITE,
	RETURN_DATA
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
