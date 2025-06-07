#include "lr35902.h"

#include "devices/component.h"
#include "platform/mos/mos6502_opcodes.h"
#include "gameboy_spec.h"
#include "gameboy.h"

using namespace mos6502;
using namespace gb;

LR35902::LR35902(Gameboy* system, devices::Bus* bus) : Mos6502(bus), system(system)
{
  opcodes[OPCODE_DJNZ_N] = static_cast<void (Mos6502::*)()>(&LR35902::djnzn);
}

void LR35902::djnzn()
{
  // if a speed switch was requested
  u8 speed = bus->peek(PORT_KEY1);

  if (bit::bit(speed, 0))
  {
    // if CPU was in double mode
    if (bit::bit(speed, 7))
    {
      system->toggleDoubleSpeed(false);
      bus->poke(PORT_KEY1, speed & 0x7E);
    }
    else
    {
      system->toggleDoubleSpeed(true);
      bus->poke(PORT_KEY1, (speed | 0x80) & 0xFE);
    }

    //s.interruptsEnabled = true;
    //mem->write(PORT_IF, 0x1F);
  }
  else
    Mos6502::djnzn();
}