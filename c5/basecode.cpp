// basecode.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"
#include "basecode.h"
#include "fltrimpl.h"
#include <ctype.h>

NAMESPACE_BEGIN(CryptoPP)

void BaseN_Encoder::IsolatedInitialize(const NameValuePairs &parameters)
{
	parameters.GetRequiredParameter("BaseN_Encoder", "EncodingLookupArray", m_alphabet);

	parameters.GetRequiredIntParameter("BaseN_Encoder", "Log2Base", m_bitsPerChar);
	if (m_bitsPerChar <= 0 || m_bitsPerChar >= 8)
		throw InvalidArgument("BaseN_Encoder: Log2Base must be between 1 and 7 inclusive");

	byte padding;
	bool pad;
	if (parameters.GetValue("PaddingByte", padding))
		pad = parameters.GetValueWithDefault("Pad", true);
	else
		pad = false;
	m_padding = pad ? padding : -1;

	m_bytePos = m_bitPos = 0;

	int i = 8;
	while (i%m_bitsPerChar != 0)
		i += 8;
	m_outputBlockSize = i/m_bitsPerChar;

	m_outBuf.New(m_outputBlockSize);
}

unsigned int BaseN_Encoder::Put2(const byte *begin, unsigned int length, int messageEnd, bool blocking)
{
	FILTER_BEGIN;
	while (m_inputPosition < length)
	{
		if (m_bytePos == 0)
			memset(m_outBuf, 0, m_outputBlockSize);

		{
		unsigned int b = begin[m_inputPosition++], bitsLeftInSource = 8;
		while (true)
		{
			assert(m_bitPos < m_bitsPerChar);
			unsigned int bitsLeftInTarget = m_bitsPerChar-m_bitPos;
			m_outBuf[m_bytePos] |= b >> (8-bitsLeftInTarget);
			if (bitsLeftInSource >= bitsLeftInTarget)
			{
				m_bitPos = 0;
				++m_bytePos;
				bitsLeftInSource -= bitsLeftInTarget;
				if (bitsLeftInSource == 0)
					break;
				b <<= bitsLeftInTarget;
				b &= 0xff;
			}
			else
			{
				m_bitPos += bitsLeftInSource;
				break;
			}
		}
		}

		assert(m_bytePos <= m_outputBlockSize);
		if (m_bytePos == m_outputBlockSize)
		{
			int i;
			for (i=0; i<m_bytePos; i++)
			{
				assert(m_outBuf[i] < (1 << m_bitsPerChar));
				m_outBuf[i] = m_alphabet[m_outBuf[i]];
			}
			FILTER_OUTPUT(1, m_outBuf, m_outputBlockSize, 0);
			
			m_bytePos = m_bitPos = 0;
		}
	}
	if (messageEnd)
	{
		if (m_bitPos > 0)
			++m_bytePos;

		int i;
		for (i=0; i<m_bytePos; i++)
			m_outBuf[i] = m_alphabet[m_outBuf[i]];

		if (m_padding != -1 && m_bytePos > 0)
		{
			memset(m_outBuf+m_bytePos, m_padding, m_outputBlockSize-m_bytePos);
			m_bytePos = m_outputBlockSize;
		}
		FILTER_OUTPUT(2, m_outBuf, m_bytePos, messageEnd);
		m_bytePos = m_bitPos = 0;
	}
	FILTER_END_NO_MESSAGE_END;
}

void BaseN_Decoder::IsolatedInitialize(const NameValuePairs &parameters)
{
	parameters.GetRequiredParameter("BaseN_Decoder", "DecodingLookupArray", m_lookup);

	parameters.GetRequiredIntParameter("BaseN_Decoder", "Log2Base", m_bitsPerChar);
	if (m_bitsPerChar <= 0 || m_bitsPerChar >= 8)
		throw InvalidArgument("BaseN_Decoder: Log2Base must be between 1 and 7 inclusive");

	m_bytePos = m_bitPos = 0;

	int i = m_bitsPerChar;
	while (i%8 != 0)
		i += m_bitsPerChar;
	m_outputBlockSize = i/8;

	m_outBuf.New(m_outputBlockSize);
}

unsigned int BaseN_Decoder::Put2(const byte *begin, unsigned int length, int messageEnd, bool blocking)
{
	FILTER_BEGIN;
	while (m_inputPosition < length)
	{
		unsigned int value;
		value = m_lookup[begin[m_inputPosition++]];
		if (value >= 256)
			continue;

		if (m_bytePos == 0 && m_bitPos == 0)
			memset(m_outBuf, 0, m_outputBlockSize);

		{
			int newBitPos = m_bitPos + m_bitsPerChar;
			if (newBitPos <= 8)
				m_outBuf[m_bytePos] |= value << (8-newBitPos);
			else
			{
				m_outBuf[m_bytePos] |= value >> (newBitPos-8);
				m_outBuf[m_bytePos+1] |= value << (16-newBitPos);
			}

			m_bitPos = newBitPos;
			while (m_bitPos >= 8)
			{
				m_bitPos -= 8;
				++m_bytePos;
			}
		}

		if (m_bytePos == m_outputBlockSize)
		{
			FILTER_OUTPUT(1, m_outBuf, m_outputBlockSize, 0);
			m_bytePos = m_bitPos = 0;
		}
	}
	if (messageEnd)
	{
		FILTER_OUTPUT(2, m_outBuf, m_bytePos, messageEnd);
		m_bytePos = m_bitPos = 0;
	}
	FILTER_END_NO_MESSAGE_END;
}

void BaseN_Decoder::InitializeDecodingLookupArray(int *lookup, const byte *alphabet, unsigned int base, bool caseInsensitive)
{
	std::fill(lookup, lookup+256, -1);

	for (unsigned int i=0; i<base; i++)
	{
		if (caseInsensitive && isalpha(alphabet[i]))
		{
			assert(lookup[toupper(alphabet[i])] == -1);
			lookup[toupper(alphabet[i])] = i;
			assert(lookup[tolower(alphabet[i])] == -1);
			lookup[tolower(alphabet[i])] = i;
		}
		else
		{
			assert(lookup[alphabet[i]] == -1);
			lookup[alphabet[i]] = i;
		}
	}
}

void Grouper::IsolatedInitialize(const NameValuePairs &parameters)
{
	m_groupSize = parameters.GetIntValueWithDefault("GroupSize", 0);
	ConstByteArrayParameter separator, terminator;
	if (m_groupSize)
		parameters.GetRequiredParameter("Grouper", "Separator", separator);
	parameters.GetValue("Terminator", terminator);

	m_separator.Assign(separator.begin(), separator.size());
	m_terminator.Assign(terminator.begin(), terminator.size());
	m_counter = 0;
}

unsigned int Grouper::Put2(const byte *begin, unsigned int length, int messageEnd, bool blocking)
{
	FILTER_BEGIN;
	if (m_groupSize)
	{
		while (m_inputPosition < length)
		{
			if (m_counter == m_groupSize)
			{
				FILTER_OUTPUT(1, m_separator, m_separator.size(), 0);
				m_counter = 0;
			}

			unsigned int len;
			FILTER_OUTPUT2(2, len = STDMIN(length-m_inputPosition, m_groupSize-m_counter),
				begin+m_inputPosition, len, 0);
			m_inputPosition += len;
			m_counter += len;
		}
	}
	else
		FILTER_OUTPUT(3, begin, length, 0);

	if (messageEnd)
	{
		FILTER_OUTPUT(4, m_terminator, m_terminator.size(), messageEnd);
		m_counter = 0;
	}
	FILTER_END_NO_MESSAGE_END
}

NAMESPACE_END
