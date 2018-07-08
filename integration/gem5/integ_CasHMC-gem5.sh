# Script for integrating CasHMC and gem5
#
#  CasHMC is integrated with gem5 by DRAMSim2 wrapper which is provided by gem5 simulator
#  in order to minimize modification of gem5 files.
#  We used DRAMSim2 wrappper class and ext/dramsim2 folder location in the same way as before.
#  All original DRAMSim2 files are made backup copy [*_backup.*]
#
#  This script file should be run in gem5 root directory
#  (ex: source gem5/integ_CasHMC-gem5.sh)
#
# Copyright (C) 2017, by Dongik Jeon. All rights reserved

# Path(directory) variables
CURRENT_PATH=$(pwd)
MEM_PATH=${CURRENT_PATH}/src/mem
CASHMC_PATH=${CURRENT_PATH}/ext/dramsim2
CONFIG_FILE=${CASHMC_PATH}/CasHMC/ConfigSim.ini
TRANS_SIZE=64		# It should be the same as the last level cache block size


echo ""
echo "======================================================="
echo "  Script for integrating CasHMC and gem5"
date "+  time : %c"
echo "======================================================"
echo ""


echo "----------------- gem5 folder check -----------------"
if [ -d "$MEM_PATH" ]; then
echo " folder exist [${MEM_PATH}]"
if [ -d "$CASHMC_PATH" ]; then
echo " folder exist [${CASHMC_PATH}]"
echo ""


echo "--------------- Backup DRAMSim2 files ---------------"
cd ${MEM_PATH}
if [ -f dramsim2.hh ] && [ ! -f dramsim2_backup.hh ]; then	
	mv dramsim2.hh dramsim2_backup.hh; fi
if [ -f dramsim2.cc ] && [ ! -f dramsim2_backup.cc ]; then
	mv dramsim2.cc dramsim2_backup.cc; fi
if [ -f DRAMSim2.py ] && [ ! -f DRAMSim2_backup.py ]; then
	mv DRAMSim2.py DRAMSim2_backup.py; fi
if [ -f dramsim2_wrapper.hh ] && [ ! -f dramsim2_wrapper_backup.hh ]; then
	mv dramsim2_wrapper.hh dramsim2_wrapper_backup.hh; fi
if [ -f dramsim2_wrapper.cc ] && [ ! -f dramsim2_wrapper_backup.cc ]; then
	mv dramsim2_wrapper.cc dramsim2_wrapper_backup.cc; fi
echo " Creating backup copy of DRAMSim2 wrapper files"
find ${MEM_PATH}/ -name "*_backup*"
echo ""


echo "------------------ Download CasHMC ------------------"
cd ${CASHMC_PATH}
git clone https://github.com/estwings57/CasHMC
cp -a ${CASHMC_PATH}/CasHMC/integration/gem5/mem/* ${MEM_PATH}
if [ -f SConscript ] && [ ! -f SConscript_backup ]; then
	mv ${CASHMC_PATH}/SConscript ${CASHMC_PATH}/SConscript_backup; fi
cp -a ${CASHMC_PATH}/CasHMC/integration/gem5/SConscript_CasHMC ${CASHMC_PATH}/SConscript
echo " -> Transantion size modification"
echo "    for matching cache block and memory request size"
sed -i '29d' ${CONFIG_FILE}
sed -i '29i\TRANSACTION_SIZE = '${TRANS_SIZE}'; 		//[byte] Data size of DRAM request (the internal 32-byte granularity of the DRAM data bus within each vault in the HMC)' ${CONFIG_FILE}
echo " "


echo "--------- Running hello example with CasHMC ---------"
cd ${CURRENT_PATH}
echo " -> Compile gem5"
scons build/ARM/gem5.opt
echo " -> Running hello example"
echo ""
build/ARM/gem5.opt --outdir=./result ./configs/example/se.py --cpu-type=DerivO3CPU --caches --mem-type=DRAMSim2 -c ./tests/test-progs/hello/bin/arm/linux/hello


else
echo " folder not exist [${CASHMC_PATH}]"
fi
else
echo " folder not exist [${MEM_PATH}]"
fi