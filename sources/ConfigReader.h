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

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

//ConfigReader.h

#include <string>
#include <sstream>
#include <fstream>
#include <math.h>		//ceil()
#include <stdlib.h>		//exit()

#include "ConfigValue.h"

using namespace std;

namespace CasHMC
{

typedef enum _variableType {STRING, BOOL, INT, UNSIGNED_CLK, UINT64, DOUBLE} varType;
typedef struct _configMap
{
	string fieldName;
	void *fieldPtr;
	varType fieldType;
	bool wasSet;
} ConfigMap;

void ReadIniFile(string file_name);
void SetField(string field_name, string field_value);

}

#endif