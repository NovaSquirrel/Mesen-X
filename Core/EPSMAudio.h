#pragma once
#include "stdafx.h"
#include "Snapshotable.h"
#include "APU.h"
#include "BaseExpansionAudio.h"
#include "SSGAudio.h"
#include "Console.h"
#include "CPU.h"

#include <array>
#include "ym3438.h"

using EPSMSSGAudio = SSGAudio<AudioChannel::EPSM_L, AudioChannel::EPSM_R>;

class EPSMAudio : public EPSMSSGAudio
{
private:
	ym3438_t _chip;

	int16_t _lastOutputs[2];
	int16_t _currentOutputs[2];
	uint8_t writeValue;
	uint16_t writeAddr;
	uint16_t irqATimer;
	uint16_t irqBTimer;
	uint16_t irqACurrentTimer;
	uint16_t irqBCurrentTimer;
	uint8_t irqATimerEnable;
	uint8_t irqBTimerEnable;
	uint8_t irqAHighValue;
	uint8_t irqALowValue;
	uint8_t irqBValue;
	uint16_t currentRegister;


	double _clock;
	double _clockIRQ;
	double _cycleCountIRQ = 0;

	static constexpr uint8_t cycleCount = 24;

	struct InputEntry
	{
		uint8_t addr = 0;
		uint8_t data = 0;
		uint8_t cycle = 0;
		uint8_t wrote = 0;
	};

	static constexpr uint8_t INPUT_BUFFER_SIZE = cycleCount;
	using InputBuffer = std::array<InputEntry, INPUT_BUFFER_SIZE>;
	InputBuffer _inputBuffer;

	void UpdateOutputLevel()
	{
		for (size_t x = 0; x < 2; x++)
		{
			_console->GetApu()->AddExpansionAudioDelta(x == 0 ? AudioChannel::EPSM_L : AudioChannel::EPSM_R, _currentOutputs[x] - _lastOutputs[x]);
			_lastOutputs[x] = _currentOutputs[x];
		}
	}

	uint8_t GetCurrentCycle() const
	{
		return static_cast<uint8_t>(std::floor(_clock)) % cycleCount;
	}

	void WriteToChip(uint8_t a, uint8_t d)
	{
		const auto cycle = GetCurrentCycle();

		if (_inputBuffer[cycle].wrote)
		{
			std::cout << "EPSM CHIP DOUBLE WRITE" << std::endl;
		}

		_inputBuffer[cycle] = {
			a,
			d,
			cycle,
			true
		};
	}


	void WriteToChipIRQ(uint16_t addr, uint8_t value)
	{
		switch (addr) {
		case 0xC000:
		case 0xC002:
			currentRegister = value;
			break;

		case 0xE000:
			if (currentRegister == 0x24) {
				//Timer A High 8 bits
				//std::cout << "Timer A High 8 bits" << std::endl;
				irqAHighValue = value;
			}
			if (currentRegister == 0x25) {
				//Timer A Low 2 bits
				//std::cout << "Timer A Low 2 bits" << std::endl;
				irqALowValue = (value & 0x3);
			}
			if (currentRegister == 0x26) {
				//Timer B 8 bits
				//std::cout << "Timer B 8 bits" << std::endl;
				irqBValue = value;
			}
			if ((currentRegister == 0x27) && ((value & 0x5)|(value & 0xA))) {
				//Load+Enable IRQ (0xA = TimerB, 0x5 = TimerA)
				//std::cout << "Load+Enable IRQ" << std::endl;
				if ((currentRegister == 0x27) && (value & 0x5)) {
					irqATimer = (uint16_t(irqAHighValue) << 2) | irqALowValue;
					irqACurrentTimer = 72 * (1024 - irqATimer) * 2;
					irqATimerEnable = 1;
					//std::cout << "Load+Enable IRQ A" << std::endl;
				}
				if ((currentRegister == 0x27) && (value & 0xA)) {
					irqBTimer = 1152 * (256 - irqBValue) * 2;
					irqBCurrentTimer = irqBTimer;
					irqBTimerEnable = 1;
					//std::cout << "Load+Enable IRQ B " << irqBCurrentTimer << std::endl;
				}
			}
			if ((currentRegister == 0x27) && (value & 0x30)) {
				//Enable/Reset IRQ
				//std::cout << std::hex << uint16_t(value) << "Reset IRQ" << std::endl;
				_console->GetCpu()->ClearIrqSource(IRQSource::EPSM);
				irqATimerEnable = 0;
				irqBTimerEnable = 0;
			}
			if ((currentRegister == 0x29) && (value & 0x3)) {
				//enable IRQ's
				//std::cout << "enable IRQ's" << std::endl;
			}
			break;
		case 0xE002:
			/*if (currentRegister == 0x10) {
				std::cout << "0x10" << std::endl;
			}*/
			break;
		}
	
		//irqBValue = value;
		//std::cout << std::hex << irqBValue << std::endl;

	}

