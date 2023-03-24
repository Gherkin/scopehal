/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
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

/**
	@file
	@author Andrew D. Zonenberg
	@brief Declaration of DPAuxChannelDecoder
 */
#ifndef DPAuxChannelDecoder_h
#define DPAuxChannelDecoder_h

class DPAuxSymbol
{
public:
	enum stype
	{
		TYPE_ERROR,
		TYPE_PREAMBLE,
		TYPE_SYNC,
		TYPE_COMMAND,
		TYPE_ADDRESS,
		TYPE_LEN,
		TYPE_STOP
	};

	DPAuxSymbol()
	{}

	DPAuxSymbol(stype t, uint32_t d = 0)
	 : m_stype(t)
	 , m_data(d)
	{}

	stype m_stype;
	uint32_t m_data;

	bool operator== (const DPAuxSymbol& s) const
	{
		return (m_stype == s.m_stype) && (m_data == s.m_data);
	}
};

class DPAuxWaveform : public SparseWaveform<DPAuxSymbol>
{
public:
	DPAuxWaveform () : SparseWaveform<DPAuxSymbol>() {};
	virtual std::string GetText(size_t) override;
	virtual std::string GetColor(size_t) override;
};

class DPAuxChannelDecoder : public PacketDecoder
{
public:
	DPAuxChannelDecoder(const std::string& color);
	virtual ~DPAuxChannelDecoder();

	virtual void Refresh();
	static std::string GetProtocolName();

	virtual bool ValidateChannel(size_t i, StreamDescriptor stream);
	virtual std::vector<std::string> GetHeaders();

	PROTOCOL_DECODER_INITPROC(DPAuxChannelDecoder)

protected:
	bool FindFallingEdge(size_t& i, UniformAnalogWaveform* cap);
	bool FindRisingEdge(size_t& i, UniformAnalogWaveform* cap);

	bool FindEdge(size_t& i, UniformAnalogWaveform* cap, bool polarity)
	{
		if(polarity)
			return FindRisingEdge(i, cap);
		else
			return FindFallingEdge(i, cap);
	}
};

#endif