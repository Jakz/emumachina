#pragma once

#include "platform/mos/mos6502.h"

namespace gb
{
  class Gameboy;

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
