#pragma once

#include "platform/mos/mos6502.h"

namespace gb
{
  class Gameboy;

  enum Interrupt
  {
    INT_VBLANK = 0,
    INT_STAT = 1,
    INT_TIMER = 2,
    INT_SERIAL = 3,
    INT_JOYPAD = 4
  };

  class LR35902 : public mos6502::Mos6502
  {
  protected:
    Gameboy* system;

  public:
    LR35902(Gameboy* system, devices::Bus* bus);

  public:
    void djnzn();
  };
}
