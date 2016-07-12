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

#include "Transaction.h"

using namespace std;

namespace CasHMC
{
	
Transaction::Transaction(TransactionType tranType, uint64_t addr, unsigned size, TranStatistic *statis):
	transactionType(tranType),
	address(addr),
	dataSize(size)
{
	LNG = 1;
	transactionID = tranGlobalID++;
	trace = new TranTrace(statis);
}

Transaction::~Transaction()
{
}

//
//Reduction tranGlobalID
//
void Transaction::ReductGlobalID()
{
	tranGlobalID--;
}	

//
//Defines "<<" operation for printing
//
ostream& operator<<(ostream &out, const Transaction &t)
{
	string header;
	stringstream id;
	id << t.transactionID;

	switch(t.transactionType) {
		case DATA_READ:
			header = "[T" + id.str() + "-Read]";
			break;
		case DATA_WRITE:
			header = "[T" + id.str() + "-Write]";
			break;
		case RETURN_DATA:
			header = "[T" + id.str() + "-Data]";
			break;
		default:
			ERROR(" (TS) == Error - Trying to print unknown kind of transaction type");
			ERROR("         T"<<t.transactionID<<" [?"<<t.transactionType<<"?] [0x"<<hex<<setw(16)<<setfill('0')<<t.address<<dec<<"]");
			exit(0);
	}
	out<<header;
	return out;
}

} //namespace CasHMC
