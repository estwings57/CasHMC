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

#include "ConfigReader.h"
	
//Configure values that are extern vaules

//The unique identifier for transaction and packet
unsigned tranGlobalID = 0;
unsigned packetGlobalTAG = 0;

//
//SimConfig.ini
//
uint64_t LOG_EPOCH;
bool DEBUG_SIM;
bool ONLY_CR;
bool STATE_SIM;
int PLOT_SAMPLING;
bool BANDWIDTH_PLOT;

double CPU_CLK_PERIOD;
int TRANSACTION_SIZE;
int MAX_REQ_BUF;

int NUM_LINKS;
int LINK_WIDTH;
double LINK_SPEED;
int MAX_LINK_BUF;
int MAX_RETRY_BUF;
int MAX_VLT_BUF;
int MAX_CROSS_BUF;
int MAX_CMD_QUE;
bool CRC_CHECK;
double CRC_CAL_CYCLE;
int NUM_OF_IRTRY;
int RETRY_ATTEMPT_LIMIT;
int LINK_BER;
LINK_PRIORITY_SCHEME LINK_PRIORITY;
LINK_POWER_MANAGEMENT LINK_POWER;
int AWAKE_REQ;
double LINK_EPOCH;
double MSHR_SCALING;
double LINK_SCALING;

double PowPerLane;
double SleepPow;
double DownPow;

double tPST;
double tSME;
double tSS;
double tSD;
double tSREF;
double tOP;
double tQUIESCE;

double tTXD;
double tRESP1;
double tRESP2;
double tPSC;

//
//DRAMConfig.ini
//
int MEMORY_DENSITY;
int NUM_VAULTS;
int NUM_BANKS;
int NUM_ROWS;
int NUM_COLS;
MAPPING_SCHEME ADDRESS_MAPPING;
bool QUE_PER_BANK;
bool OPEN_PAGE;
int MAX_ROW_ACCESSES;
bool USE_LOW_POWER;

int REFRESH_PERIOD;

double tCK;
unsigned CWL;
unsigned CL;
unsigned AL;
unsigned tRAS;
unsigned tRCD;
unsigned tRRD;
unsigned tRC;
unsigned tRP;
unsigned tCCD;
unsigned tRTP;
unsigned tWTR;
unsigned tWR;
unsigned tRTRS;
unsigned tRFC;
unsigned tFAW;
unsigned tCKE;
unsigned tXP;
unsigned tCMD;

#define DEFINE_PARAM(type, name) {#name, &name, type, false}

