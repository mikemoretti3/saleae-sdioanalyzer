// The MIT License (MIT)
//
// Copyright (c) 2013 Erick Fuentes http://erickfuent.es
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "SDIOAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "SDIOAnalyzer.h"
#include "SDIOAnalyzerSettings.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdarg.h>

SDIOAnalyzerResults::SDIOAnalyzerResults( SDIOAnalyzer* analyzer, SDIOAnalyzerSettings* settings )
:	AnalyzerResults(),
	mSettings( settings ),
	mAnalyzer( analyzer )
{
}

SDIOAnalyzerResults::~SDIOAnalyzerResults()
{
}

static void logDbg(char* fmt, ...)
{
	va_list args;
	FILE* lf = fopen("logic.log", "a");
	if (!lf) {
		return;
	}
	va_start(args, fmt);
	vfprintf(lf, fmt, args);
	va_end(args);
	fclose(lf);
}

#define CMD52_WR_DATA 0
#define CMD52_WR_DATA_MASK 0xFF
#define CMD52_REG_ADDR (8+1)
#define CMD52_REG_ADDR_MASK 0x1FFFF
#define CMD52_RAW (CMD52_REG_ADDR+17+1)
#define CMD52_RAW_MASK 1
#define CMD52_FUNC (CMD52_RAW+1)
#define CMD52_FUNC_MASK 0x7
#define CMD52_RW_FLAG (CMD52_FUNC+3)
#define CMD52_RW_FLAG_MASK 1

static std::stringstream ParseCmd52(Frame& frame, DisplayBase display_base)
{
	std::stringstream result;
	char number_str1[128];
	uint8_t rw = (frame.mData1 >> CMD52_RW_FLAG) & CMD52_RW_FLAG_MASK;
	uint8_t raw = (frame.mData1 >> CMD52_RAW) & CMD52_RAW_MASK;
	if (!rw) {
		result << "R ";
	}
	else {
		result << "W";
		if (raw) {
			result << "+R";
		}
		result << " ";
	}
	uint32_t func = (frame.mData1 >> CMD52_FUNC) & CMD52_FUNC_MASK;
	AnalyzerHelpers::GetNumberString(func, Decimal, 0, number_str1, 128);
	result << "F:" << number_str1 << " ";
	uint32_t reg = (frame.mData1 >> CMD52_REG_ADDR) & CMD52_REG_ADDR_MASK;
	AnalyzerHelpers::GetNumberString(reg, display_base, 0, number_str1, 128);
	result << "REG:" << number_str1 << " ";
	if (rw) {
		uint8_t wrdata = (frame.mData1 >> CMD52_WR_DATA) & CMD52_WR_DATA_MASK;
		AnalyzerHelpers::GetNumberString(wrdata, display_base, 0, number_str1, 128);
		result << "DATA:" << number_str1 << " ";
	}
//	logDbg("Result: %s\n", result.str().c_str());
	return result;
}

#define CMD52R5_DATA (0)
#define CMD52R5_DATA_MASK 0xFF
#define CMD52R5_RESP_FLAGS (8)
#define CMD52R5_RESP_FLAGS_MASK 0xFF

static std::stringstream ParseCmd52Resp(Frame& frame, DisplayBase display_base)
{
	std::stringstream result;
	char number_str1[128];

	uint8_t flags = (frame.mData1 >> CMD52R5_RESP_FLAGS) & CMD52R5_RESP_FLAGS_MASK;
	result << "F:";
	if (flags & (1<<7)) {
		result << "CRC ";
	}
	if (flags & (1<<6)) {
		result << "ILL ";
	}
	uint8_t state = (flags & (3<<4)) >> 4;
	switch (state) {
	case 0b00:
		result << "DIS ";
		break;
	case 0b01:
		result << "CMD ";
		break;
	case 0b10:
		result << "TRN ";
		break;
	}
	if (flags & (1<<3)) {
		result << "ERR ";
	}
	if (flags & (1<<1)) {
		result << "FUN ";
	}
	if (flags & 1) {
		result << "OOR ";
	}
	uint8_t data = (frame.mData1 >> CMD52R5_DATA) & CMD52R5_DATA_MASK;
	AnalyzerHelpers::GetNumberString(data, display_base, 0, number_str1, 128);
	result << "DATA: " << number_str1 << " ";

	return result;
}

void SDIOAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base )
{
	ClearResultStrings();
	Frame frame = GetFrame( frame_index );
	static uint8_t lastCmd = 0;
	static uint8_t lastDir = 0;

	char number_str1[128];
	char number_str2[128];
	if (frame.mType == SDIOAnalyzer::FRAME_DIR){
		if (frame.mData1){
			AddResultString("H");
			AddResultString("Host");
			AddResultString("DIR: Host");
			lastDir = 0;
		}else{
			AddResultString("S");
			AddResultString("Slave");
			AddResultString("DIR: Slave");
			lastDir = 1;
		}
	}else if (frame.mType == SDIOAnalyzer::FRAME_CMD){
		AnalyzerHelpers::GetNumberString( frame.mData1, Decimal, 6, number_str1, 128 );
		AddResultString("CMD ", number_str1);
		lastCmd = frame.mData1;
	}
	else if (frame.mType == SDIOAnalyzer::FRAME_ARG) {
		std::stringstream result;
		logDbg("LastCmd=%d\n", lastCmd);
		if (lastCmd == 52) {
//			logDbg("Cmd 52\n");
			// host
			if (!lastDir) {
//				logDbg("Host\n");
				result = ParseCmd52(frame, display_base);
			}
			else {
				result = ParseCmd52Resp(frame, display_base);
			}
		}
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 32, number_str1, 128 );
		result << "ARG:" << number_str1;
		AddResultString(result.str().c_str());
	}else if (frame.mType == SDIOAnalyzer::FRAME_LONG_ARG){
		AnalyzerHelpers::GetNumberString (frame.mData1, display_base, 64, number_str1, 128);
		AnalyzerHelpers::GetNumberString (frame.mData2, display_base, 64, number_str2, 128);
		AddResultString("LONG: ", number_str1, number_str2);
	}else if (frame.mType == SDIOAnalyzer::FRAME_CRC){
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 7, number_str1, 128 );
		AddResultString("CRC ", number_str1);
	}
}

void SDIOAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
	std::ofstream file_stream( file, std::ios::out );

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	file_stream << "Time [s],Value" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 i=0; i < num_frames; i++ )
	{
		Frame frame = GetFrame( i );
		
		char time_str[128];
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

		char number_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

		file_stream << time_str << ",";
		
		if (frame.mType == SDIOAnalyzer::FRAME_DIR){
			file_stream << "DIR:";
			if (frame.mData1){
				file_stream << "from Host";
			}else{
				file_stream << "from Slave";
			}
		}else if (frame.mType == SDIOAnalyzer::FRAME_CMD){
			file_stream << "CMD:" << number_str;
		}else if (frame.mType == SDIOAnalyzer::FRAME_ARG){
			file_stream << "ARG:" << number_str;
		}else if (frame.mType == SDIOAnalyzer::FRAME_LONG_ARG){
			file_stream << "LONG_ARG:" << number_str;
		}else if (frame.mType == SDIOAnalyzer::FRAME_CRC){
			file_stream << "CRC:" << number_str;
		}
		
		file_stream << std::endl;

		if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
		{
			file_stream.close();
			return;
		}
	}

	file_stream.close();
}

void SDIOAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	Frame frame = GetFrame(frame_index);
	ClearTabularText();

	char number_str[128];
	AnalyzerHelpers::GetNumberString(frame.mData1, display_base, 8, number_str, 128);

	std::stringstream result;

	if (frame.mType == SDIOAnalyzer::FRAME_DIR) {
		if (frame.mData1) {
			result << "Host";
		}
		else {
			result << "Slave";
		}
	}
	else if (frame.mType == SDIOAnalyzer::FRAME_CMD) {
		result << "CMD:" << number_str;
	}
	else if (frame.mType == SDIOAnalyzer::FRAME_ARG) {
		result << "ARG:" << number_str;
	}
	else if (frame.mType == SDIOAnalyzer::FRAME_LONG_ARG) {
		result << "L_ARG:" << number_str;
	}
	else if (frame.mType == SDIOAnalyzer::FRAME_CRC) {
		result << "CRC:" << number_str;
	}

	AddTabularText(result.str().c_str());
}

void SDIOAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}

void SDIOAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}
