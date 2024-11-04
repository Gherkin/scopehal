/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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

/**
	@file
	@author Frederic Borry
	@brief Implementation of TinySA

	@ingroup scopedrivers
 */

#include "scopehal.h"
#include "TinySA.h"
#include <cinttypes>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initialize the driver

	@param transport	SCPITransport connected to the SA
 */
TinySA::TinySA(SCPITransport* transport)
	: SCPIDevice(transport,false)
	, SCPIInstrument(transport,false)
{
	std::vector<string> info;
	string version =  ConverseSingle("version");
	if(version.empty())
	{
		LogError("Could not connect to TinySA :-/\n");
		return;
	}
	// Set vendor and version
	m_vendor= "tinySA";
	m_fwVersion = version;
	LogDebug("Version = %s\n",m_fwVersion.c_str());

	// Get model out of first line of info command response
	m_model= ConverseSingle("info");
	LogDebug("Model = %s\n",m_model.c_str());
	if(m_model.find("ULTRA")!= std::string::npos)
		m_tinySAModel = Model::TINY_SA_ULTRA;
	else
		m_tinySAModel = Model::TINY_SA;

	//Add spectrum view channel
	m_channels.push_back(
		new SpectrumChannel(
		this,
		string("CH1"),
		string("#ffff00"),
		m_channels.size()));

	// Default memory depth to 1000 points
	m_sampleDepth = 1000;
	switch (m_tinySAModel)
	{
		case TINY_SA_ULTRA:
			m_freqMin = 0L; 			// Doc says 100kHz, but sweep can start from 0Hz
			m_freqMax = 13000000000L;	// Doc says 6GHz, but sweep seem to be able to go up to 12.0726 GHz => let the device decide
			m_rbwMin  = 200L; 			//200Hz
			m_rbwMax  = 850000L;		//850kHz
			m_modelDbmOffset = 174;
			break;
		case TINY_SA:
			m_freqMin = 0L; 			// Doc says 100kHz, but sweep can start from 0Hz
			m_freqMax = 6000000000L;	// Doc says 350MHz, but might be higher => let the device decide
			m_rbwMin  = 1L; 			//1kHz
			m_rbwMax  = 600000L;		//600kHz
			m_modelDbmOffset = 128;
		default:
			break;
	}
	// Get span information, format is "<start> <stop> <points>"
	ConverseSweep(m_sweepStart,m_sweepStop);
	// Read rbw
	m_rbw = ConverseRbwValue();
	// Init channel range and offet
	SetChannelVoltageRange(0,0,130);
	SetChannelOffset(0,0,50);
}

TinySA::~TinySA()
{
}

/**
 * @brief Converse with the device : send a command and read the reply over several lines
 *
 * @param commandString the command string to send
 * @param readLines a verctor to store the reply lines
 * @return size_t the number of lines received from the device
 */
size_t TinySA::ConverseMultiple(const std::string commandString, std::vector<string> &readLines)
{
	stringstream ss(ConverseString(commandString));
	string curLine;
	bool firstLine = true;
	size_t size = 0;
    while (getline(ss, curLine))
	{
		// Remove remaining \r
		RemoveCR(curLine);
		if(firstLine)
		{
			// First line is always an echo of the sent command
			if(curLine.compare(commandString) != 0)
				LogWarning("Unexpected response \"%s\" to command string \"%s\".\n",curLine.c_str(),commandString.c_str());
			firstLine = false;
		}
        else if (!curLine.empty())
		{
			LogTrace("Pusshing back line \"%s\".\n",curLine.c_str());
            readLines.push_back(curLine);
			size++;
        }
    }
	return size;
}

/**
 * @brief Converse with the device by sending a command and receiving a single line response
 *
 * @param commandString the command string to send
 * @return std::string the received response
 */
std::string TinySA::ConverseSingle(const std::string commandString)
{
	stringstream ss(ConverseString(commandString));
	string result;
	// Read first line (echo of command string)
	getline(ss,result);
	// Remove remaining \r
	RemoveCR(result);
	if(result.compare(commandString) != 0)
		LogWarning("Unexpected response \"%s\" to command string \"%s\".\n",result.c_str(),commandString.c_str());
	// Get second line as result
	getline(ss,result);
	// Remove remaining \r
	RemoveCR(result);
	return result;
}

