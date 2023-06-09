#pragma once
#include "stdafx.h"
#include "BaseVideoFilter.h"
#include "../Utilities/nes_ntsc.h"
#include "../Utilities/AutoResetEvent.h"

class Console;

class NtscFilter : public BaseVideoFilter
{
private:
	nes_ntsc_setup_t _ntscSetup;
	nes_ntsc_t _ntscData;
	bool _keepVerticalRes = false;
	bool _useExternalPalette = true;
	uint8_t _palette[512 * 3];
	uint16_t* _ppuOutputBuffer = nullptr;
	uint32_t* _ntscBuffer;
	bool _ntscBorder = true;

	std::thread _extraThread;
	AutoResetEvent _waitWork;
	atomic<bool> _stopThread;
	atomic<bool> _workDone;

	void GenerateArgbFrame(uint32_t *outputBuffer);

protected:
	void OnBeforeApplyFilter();

public:
	NtscFilter(shared_ptr<Console> console);
	virtual ~NtscFilter();

	virtual void ApplyFilter(uint16_t *ppuOutputBuffer);
	virtual FrameInfo GetFrameInfo();
};