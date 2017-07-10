/*
 * Copyright (c) 2013 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Andreas Hansson
 */

#include <cassert>

#include "mem/dramsim2_wrapper.hh"

#include <fstream>

#include "base/compiler.hh"
#include "base/misc.hh"

DRAMSim2Wrapper::DRAMSim2Wrapper(const std::string& configSim_file,
                                 const std::string& configDRAM_file,
                                 const std::string& working_dir) :
    casHMCWrapper(new CasHMC::CasHMCWrapper(working_dir+'/'+configSim_file, working_dir+'/'+configDRAM_file)),
    _clockPeriod(0.0), _queueSize(0), _burstSize(0)
{
    // there is no way of getting CasHMC to tell us what frequency
    // it is assuming, so we have to extract it ourselves
	_clockPeriod = extractConfig<double>("tCK =", working_dir + '/' + configDRAM_file);

    if (!_clockPeriod)
        fatal("CasHMC wrapper failed to get clock\n");

    // we also need to know what transaction queue size CasHMC is
    // using so we can stall when responses are blocked
	_queueSize = extractConfig<unsigned int>("MAX_REQ_BUF =", working_dir + '/' + configSim_file);

    if (!_queueSize)
        fatal("CasHMC wrapper failed to get queue size\n");


   // finally, get the data bus bits and burst length so we can add a
   // sanity check for the burst size
    unsigned int dataBusBits =
        extractConfig<unsigned int>("TRANSACTION_SIZE = ", working_dir + '/' + configSim_file);

	unsigned int burstLength = 1;

   if (!dataBusBits || !burstLength)
       fatal("CasHMC wrapper failed to get burst size\n");

   _burstSize = dataBusBits * burstLength;
}

DRAMSim2Wrapper::~DRAMSim2Wrapper()
{
    delete casHMCWrapper;
}

template <typename T>
T
DRAMSim2Wrapper::extractConfig(const std::string& field_name,
                               const std::string& file_name) const
{
    std::ifstream file_stream(file_name.c_str(), ios::in);

    if (!file_stream.good())
        fatal("CasHMC wrapper could not open %s for reading\n", file_name);

    bool found = false;
    T res;
    std::string line;
    while (!found && file_stream) {
        getline(file_stream, line);
        if (line.substr(0, field_name.size()) == field_name) {
            found = true;
            istringstream iss(line.substr(field_name.size()));
            iss >> res;
        }
    }

    file_stream.close();

    if (!found)
        fatal("CasHMC wrapper could not find %s in %s\n", field_name,
              file_name);

    return res;
}

void
DRAMSim2Wrapper::printStats()
{
	casHMCWrapper->PrintEpochStatistic();
	casHMCWrapper->PrintFinalStatistic();
}

void
DRAMSim2Wrapper::setCallbacks(CasHMC::TransCompCB* read_callback,
                              CasHMC::TransCompCB* write_callback)
{
	casHMCWrapper->RegisterCallbacks(read_callback, write_callback);
}

bool
DRAMSim2Wrapper::canAccept() const
{
    return casHMCWrapper->CanAcceptTran();
}

void
DRAMSim2Wrapper::enqueue(bool is_write, uint64_t addr)
{
    bool success M5_VAR_USED = casHMCWrapper->ReceiveTran((is_write ? CasHMC::DATA_WRITE : CasHMC::DATA_READ), addr, _burstSize); 
    assert(success);
}

double
DRAMSim2Wrapper::clockPeriod() const
{
    return _clockPeriod;
}

unsigned int
DRAMSim2Wrapper::queueSize() const
{
    return _queueSize;
}

unsigned int
DRAMSim2Wrapper::burstSize() const
{
    return _burstSize;
}

void
DRAMSim2Wrapper::tick()
{
	casHMCWrapper->Update();
}