/**
 * @brief Base method to converse with the device
 *
 * @param commandString the command string to send to the device
 * @return std::string a string containing all the response from the device (may contain several lines separated by \r \n)
 */
std::string TinySA::ConverseString(const std::string commandString)
{
	string result = "";
	// Lock guard
	LogTrace("Sending command: '%s'.\n",commandString.c_str());
	lock_guard<recursive_mutex> lock(m_transportMutex);
	m_transport->SendCommand(commandString+"\r\n");
	// Read untill we get  "ch>\r\n"
	char tmp = ' ';
	size_t bytesRead = 0;
	double start = GetTime();
	while(true)
	{	// Consume response until we find the end delimiter
		if(!m_transport->ReadRawData(1,(unsigned char*)&tmp))
		{
			// We might have to wait for a bit to get a response
			if(GetTime()-start >= COMMUNICATION_TIMEOUT)
			{
				// Timeout
				LogError("A timeout occurred while reading data from device.\n");
				break;
			}
			continue;
		}
		result += tmp;
		bytesRead++;
		if(bytesRead > MAX_RESPONSE_SIZE)
		{
			LogError("Error while reading data from TinySA: response too long (%zu bytes).\n",bytesRead);
			break;
		}
		if(result.size()>=TRAILER_STRING_LENGTH && (0 == result.compare (result.length() - TRAILER_STRING_LENGTH, TRAILER_STRING_LENGTH, TRAILER_STRING)))
			break;
	}
	//LogDebug("Received: %s\n",result.c_str());
	return result;
}

/**
 * @brief Special method used to converse with the device with a binary response (e.g. spanraw command)
 *
 * @param commandString the command string to send
 * @param data a vector to store the binary data received from the device
 * @param length the length of binary data expected from the device
 * @return size_t the number of bytes actually read from the device
 */
size_t TinySA::ConverseBinary(const std::string commandString, std::vector<uint8_t> &data, size_t length)
{
	string header;
	bool inHeader = true;
	bool inFooter = false;
	string result;
	// Lock guard
	lock_guard<recursive_mutex> lock(m_transportMutex);
	m_transport->SendCommand(commandString+"\r\n");
	// Read untill we get  "ch>\r\n"
	char tmp = ' ';
	size_t bytesRead = 0;
	size_t dataRead = 0;
	// Prepare buffer size
	data.resize(length);
	double start = GetTime();
	while(true)
	{
		if(inFooter || inHeader)
		{
			// Consume header and footer as strings
			if(!m_transport->ReadRawData(1,(unsigned char*)&tmp))
			{
				// We might have to wait for the sweep to start to get a response
				if(GetTime()-start >= COMMUNICATION_TIMEOUT)
				{
					// Timeout
					LogError("A timeout occurred while reading data from device.\n");
					break;
				}
				continue;
			}
			bytesRead++;
			if(bytesRead > MAX_RESPONSE_SIZE)
			{
				LogError("Error while reading data from TinySA: response too long (%zu bytes).\n",bytesRead);
				break;
			}
			if(inHeader)
			{
				result += tmp;
				if(result.size()>=EOL_STRING_LENGTH && (0 == result.compare (result.length() - EOL_STRING_LENGTH, EOL_STRING_LENGTH, EOL_STRING)))
				{
					inHeader = false;
					// Check that header matches command string
					if(result.rfind(commandString,0) != 0)
						LogWarning("Unexpected response \"%s\" to command string \"%s\".\n",result.c_str(),commandString.c_str());
					result = "";
				}
			}
			else if (inFooter)
			{
				result += tmp;
				if(result.size()>=TRAILER_STRING_LENGTH && (0 == result.compare (result.length() - TRAILER_STRING_LENGTH, TRAILER_STRING_LENGTH, TRAILER_STRING)))
					break;
			}
		}
		else
		{
			// Read binary data
			dataRead += m_transport->ReadRawData(length,data.begin().base(),[this] (float progress) { ChannelsDownloadStatusUpdate(0, InstrumentChannel::DownloadState::DOWNLOAD_IN_PROGRESS, progress); });
			if(dataRead >= length)
				inFooter = true;
			else if(GetTime()-start >= COMMUNICATION_TIMEOUT)
			{
				// Timeout
				LogError("A timeout occurred while reading data from device.\n");
				break;
			}
		}
	}
	return dataRead;
}