	uint32_t getClockFrequency()
	{
		return _console->GetSettings()->GetEPSMClockFrequency() / 6;
	}

protected:
	void StreamState(bool saving) override
	{
		EPSMSSGAudio::StreamState(saving);

		ArrayInfo<int16_t> lastOutputs{ _lastOutputs, 2 };
		ArrayInfo<int16_t> currentOutputs{ _currentOutputs, 2 };
		ArrayInfo<InputBuffer> inputBuffer{ &_inputBuffer };
		ValueInfo<ym3438_t> chip{ &_chip };
		ValueInfo<double> clock { &_clock };
		Stream(lastOutputs, currentOutputs, inputBuffer, chip, clock);
	}

	void ClockAudio() override
	{
		EPSMSSGAudio::ClockAudio();

		_clock += getClockFrequency() / (double)_console->GetCpu()->GetClockRate(_console->GetModel());
		_clockIRQ += (getClockFrequency()*6) / (double)_console->GetCpu()->GetClockRate(_console->GetModel());
		while (_clockIRQ >= _cycleCountIRQ) {
			_cycleCountIRQ++;
			//std::cout << _cycleCountIRQ << std::endl;
			if (irqATimerEnable) {
				irqACurrentTimer--;
				if (!irqACurrentTimer) {
					//std::cout << "***IRQ***" << std::endl;
					irqATimerEnable = 0;
					_console->GetCpu()->SetIrqSource(IRQSource::EPSM);
				}

			}
			if (irqBTimerEnable) {
				irqBCurrentTimer--;
				if (!irqBCurrentTimer) {
					//std::cout << "***IRQ***" << std::endl;
					irqBTimerEnable = 0;
					_console->GetCpu()->SetIrqSource(IRQSource::EPSM);
				}

			}
		}

		while (_clock >= cycleCount)
		{
			for (uint8_t x = 0; x < 2; x++)
			{
				_currentOutputs[x] = 0;
			}

			for (uint8_t cycle = 0; cycle < cycleCount; cycle++)
			{
				_clock--;

				int16_t samples[2];
				OPN2_Clock(&_chip, samples);

				for (uint8_t x = 0; x < 2; x++)
				{
					_currentOutputs[x] += samples[x];
					_currentOutputs[x] += samples[x+2]/8;
				}

				auto& input = _inputBuffer[cycle];
				if(input.wrote)
				{
					input.wrote = false;
					OPN2_Write(&_chip, input.addr, input.data);
				}
			}

			for (uint8_t x = 0; x < 2; x++)
			{
				_currentOutputs[x] /= 5;
			}

			UpdateOutputLevel();
		}
	}

	virtual uint32_t GetSSGClockFrequency() override
	{
		return EPSMSSGAudio::GetSSGClockFrequency() * (_console->GetSettings()->GetEPSMClockFrequency() / 3579545 );
	}

public:
	EPSMAudio(shared_ptr<Console> console) : EPSMSSGAudio(console)
	{
		memset(_lastOutputs, 0, sizeof(_lastOutputs));
		memset(_currentOutputs, 0, sizeof(_currentOutputs));
		_inputBuffer = {};

		_clock = 0;
		_clockIRQ = 0;

		irqATimerEnable = 0;
		irqBTimerEnable = 0;
		OPN2_Reset(&_chip);
		OPN2_SetChipType(0);
	}

	void WriteRegister(uint16_t addr, uint8_t value)
	{

		if (addr == 0x4016) {
			if ((value & 0x0F) == 0x02) {
				writeValue = value;
				writeAddr = 0xC000;
			}
			if ((value & 0x0F) == 0x0A) {
				writeValue = value;
				writeAddr = 0xE000;
			}
			if ((value & 0x0F) == 0x06) {
				writeValue = value;
				writeAddr = 0xC002;
			}
			if ((value & 0x0F) == 0x0E) {
				writeValue = value;
				writeAddr = 0xE002;
			}
			if ((value & 0x0F) == 0x00) {
				writeValue = (writeValue & 0xF0) | (value >> 4);

				const uint8_t a04016 = (writeAddr & 0xF000) == 0xE000;
				const uint8_t a14016 = !!(writeAddr & 0xF);
				EPSMSSGAudio::WriteRegister(writeAddr, writeValue);
				WriteToChip(a04016 | (a14016 << 1), writeValue);
			}
		}
		if (addr == 0x401c) { addr = 0xC000; }
		if (addr == 0x401d) { addr = 0xE000; }
		if (addr == 0x401e) { addr = 0xC002; }
		if (addr == 0x401f) { addr = 0xE002; }
		switch(addr) {
			case 0xC000:
			case 0xE000:
			case 0xC002:
			case 0xE002:

				WriteToChipIRQ(addr, value);
				const uint8_t a0 = (addr & 0xF000) == 0xE000;
				const uint8_t a1 = !!(addr & 0xF);
				EPSMSSGAudio::WriteRegister(addr, value);
				WriteToChip(a0 | (a1 << 1), value);

				break;
		}
	}
};