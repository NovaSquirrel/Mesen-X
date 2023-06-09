#pragma once
#include "stdafx.h"
#include "../Utilities/SimpleLock.h"
#include "EmulationSettings.h"
#include "FrameInfo.h"
#include "VideoHud.h"

class Console;

class BaseVideoFilter
{
private:
	uint32_t* _outputBuffer = nullptr;
	uint32_t _bufferSize = 0;
	SimpleLock _frameLock;
	OverscanDimensions _overscan;
	bool _isOddFrame;
	uint32_t _videoPhase = 0;

	void UpdateBufferSize();

protected:
	shared_ptr<Console> _console;

	virtual void ApplyFilter(uint16_t *ppuOutputBuffer) = 0;
	virtual void OnBeforeApplyFilter();

public:
	BaseVideoFilter(shared_ptr<Console> console);
	virtual ~BaseVideoFilter();

	uint32_t* GetOutputBuffer();
	bool IsOddFrame();
	uint32_t GetVideoPhase();
	void SendFrame(uint16_t *ppuOutputBuffer, uint32_t frameNumber, uint32_t videoPhase);
	void TakeScreenshot(string romName, VideoFilterType filterType);
	void TakeScreenshot(VideoFilterType filterType, string filename, std::stringstream *stream = nullptr, bool rawScreenshot = false);

	virtual OverscanDimensions GetOverscan();
	virtual FrameInfo GetFrameInfo() = 0;
};