/**
 * @brief Set and/or read the rbw value from the device
 *
 * @param sendValue true if the rbw value hase to be set
 * @param value the value to set
 * @return int64_t the rbw value read from the device
 */
int64_t TinySA::ConverseRbwValue(bool sendValue, int64_t value)
{
	size_t lines;
	vector<string> reply;
	float kHzValue = ((float)value)/1000;
	if(sendValue)
	{
		lines = ConverseMultiple("rbw "+std::to_string(kHzValue),reply);
		if(lines > 1)
		{	// Value was rejected
			LogWarning("Error while sending rbw value %" PRIi64 ": \"%s\".\n",value,reply[0].c_str());
		}
		// Clear reply for next use
		reply.clear();
	}
	// Get currently configured rbw
	lines = ConverseMultiple("rbw",reply);
	if(lines < 2)
	{
		LogWarning("Error while requesting rbw: returned only %zu lines.\n",lines);
		return 0;
	}
	// First line is for usage, get the actual rbw value from second line, the unit can be Hz or KHz
	int64_t rbw;
	sscanf(reply[1].c_str(), "%" SCNi64 "kHz", &rbw);
	bool isKhz = (reply[1].find("kHz") != std::string::npos);
	LogDebug("Found rbw value = %" PRIi64 " %s.\n",rbw,isKhz ? "kHz" : "Hz");
	return isKhz ? (rbw*1000) : rbw;
}

/**
 * @brief Set and/or read the sweep values from the device
 *
 * @param sweepStart the sweep start value (in/out)
 * @param sweepStop the sweep stop value (in/out)
 * @param setValue tru is the values have to be set on the device
 * @return true is the value returned by the device is different from the one that shoudl have been set (e.g. out of range)
 */
