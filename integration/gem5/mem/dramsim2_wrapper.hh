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

/**
 * @file
 * DRAMSim2Wrapper declaration for CasHMC wrapper
 */

#ifndef __MEM_CASHMC_WRAPPER_HH__
#define __MEM_CASHMC_WRAPPER_HH__

#include <string>

#include "CasHMC/sources/CasHMCWrapper.h"
#include "CasHMC/sources/CallBack.h"
#include "CasHMC/sources/Transaction.h"

/**
 * Wrapper class to avoid having CasHMC names like ClockDomain etc
 * clashing with the normal gem5 world. Many of the CasHMC headers
 * do not make use of namespaces, and quite a few also open up
 * std. The only thing that needs to be exposed externally are the
 * callbacks. This wrapper effectively avoids clashes by not including
 * any of the conventional gem5 headers (e.g. Packet or SimObject).
 */
class DRAMSim2Wrapper
{

  private:

    CasHMC::CasHMCWrapper *casHMCWrapper;

    double _clockPeriod;

    unsigned int _queueSize;

    unsigned int _burstSize;

    template <typename T>
    T extractConfig(const std::string& field_name,
                    const std::string& file_name) const;

  public:

    /**
     * Create an instance of the CasHMC multi-channel memory
     * controller using a specific config and system description.
     *
     * @param configSim_file simulation config file
     * @param configDRAM_file DRAM config file
     * @param working_dir Path pre-pended to config files
     */
    DRAMSim2Wrapper(const std::string& configSim_file,
                    const std::string& configDRAM_file,
                    const std::string& working_dir);
    ~DRAMSim2Wrapper();

    /**
     * Print the stats gathered in CasHMC.
     */
    void printStats();

    /**
     * Set the callbacks to use for read and write completion.
     *
     * @param read_callback Callback used for read completions
     * @param write_callback Callback used for write completions
     */
    void setCallbacks(CasHMC::TransCompCB* read_callback,
                      CasHMC::TransCompCB* write_callback);

    /**
     * Determine if the controller can accept a new packet or not.
     *
     * @return true if the controller can accept transactions
     */
    bool canAccept() const;

    /**
     * Enqueue a packet. This assumes that canAccept has returned true.
     *
     * @param pkt Packet to turn into a CasHMC transaction
     */
    void enqueue(bool is_write, uint64_t addr);

    /**
     * Get the internal clock period used by CasHMC, specified in
     * ns.
     *
     * @return The clock period of the DRAM interface in ns
     */
    double clockPeriod() const;

    /**
     * Get the transaction queue size used by CasHMC.
     *
     * @return The queue size counted in number of transactions
     */
    unsigned int queueSize() const;

    /**
     * Get the burst size in bytes used by CasHMC.
     *
     * @return The burst size in bytes (data width * burst length)
     */
    unsigned int burstSize() const;

    /**
     * Progress the memory controller one cycle
     */
    void tick();
};

#endif //__MEM_DRAMSIM2_WRAPPER_HH__