namespace CasHMC
{

static ConfigMap configMap[] =
{
	//SimConfig.ini
	DEFINE_PARAM(UINT64, LOG_EPOCH),		DEFINE_PARAM(BOOL, DEBUG_SIM),
	DEFINE_PARAM(BOOL, ONLY_CR),			DEFINE_PARAM(BOOL, STATE_SIM),
	DEFINE_PARAM(INT, PLOT_SAMPLING),		DEFINE_PARAM(BOOL, BANDWIDTH_PLOT),
	DEFINE_PARAM(DOUBLE, CPU_CLK_PERIOD),	DEFINE_PARAM(INT, TRANSACTION_SIZE),
	DEFINE_PARAM(INT, MAX_REQ_BUF),			DEFINE_PARAM(INT, NUM_LINKS),
	DEFINE_PARAM(INT, LINK_WIDTH),			DEFINE_PARAM(DOUBLE, LINK_SPEED),
	DEFINE_PARAM(INT, MAX_LINK_BUF),		DEFINE_PARAM(INT, MAX_RETRY_BUF),
	DEFINE_PARAM(INT, MAX_VLT_BUF),			DEFINE_PARAM(INT, MAX_CROSS_BUF),
	DEFINE_PARAM(INT, MAX_CMD_QUE),			DEFINE_PARAM(BOOL, CRC_CHECK),
	DEFINE_PARAM(DOUBLE, CRC_CAL_CYCLE),	DEFINE_PARAM(INT, NUM_OF_IRTRY),
	DEFINE_PARAM(INT, RETRY_ATTEMPT_LIMIT),	DEFINE_PARAM(INT, LINK_BER),
	DEFINE_PARAM(STRING, LINK_PRIORITY),	DEFINE_PARAM(STRING, LINK_POWER),
	DEFINE_PARAM(INT, AWAKE_REQ),			DEFINE_PARAM(DOUBLE, LINK_EPOCH),
	DEFINE_PARAM(DOUBLE, MSHR_SCALING),		DEFINE_PARAM(DOUBLE, LINK_SCALING),
	DEFINE_PARAM(DOUBLE, PowPerLane),		DEFINE_PARAM(DOUBLE, SleepPow),
	DEFINE_PARAM(DOUBLE, DownPow),			DEFINE_PARAM(DOUBLE, tPST),
	DEFINE_PARAM(DOUBLE, tSME),				DEFINE_PARAM(DOUBLE, tSS),
	DEFINE_PARAM(DOUBLE, tSD),				DEFINE_PARAM(DOUBLE, tSREF),
	DEFINE_PARAM(DOUBLE, tOP),				DEFINE_PARAM(DOUBLE, tQUIESCE),
	DEFINE_PARAM(DOUBLE, tTXD),				DEFINE_PARAM(DOUBLE, tRESP1),
	DEFINE_PARAM(DOUBLE, tRESP2),			DEFINE_PARAM(DOUBLE, tPSC),
	
	//DRAMConfig.ini
	DEFINE_PARAM(INT, MEMORY_DENSITY),		DEFINE_PARAM(INT, NUM_VAULTS),
	DEFINE_PARAM(INT, NUM_BANKS),			DEFINE_PARAM(INT, NUM_ROWS),
	DEFINE_PARAM(INT, NUM_COLS),			DEFINE_PARAM(STRING, ADDRESS_MAPPING),
	DEFINE_PARAM(BOOL, QUE_PER_BANK),		DEFINE_PARAM(BOOL, OPEN_PAGE),
	DEFINE_PARAM(INT, MAX_ROW_ACCESSES),	DEFINE_PARAM(BOOL, USE_LOW_POWER),
	DEFINE_PARAM(INT, REFRESH_PERIOD),		DEFINE_PARAM(DOUBLE, tCK),
	DEFINE_PARAM(UNSIGNED_CLK, CWL),		DEFINE_PARAM(UNSIGNED_CLK, CL),
	DEFINE_PARAM(UNSIGNED_CLK, AL),			DEFINE_PARAM(UNSIGNED_CLK, tRAS),
	DEFINE_PARAM(UNSIGNED_CLK, tRCD),		DEFINE_PARAM(UNSIGNED_CLK, tRRD),
	DEFINE_PARAM(UNSIGNED_CLK, tRC),		DEFINE_PARAM(UNSIGNED_CLK, tRP),
	DEFINE_PARAM(UNSIGNED_CLK, tCCD),		DEFINE_PARAM(UNSIGNED_CLK, tRTP),
	DEFINE_PARAM(UNSIGNED_CLK, tWTR),		DEFINE_PARAM(UNSIGNED_CLK, tWR),
	DEFINE_PARAM(UNSIGNED_CLK, tRTRS),		DEFINE_PARAM(UNSIGNED_CLK, tRFC),
	DEFINE_PARAM(UNSIGNED_CLK, tFAW),		DEFINE_PARAM(UNSIGNED_CLK, tCKE),
	DEFINE_PARAM(UNSIGNED_CLK, tXP),		DEFINE_PARAM(UNSIGNED_CLK, tCMD),
	//end of list
	{"", NULL, BOOL, false}
};

//
//Read configure field values
//
void ReadIniFile(string file_name)
{
	fstream file_stream(file_name.c_str(), ios::in);
    if(!file_stream.good()) {
		ERROR("CasHMC could not open "<<file_name<<" for reading");
		exit(0);
	}
	
	string line, field_name, field_value;
	int commentIdx, equalsIdx, spaceIdx;
	unsigned int lineNumber=0;
	
	while(!file_stream.eof())
	{
		lineNumber++;
		getline(file_stream, line);
		//skip zero-length lines
		if(line.size() == 0) {
			//DEBUG("Skipping blank line "<<lineNumber);
			continue;
		}
		//search for a comment
		commentIdx = line.find_first_of(";");
		if(commentIdx != string::npos) {
			if(commentIdx == 0) {
				//DEBUG("Skipping comment line "<<lineNumber);
				continue;
			}
			line = line.substr(0, commentIdx);
		}
		else {
			continue;
		}
		//a line has to have an equals sign
		equalsIdx = line.find_first_of("=");
		if(equalsIdx == string::npos) {
			//DEBUG("Skipping comment line "<<lineNumber);
			continue;
		}

		//extract field value
		field_value = line.substr(equalsIdx+1, line.size());
		spaceIdx = field_value.find_last_not_of(" ");
		field_value = field_value.substr(0, spaceIdx+1);
		spaceIdx = field_value.find_last_of(" ");
		field_value = field_value.substr(spaceIdx+1, field_value.size());
		 
		//extract field name
		line = line.substr(0, equalsIdx);
		spaceIdx = line.find_last_not_of(" ");
		line = line.substr(0, spaceIdx+1);
		spaceIdx = line.find_last_of(" ");
		field_name = line.substr(spaceIdx+1, line.size());

		//cout<<"["<<lineNumber<<"] "<<field_name<<" : "<<field_value<<endl;
		SetField(field_name, field_value);
	}
    file_stream.close();
}

//
//Setting field value from ini file to variable
//
void SetField(string field_name, string field_value)
{
	int i;
	istringstream val_st(field_value);
	
	for(i=0; configMap[i].fieldPtr != NULL; i++) {
		if(field_name.compare(configMap[i].fieldName) == 0) {
			switch(configMap[i].fieldType) {
			case STRING: {
				//LINK_PRIORITY_SCHEME
				if(configMap[i].fieldName == "LINK_PRIORITY") {
					if(field_value == "ROUND_ROBIN") {
						LINK_PRIORITY = ROUND_ROBIN;
					}
					else if(field_value == "BUFFER_AWARE") {
						LINK_PRIORITY = BUFFER_AWARE;
					}
					else {
						ERROR(" == Error - Unknown field_value ["<<field_value<<"] for field_name ["<<field_name<<"] in ini file");
						exit(0);
					}
				}
				//LINK_POWER_MANAGEMENT
				else if(configMap[i].fieldName == "LINK_POWER") {
					if(field_value == "NO_MANAGEMENT") {
						LINK_POWER = NO_MANAGEMENT;
					}
					else if(field_value == "QUIESCE_SLEEP") {
						LINK_POWER = QUIESCE_SLEEP;
					}
					else if(field_value == "MSHR") {
						LINK_POWER = MSHR;
					}
					else if(field_value == "LINK_MONITOR") {
						LINK_POWER = LINK_MONITOR;
					}
					else if(field_value == "AUTONOMOUS") {
						LINK_POWER = AUTONOMOUS;
					}
					else {
						ERROR(" == Error - Unknown field_value ["<<field_value<<"] for field_name ["<<field_name<<"] in ini file");
						exit(0);
					}
				}
				//MAPPING_SCHEME
				else if(configMap[i].fieldName == "ADDRESS_MAPPING") {
					if(field_value == "MAX_BLOCK_32B") {
						ADDRESS_MAPPING = MAX_BLOCK_32B;
					}
					else if(field_value == "MAX_BLOCK_64B") {
						ADDRESS_MAPPING = MAX_BLOCK_64B;
					}
					else if(field_value == "MAX_BLOCK_128B") {
						ADDRESS_MAPPING = MAX_BLOCK_128B;
					}
					else if(field_value == "MAX_BLOCK_256B") {
						ADDRESS_MAPPING = MAX_BLOCK_256B;
					}
					else {
						ERROR(" == Error - Unknown field_value ["<<field_value<<"] for field_name ["<<field_name<<"] in ini file");
						exit(0);
					}
				}
				break;
			}
			case BOOL: {
				if(field_value == "true" || field_value == "1") {
					*((bool *)configMap[i].fieldPtr) = true;
				}
				else {
					*((bool *)configMap[i].fieldPtr) = false;
				}
				break;
			}
			case INT: {
				int val;
				val_st >> val;
				*((int *)configMap[i].fieldPtr) = val;
				break;
			}
			case UNSIGNED_CLK: {
				double val;
				val_st >> val;
				unsigned val_by_clk = ceil(val/tCK);
				*((unsigned *)configMap[i].fieldPtr) = val_by_clk;
				break;
			}
			case UINT64: {
				uint64_t val;
				val_st >> val;
				*((uint64_t *)configMap[i].fieldPtr) = val;
				break;
			}
			case DOUBLE: {
				double val;
				val_st >> val;
				*((double *)configMap[i].fieldPtr) = val;
				break;
			}
			}
			configMap[i].wasSet = true;
			break;
		}
	}
	
	if(configMap[i].fieldPtr == NULL) {
		ERROR(" == Error - Unknown field ["<<field_name<<"] in ini file");
		exit(0);
	}
}

} //namespace CasHMC