bool TinySA::ConverseSweep(int64_t &sweepStart, int64_t &sweepStop, bool setValue)
{
	size_t lines;
	vector<string> reply;
	int64_t origStartValue = sweepStart;
	int64_t origStopValue = sweepStop;
	if(setValue)
	{
		// Send start value
		lines = ConverseMultiple("sweep start "+std::to_string(sweepStart),reply);
		if(lines > 1)
		{	// Value was rejected
			LogWarning("Error while sending sweep start value %" PRIi64 ": \"%s\".\n",sweepStart,reply[0].c_str());
		}
		// Send stop value
		lines = ConverseMultiple("sweep stop "+ std::to_string(sweepStop) ,reply);
		if(lines > 1)
		{	// Value was rejected
			LogWarning("Error while sending sweep stop value %" PRIi64 ": \"%s\".\n",sweepStop,reply[0].c_str());
		}
		// Clear reply for next use
		reply.clear();
	}
	// Get currently configured sweep
	lines = ConverseMultiple("sweep",reply);
	if(lines < 1)
	{
		LogWarning("Error while requesting sweep values: no lines returned.\n");
		return false;
	}
	sscanf(reply[0].c_str(), "%" SCNi64 " %" SCNi64, &sweepStart, &sweepStop);
	LogDebug("Found sweep start %" PRIi64 " / stop %" PRIi64 ".\n",sweepStart,sweepStop);
	return setValue && ((origStartValue != sweepStart) || (origStopValue != sweepStop));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

///@brief Return the constant driver name string "tektronix"
string TinySA::GetDriverNameInternal()
{
	return "tiny_sa";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device interface functions

bool TinySA::AcquireData()
{
	// LogDebug("Acquiring data\n");
	// Store sample depth value
	size_t nsamples = m_sampleDepth;
	string command = "scanraw " + std::to_string(m_sweepStart) + " " + std::to_string(m_sweepStop) + " " + std::to_string(nsamples);
	std::vector<uint8_t> data;
	// Data format is  '{' ('x' MSB LSB)*points '}'
	size_t toRead = nsamples * 3 + 2;
	size_t read = ConverseBinary(command,data,toRead);
	if(read != toRead)
	{
		LogError("Invalid number of acquired bytes: %zu, expected %zu. Ingoring capture.\n",read,toRead);
		return false;
	}

	map<int, vector<WaveformBase*> > pending_waveforms;

	int64_t stepsize = (m_sweepStop - m_sweepStart) / nsamples;

	double tstart = GetTime();
	int64_t fs = (tstart - floor(tstart)) * FS_PER_SECOND;

	//Set up the capture we're going to store our data into
	auto cap = new UniformAnalogWaveform;
	cap->m_timescale = stepsize;
	cap->m_triggerPhase = m_sweepStart;
	cap->m_startTimestamp = floor(tstart);
	cap->m_startFemtoseconds = fs;
	cap->Resize(nsamples);
	cap->PrepareForCpuAccess();

	// Check data opening and closing brackets
	if(data[0] != '{')
		LogWarning("Invalid opening byte '%02x'.\n",data[0]);
	if(data[toRead-1] != '}')
		LogWarning("Invalid closing byte '%02x'.\n",data[toRead-1]);

	//We get dBm from the instrument, so just have to convert double to single precision
	for(size_t j=0; j<nsamples; j++)
	{
		// Read content (3 bytes per point, skipping first byte)
		uint8_t data0 = data[1+3*j];
		uint16_t data1 = (uint16_t)data[2+3*j];
		uint16_t data2 = (uint16_t)data[3+3*j];
		if(data0 != 'x')
			LogWarning("Invalid point header byte '%02x'.\n",data0);
		float value = (((float)((data2 << 8) + data1))/32)-m_modelDbmOffset;
		//LogDebug("Value[%zu] = %02x%02x = %f\n",j,data1,data2,value);
		cap->m_samples[j] = value;
	}

	//Done, update the data
	cap->MarkSamplesModifiedFromCpu();
	pending_waveforms[0].push_back(cap);

	//Look for peaks
	//TODO: make this configurable, for now 500 kHz spacing and up to 10 peaks
	dynamic_cast<SpectrumChannel*>(m_channels[0])->FindPeaks(cap, 10, 500000);

	//Now that we have all of the pending waveforms, save them in sets across all channels
	m_pendingWaveformsMutex.lock();
	size_t num_pending = 1;
	for(size_t i=0; i<num_pending; i++)
	{
		SequenceSet s;
		for(size_t j=0; j<m_channels.size(); j++)
		{
			if(IsChannelEnabled(j))
				s[GetOscilloscopeChannel(j)] = pending_waveforms[j][i];
		}
		m_pendingWaveforms.push_back(s);
	}
	m_pendingWaveformsMutex.unlock();

	if(m_triggerOneShot)
		m_triggerArmed = false;
	//LogDebug("Acquisition done\n");
	// Tell the download monitor that waveform download has finished
	ChannelsDownloadFinished();
	return true;
}

vector<uint64_t> TinySA::GetSampleDepthsNonInterleaved()
{
	vector<uint64_t> ret;
	ret.push_back(51);
	ret.push_back(101);
	ret.push_back(145);
	ret.push_back(290);
	ret.push_back(500);
	ret.push_back(1000);
	ret.push_back(3000);
	ret.push_back(10000);
	ret.push_back(30000);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Spectrum analyzer mode

void TinySA::SetResolutionBandwidth(int64_t rbw)
{
	//Clamp to instrument limits
	m_rbw = max(m_rbwMin, rbw);
	m_rbw = min(m_rbwMax, m_rbw);
	// Send rbw and read actual return
	m_rbw = ConverseRbwValue(true, m_rbw);
}

void TinySA::SetSpan(int64_t span)
{
	//Calculate requested start/stop
	auto freq = GetCenterFrequency(0);
	m_sweepStart = freq - span/2;
	m_sweepStop = freq + span/2;

	//Clamp to instrument limits
	m_sweepStart = max(m_freqMin, m_sweepStart);
	m_sweepStop = min(m_freqMax, m_sweepStop);

	// Send and read back the values to/from the devices to check boundaries
	ConverseSweep(m_sweepStart,m_sweepStop,true);
}

int64_t TinySA::GetSpan()
{
	return m_sweepStop - m_sweepStart;
}

void TinySA::SetCenterFrequency([[maybe_unused]] size_t channel, int64_t freq)
{
	//Calculate requested start/stop
	int64_t span = GetSpan();
	m_sweepStart = freq - span/2;
	m_sweepStop = freq + span/2;

	//Clamp to instrument limits
	m_sweepStart = max(m_freqMin, m_sweepStart);
	m_sweepStop = min(m_freqMax, m_sweepStop);

	// Send and read back the values to/from the devices to check boundaries
	ConverseSweep(m_sweepStart,m_sweepStop,true);
}

int64_t TinySA::GetCenterFrequency([[maybe_unused]] size_t channel)
{
	return (m_sweepStop + m_sweepStart) / 2;
}
