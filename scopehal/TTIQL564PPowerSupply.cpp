/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "scopehal.h"
#include "TTIQL564PPowerSupply.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TTIQL564PPowerSupply::TTIQL564PPowerSupply(SCPITransport* transport)
	: SCPIDevice(transport)
	, SCPIInstrument(transport)
{
	LogDebug("m_model = %s\n", m_model.c_str());

	m_channels.push_back(
		new PowerSupplyChannel(string("CH") + to_string(1), this, "#808080", 0));
	GetPowerOvercurrentShutdownEnabled(0); // Initialize map
}

TTIQL564PPowerSupply::~TTIQL564PPowerSupply()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device info

string TTIQL564PPowerSupply::GetDriverNameInternal()
{
	return "tti_ql564p";
}

uint32_t TTIQL564PPowerSupply::GetInstrumentTypesForChannel(size_t /*i*/) const
{
	return INST_PSU;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device capabilities

bool TTIQL564PPowerSupply::SupportsSoftStart()
{
	return false;
}

bool TTIQL564PPowerSupply::SupportsIndividualOutputSwitching()
{
	return true;
}

bool TTIQL564PPowerSupply::SupportsMasterOutputSwitching()
{
	return false;
}

bool TTIQL564PPowerSupply::SupportsOvercurrentShutdown()
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual hardware interfacing

#define CHNAME(ch) ("CH" + to_string(ch + 1))
#define SOURCENAME(ch) ("SOURCE" + to_string(ch + 1))

bool TTIQL564PPowerSupply::IsPowerConstantCurrent(int chan)
{
	return false; //TODO probably just check if actual V is less than nominal V
}

double TTIQL564PPowerSupply::GetPowerVoltageActual(int chan)
{
	string reply = m_transport->SendCommandQueuedWithReply("V" + to_string(chan + 1) + "O?");
	reply.pop_back();
	return stof(reply);
}

double TTIQL564PPowerSupply::GetPowerVoltageNominal(int chan)
{
	return stof(m_transport->SendCommandQueuedWithReply("V" + to_string(chan + 1) + "?").substr(3));
}

double TTIQL564PPowerSupply::GetPowerCurrentActual(int chan)
{
	string reply = m_transport->SendCommandQueuedWithReply("I" + to_string(chan + 1) + "O?");
	reply.pop_back();
	return stof(reply);
}

double TTIQL564PPowerSupply::GetPowerCurrentNominal(int chan)
{
	return stof(m_transport->SendCommandQueuedWithReply("I" + to_string(chan + 1) + "?").substr(3));
}

bool TTIQL564PPowerSupply::GetPowerChannelActive(int chan)
{
	return m_transport->SendCommandQueuedWithReply("OP" + to_string(chan + 1) + "?") == "1";
}

void TTIQL564PPowerSupply::SetPowerOvercurrentShutdownEnabled(int chan, bool enable)
{

}

bool TTIQL564PPowerSupply::GetPowerOvercurrentShutdownEnabled(int chan)
{
	return false; //TODO
}

bool TTIQL564PPowerSupply::GetPowerOvercurrentShutdownTripped(int chan)
{
	return false; //TODO
}

void TTIQL564PPowerSupply::SetPowerVoltage(int chan, double volts)
{
	m_transport->SendCommandQueued("V" + to_string(chan + 1) + " " + to_string(volts));
}

void TTIQL564PPowerSupply::SetPowerCurrent(int chan, double amps)
{
	m_transport->SendCommandQueued("I" + to_string(chan + 1) + " " + to_string(amps));
}

void TTIQL564PPowerSupply::SetPowerChannelActive(int chan, bool on)
{
	m_transport->SendCommandQueued("OP" + to_string(chan + 1) + " " + (on ? "1" : "0"));
}
