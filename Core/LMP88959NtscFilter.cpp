// NTSC filter based on EMMIR's composite modem
// https://github.com/LMP88959/NTSC-CRT
#include "stdafx.h"
#include "LMP88959NtscFilter.h"
#include "PPU.h"
#include "EmulationSettings.h"
#include "Console.h"

LMP88959NtscFilter::LMP88959NtscFilter(shared_ptr<Console> console) : BaseVideoFilter(console)
{
	memset(&_crt, 0, sizeof(CRT));
	memset(&_nesNTSC, 0, sizeof(NTSC_SETTINGS));
	_frameBuffer = new uint32_t[(PPUpx2pos(256)) * 240]();

	crt_init(&_crt, (PPUpx2pos(256)), 240, 5, reinterpret_cast<unsigned char*>(_frameBuffer));

	_nesNTSC.w = PPU::ScreenWidth;
	_nesNTSC.h = PPU::ScreenHeight;
}

FrameInfo LMP88959NtscFilter::GetFrameInfo()
{
	OverscanDimensions overscan = GetOverscan();
	uint32_t overscanLeft = overscan.Left > 0 ? PPUpx2pos(overscan.Left) : 0;
	uint32_t overscanRight = overscan.Right > 0 ? PPUpx2pos(overscan.Right) : 0;

	if (_keepVerticalRes) {
		return {
			(PPUpx2pos(PPU::ScreenWidth) - overscanLeft - overscanRight),
			(PPU::ScreenHeight - overscan.Top - overscan.Bottom),
			PPUpx2pos(PPU::ScreenWidth),
			PPU::ScreenHeight,
			4
		};
	}
	else {
		return {
			PPUpx2pos(PPU::ScreenWidth) - overscanLeft - overscanRight,
			(PPU::ScreenHeight - overscan.Top - overscan.Bottom) * 2,
			PPUpx2pos(PPU::ScreenWidth),
			PPU::ScreenHeight * 2,
			4
		};
	}
}

void LMP88959NtscFilter::OnBeforeApplyFilter()
{
	_ntscBorder = (_console->GetModel() == NesModel::NTSC);

	PictureSettings pictureSettings = _console->GetSettings()->GetPictureSettings();
	NtscFilterSettings ntscSettings = _console->GetSettings()->GetNtscFilterSettings();

	_keepVerticalRes = ntscSettings.KeepVerticalResolution;

	_crt.hue = static_cast<int>(pictureSettings.Hue * 180.0);
	_crt.saturation = static_cast<int>(((pictureSettings.Saturation + 1.0) / 2.0) * 25.0);
	_crt.brightness = static_cast<int>(pictureSettings.Brightness * 100.0);
	_crt.contrast = static_cast<int>(((pictureSettings.Contrast + 1.0) / 2.0) * 301.0);
	_noise = static_cast<int>(ntscSettings.Noise * 500.0);
	_crt.blend = static_cast<int>(ntscSettings.FrameBlend);

	_nesNTSC.dot_crawl_offset = GetVideoPhase();
	_nesNTSC.border_color = _ntscBorder ? _console->GetPpu()->GetCurrentBgColor() : 0x0F;
}

void LMP88959NtscFilter::ApplyFilter(uint16_t *ppuOutputBuffer)
{
	_ppuOutputBuffer = ppuOutputBuffer;

	_nesNTSC.data = reinterpret_cast<unsigned short*>(_ppuOutputBuffer);

	crt_modulate(&_crt, &_nesNTSC);

	crt_demodulate(&_crt, _noise);
	GenerateArgbFrame(_frameBuffer);

}

void LMP88959NtscFilter::GenerateArgbFrame(uint32_t* frameBuffer)
{
	uint32_t* outputBuffer = GetOutputBuffer();
	OverscanDimensions overscan = GetOverscan();
	int overscanLeft = overscan.Left > 0 ? PPUpx2pos(overscan.Left) : 0;
	int overscanRight = overscan.Right > 0 ? PPUpx2pos(overscan.Right) : 0;
	int rowWidth = PPUpx2pos(PPU::ScreenWidth);
	int rowWidthOverscan = rowWidth - overscanLeft - overscanRight;

	if (_keepVerticalRes) {
		frameBuffer += rowWidth * overscan.Top + overscanLeft;
		for (uint32_t i = 0, len = overscan.GetScreenHeight(); i < len; i++) {
			memcpy(outputBuffer, frameBuffer, rowWidthOverscan * sizeof(uint32_t));
			outputBuffer += rowWidthOverscan;
			frameBuffer += rowWidth;
		}
	}
	else {
		double scanlineIntensity = 1.0 - _console->GetSettings()->GetPictureSettings().ScanlineIntensity;
		bool verticalBlend = _console->GetSettings()->GetNtscFilterSettings().VerticalBlend;

		for (int y = PPU::ScreenHeight - 1 - overscan.Bottom; y >= (int)overscan.Top; y--) {
			uint32_t const* in = frameBuffer + y * rowWidth;
			uint32_t* out = outputBuffer + (y - overscan.Top) * 2 * rowWidthOverscan;

			if (verticalBlend || scanlineIntensity < 1.0) {
				for (int x = 0; x < rowWidthOverscan; x++) {
					uint32_t prev = in[overscanLeft];
					uint32_t next = y < 239 ? in[overscanLeft + rowWidth] : 0;

					*out = 0xFF000000 | prev;

					/* mix 24-bit rgb without losing low bits */
					uint32_t mixed;
					if (verticalBlend) {
						mixed = (prev + next + ((prev ^ next) & 0x030303)) >> 1;
					}
					else {
						mixed = prev;
					}

					if (scanlineIntensity < 1.0) {
						uint8_t r = (mixed >> 16) & 0xFF, g = (mixed >> 8) & 0xFF, b = mixed & 0xFF;
						r = (uint8_t)(r * scanlineIntensity);
						g = (uint8_t)(g * scanlineIntensity);
						b = (uint8_t)(b * scanlineIntensity);
						*(out + rowWidthOverscan) = 0xFF000000 | (r << 16) | (g << 8) | b;
					}
					else {
						*(out + rowWidthOverscan) = 0xFF000000 | mixed;
					}
					in++;
					out++;
				}
			}
			else {
				for (int i = 0; i < rowWidthOverscan; i++) {
					out[i] = 0xFF000000 | in[overscanLeft + i];
				}
				memcpy(out + rowWidthOverscan, out, rowWidthOverscan * sizeof(uint32_t));
			}
		}
	}
}

LMP88959NtscFilter::~LMP88959NtscFilter()
{
	memset(&_crt, 0, sizeof(CRT));
	memset(&_nesNTSC, 0, sizeof(NTSC_SETTINGS));
	delete[] _frameBuffer;
}