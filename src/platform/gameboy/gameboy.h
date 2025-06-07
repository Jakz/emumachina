#ifndef _GB_EMULATOR_H_
#define _GB_EMULATOR_H_

#include "utils.h"

#include "systems/gb/gpu.h"
#ifndef DEBUGGER
#include "sound.h"
#endif

#include "lr35902.h"

namespace gb {

class GpuGB;

class GBSound;
class Memory;
class Mos6502;

enum Mode
{
  MODE_CGB = 0,
  MODE_GB = 1,
  MODE_CGB_IN_GB = 2
};

class Gameboy
{
public:

private:
  s32 dividerCounter;
  s32 timerCounter;

  s32 cyclesLeft;

  u8 keysState;

  bool isTimerEnabled();
  void updateTimers(u16 cycles);
  void timerTrigger();

  bool doubleSpeed;
  bool lcdChangedState;

public:
  Gameboy(const EmuSpec& spec);

  void init();

  void loadCartridge(const std::string& fileName);

  void setupSound(int sampleRate);


  void resetTimerCounter();
  void resetDivCounter();
  u32 timerTicks();


  u8 step();
  bool run(u32 cycles);

  void requestInterrupt(Interrupt interrupt);

  u64 cycles;
  Mode mode;

  static constexpr u32 timerFrequencies[4] = { 4_kb, 256_kb, 64_kb, 16_kb };

  void keyPressed(Key key);
  void keyReleased(Key key);
  u8 keyPadState(u8 writeValue) const;


  gb::LR35902 cpu;
  Memory mem;
  GpuGB* display;
#ifndef DEBUGGER
  GBSound sound;
#endif

  void toggleLcdState() { lcdChangedState = true; }
  void toggleDoubleSpeed(bool value);

  void mute(bool toggle) { sound.mute(toggle); };

  EmuSpec spec;
};
  
}

#endif