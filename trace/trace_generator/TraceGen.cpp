#include <getopt.h>		//getopt_long
#include <stdint.h>		//uint64_t
#include <stdlib.h>		//exit(0)
#include <fstream>		//ofstream
#include <iostream>		//cout
#include <fstream>		//ofstream
#include <sstream>		//stringstream
#include <iomanip>		//setw
#include <unistd.h>		//access()

using namespace std;

ofstream traceOut;
long numSimCycles = 100000;
uint64_t lineNumber = 1;
double memUtil = 0.1;
double rwRatio = 80;
int dataSize = 32;

void help()
{
	cout<<endl<<"-c (--cycle)   : The number of CPU cycles to be simulated"<<endl;
	cout<<"-u (--util)    : Requests frequency (0 = no requests, 1 = as fast as possible) [Default 0.1]"<<endl;
	cout<<"-r (--rwratio) : (%) The percentage of reads in request stream [Default 80]"<<endl;
	cout<<"-s (--size)    : Data size of DRAM request ('0' means random data size) [Default 32]"<<endl<<endl;
	cout<<"-h (--help)    : Simulation option help"<<endl<<endl;
}

int main(int argc, char **argv)
{
	//
	//parse command-line options setting
	//
	int opt;
	string pwdString = "";
	while(1) {
		static struct option long_options[] = {
			{"pwd", required_argument, 0, 'p'},
			{"cycle",  required_argument, 0, 'c'},
			{"util",  required_argument, 0, 'u'},
			{"rwratio",  required_argument, 0, 'r'},
			{"size",  required_argument, 0, 's'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};
		int option_index=0;
		opt = getopt_long (argc, argv, "p:c:u:r:s:h", long_options, &option_index);
		if(opt == -1) {
			break;
		}
		switch(opt) {
			case 0:
				if (long_options[option_index].flag != 0) {
					cout<<"setting flag"<<endl;
					break;
				}
				cout<<"option "<<long_options[option_index].name<<endl;
				if (optarg) {
					cout<<" with arg"<<optarg<<endl;
				}
				cout<<""<<endl;
				break;
			case 'p':
				pwdString = string(optarg);
				break;
			case 'c':
				numSimCycles = atol(optarg);
				break;
			case 'u':
				memUtil = atof(optarg);
				if(memUtil < 0 || memUtil > 1) {
					cout<<endl<<" == -u (--util) [-u "<<memUtil<<"] ERROR ==";
					cout<<endl<<"  This value must be in the between '0' and '1'"<<endl<<endl;
					exit(0);
				}
				break;
			case 'r':
				rwRatio = atof(optarg);
				if(rwRatio < 0 || rwRatio > 100) {
					cout<<endl<<" == -r (--rwratio) [-r "<<rwRatio<<"] ERROR ==";
					cout<<endl<<"  This value is the percentage of reads in request stream"<<endl<<endl;
					exit(0);
				}
				break;
			case 's':
				dataSize = atol(optarg);
				if(dataSize!=16 && dataSize!=32 && dataSize!=48 && dataSize!=64 && dataSize!=80 &&
					dataSize!=96 && dataSize!=112 && dataSize!=128 && dataSize!=256 && dataSize!=0) {
					cout<<endl<<" == -s (--size) [-s "<<dataSize<<"] ERROR ==";
					cout<<endl<<"  size should be 16, 32, 48, 64, 80, 96, 112, 128, 256, or 0"<<endl<<endl;
					exit(0);
				}
				break;
			case 'h':
			case '?':
				help();
				exit(0);
				break;
		}
	}

	unsigned int ver_num = 0;
	stringstream temp_vn;
	string traceName;
	while(1) { 
		traceName += "CasHMC_trace_no";
		temp_vn << ver_num;
		traceName += temp_vn.str();
		traceName += ".trc";
		
		if(access(traceName.c_str(), 0) == -1)	break;
		else {
			traceName.erase(traceName.find("CasHMC"));
			temp_vn.str( string() );
			temp_vn.clear();
			ver_num++;
		}
	}
	traceOut.open(traceName.c_str());
	srand((unsigned)time(NULL));
	
	for(uint64_t cpuCycle=0; cpuCycle<numSimCycles; cpuCycle++) {
		int rand_tran = rand()%10000+1;
		if(rand_tran <= (int)(memUtil*10000)) {
			uint64_t physicalAddress = rand();
			physicalAddress = (physicalAddress<<32)|rand();
			
			if(dataSize==16 || dataSize==32 || dataSize==48 || dataSize==64 || dataSize==80 ||
				dataSize==96 || dataSize==112 || dataSize==128 || dataSize==256) {
				if(physicalAddress%101 <= (int)rwRatio) {
					traceOut<<cpuCycle<<" 0x"<<right<<setw(16)<<setfill('0')<<hex<<physicalAddress
								<<dec<<" READ "<<dataSize<<" "<<endl;
				}
				else {
					traceOut<<cpuCycle<<" 0x"<<right<<setw(16)<<setfill('0')<<hex<<physicalAddress
								<<dec<<" WRITE "<<dataSize<<" "<<endl;
				}
			}
			else {
				int size = 0;
				switch(rand_tran%9) {
					case 0:	size = 16;		break;
					case 1:	size = 32;		break;
					case 2:	size = 48;		break;
					case 3:	size = 64;		break;
					case 4:	size = 80;		break;
					case 5:	size = 96;		break;
					case 6:	size = 112;		break;
					case 7:	size = 128;		break;
					case 8:	size = 256;		break;
					default:
						cout<<"  == Error - WRONG transaction data size"<<endl;
						exit(0);
				}
				if(physicalAddress%101 <= (int)rwRatio) {
					traceOut<<cpuCycle<<" 0x"<<right<<setw(16)<<setfill('0')<<hex<<physicalAddress
								<<dec<<" READ "<<size<<" "<<endl;
				}
				else {
					traceOut<<cpuCycle<<" 0x"<<right<<setw(16)<<setfill('0')<<hex<<physicalAddress
								<<dec<<" WRITE "<<size<<" "<<endl;
				}
			}
		}
	}

	traceOut.flush();
	traceOut.close();
	return 0;
}