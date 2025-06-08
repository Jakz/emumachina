#include "gameboy.h"

#include "platform/mos/mos6502_opcodes.h"

#include "gameboy_ppu.h"
#include "gameboy_apu.h"
#include "cartridge.h"


using namespace gb;

constexpr u32 Gameboy::timerFrequencies[4];

Gameboy::Gameboy() : mem(), cpu(LR35902(*this)), spec(spec), apu(new GBSound()), _cart(new Cartridge())
{
  this->timerCounter = 1024;
  this->cycles = 0;
  this->mode = MODE_GB;
  
  this->display = new GpuGB(&_bus, this, spec);
  
  keysState = 0xFF;
  doubleSpeed = false;
  cyclesLeft = 0;

  mem.setEmulator(this);

  _bus.map(_cart.get(), 0x0000, 0x7FFF); /* rom */
  _bus.map(&mem.memory.vramBank, 0x8000, 0x9FFF); /* vram */
  _bus.map(_cart.get(), 0xA000, 0xBFFF); /* ext ram */
  _bus.map(&mem.memory.wramBank0, 0xC000, 0xCFFF); /* wram */
  _bus.map(&mem.memory.wramBank1, 0xD000, 0xDFFF); /* wram */
  _bus.map(&mem.memory.wramBank0, 0xE000, 0xEFFF); /* echo wram 1 */
  _bus.map(&mem.memory.wramBank1, 0xF000, 0xFDFF); /* echo wram 2 */

  _bus.map(apu, 0xFF10, 0xFF3F); /* apu ports */
}

void Gameboy::loadCartridge(const std::string& fileName)
{
  mem.cart.reset(new Cartridge(fileName));
  mode = mem.cart->isCGB() ? MODE_CGB : MODE_GB;
  init();
}

void Gameboy::setupSound(int sampleRate) { apu->start(sampleRate); }
void Gameboy::mute(bool toggle) { apu->mute(toggle); };


u8 Gameboy::step()
{
  static char buffer[128];
  
  u8 d1 = mem.read(cpu.regs()->PC);
  u8 d2 = mem.read(cpu.regs()->PC+1);
  u8 d3 = mem.read(cpu.regs()->PC+2);
  
  if (d1 != mos6502::OPCODE_BITS)
  {
    mos6502::OpcodeGB params = mos6502::Opcodes::opcodesSpecs[d1];
    int length = params.length;
    
    if (length == 1)
      sprintf(buffer, "%s", params.name);
    else if (length == 2)
      sprintf(buffer, params.name, params.paramsSign ? (s8)d2 : d2);
    else if (length == 3)
      sprintf(buffer, params.name, (d3 << 8) | d2);
  }
  else
  {
    sprintf(buffer, "%s", mos6502::Opcodes::cbMnemonics[d2]);
  }
  
  printf("%04X: %s\n", cpu.regs()->PC, buffer);
  
  u8 cycles;
  
  if (!cpu.halted)
    cycles = cpu.executeSingle();
  else
    cycles = 4;
  
  this->cycles += cycles;
  
  updateTimers(cycles);
  
  display->update(doubleSpeed ? cycles/2 : cycles);
  cpu.manageInterrupts();
  return cycles;
}

/*
 
 ++pc;
 cycles += 4;
 updateTimers(4);
 ++pc;
 cycles += 4;
 updateTimers(2);
 u8 v = mem.read(cpu.regs()->HL.HL);
 cycles += 4;
 updateTimers(4);
 updateTimers(2);
 u8 b = 0;
 cpu.setFlag(FLAG_Z, (v & (1 << b)) == 0);
 cpu.setFlag(FLAG_H, 1);
 cpu.setFlag(FLAG_N, 0);
 */

bool Gameboy::run(u32 maxCycles)
{
  if (doubleSpeed)
    maxCycles *= 2;
  
  cyclesLeft += maxCycles;
  
  lcdChangedState = false;
  
  while (cyclesLeft >= 0 && !lcdChangedState)
  {
    u8 cycles = 0;
    
    u16& pc = cpu.regs()->PC;
    
    u8 opcode = mem.read(pc);

    /*if (opcode == 0xF0)
    {
      //salcazzo
      cycles += 4;
      ++pc;
      
      updateTimers(4);

      u8 value = mem.read(pc);
      ++pc;
      cycles += 4;
      
      updateTimers(2);
      
      cpu.regs()->AF.A = mem.read(0xFF00 | value);

      cycles += 4;
      
      updateTimers(6);
    }
    else*/
    {
    
    
    if (cpu.manageInterrupts())
      cycles = 12;
    else
    {
      if (!cpu.halted)
        cycles += cpu.executeSingle();
      else
        cycles += 4;
    }

    cyclesLeft -= cycles;
    
    updateTimers(cycles);
      
    }
    
    if (doubleSpeed)
    {
      display->update(cycles/2);
    }
    else
    {
      display->update(cycles);
    }

  }
  
  
  cycles += maxCycles + cyclesLeft;
  apu->update();
  
  return true;
}

void Gameboy::toggleDoubleSpeed(bool value)
{
  doubleSpeed = value;
  
  if (value)
  {
    cyclesLeft *= 2;
  }
  else
  {
    cyclesLeft /= 2;
  }
}

void Gameboy::init()
{
  cpu.reset();
  
  Registers *regs = cpu.regs();
  
  regs->PC = 0x0100;
  
  if (mode == MODE_GB)
    regs->AF = 0x01B0;
  else
    regs->AF = 0x11B0;

  regs->BC = 0x0013;
  regs->DE = 0x00D8;
  regs->HL = 0x014D;
  regs->SP = 0xFFFE;

  mem.rawPortWrite(0xFF05, 0x00);
  mem.rawPortWrite(0xFF06, 0x00);
  mem.rawPortWrite(0xFF07, 0x00);
  mem.rawPortWrite(0xFF10, 0x80);
  mem.rawPortWrite(0xFF11, 0xBF);
  mem.rawPortWrite(0xFF12, 0xF3);
  mem.rawPortWrite(0xFF14, 0xBF);
  mem.rawPortWrite(0xFF16, 0x3F);
  mem.rawPortWrite(0xFF17, 0x00);
  mem.rawPortWrite(0xFF19, 0xBF);
  mem.rawPortWrite(0xFF1A, 0x7F);
  mem.rawPortWrite(0xFF1B, 0xFF);
  mem.rawPortWrite(0xFF1C, 0x9F);
  mem.rawPortWrite(0xFF1E, 0xBF);
  mem.rawPortWrite(0xFF20, 0xFF);
  mem.rawPortWrite(0xFF21, 0x00);
  mem.rawPortWrite(0xFF22, 0x00);
  mem.rawPortWrite(0xFF23, 0xBF);
  mem.rawPortWrite(0xFF24, 0x77);
  mem.rawPortWrite(0xFF25, 0xF3);
  mem.rawPortWrite(0xFF26, 0xF1);
  mem.rawPortWrite(0xFF40, 0x91);
  mem.rawPortWrite(0xFF42, 0x00);
  mem.rawPortWrite(0xFF43, 0x00);
  mem.rawPortWrite(0xFF45, 0x00);
  mem.rawPortWrite(0xFF47, 0xFC);
  mem.rawPortWrite(0xFF48, 0xFF);
  mem.rawPortWrite(0xFF49, 0xFF);
  mem.rawPortWrite(0xFF4A, 0x00);
  mem.rawPortWrite(0xFF4B, 0x00);
  mem.rawPortWrite(0xFFFF, 0x00);
  
  dividerCounter = CYCLES_PER_DIVIDER_INCR;
  resetTimerCounter();
  
  doubleSpeed = false;
  
}

void Gameboy::timerTrigger()
{
  /* set TIMA according to TMA */
  mem.rawPortWrite(PORT_TIMA, mem.read(PORT_TMA));
  /* request timer interrupt */
  requestInterrupt(INT_TIMER);
}

void Gameboy::updateTimers(u16 cycles)
{
  // if timer is enabled
  if (isTimerEnabled())
  {
    // update its counter
    //printf("%d - %d = %d\n", timerCounter, cycles, timerCounter-cycles);
    timerCounter -= cycles;
        
    // while the counter is expired
    while (timerCounter <= 0)
    {
      u8 counter = mem.read(PORT_TIMA);
      
      // if we overflowed
      if (counter == 0xFF)
      {
        //mem.rawPortWrite(PORT_TIMA, 0);
        /* TODO: real behavior delays trigger by 4 clock cycles and value of TIMA is 0 meanwhile */
        timerTrigger();
      }
      // just increment it
      else
        mem.rawPortWrite(PORT_TIMA, counter + 1);
      
      timerCounter += timerTicks();
    }
  }

  dividerCounter -= cycles;
  
  // if divider register should be incremented
  if (dividerCounter <= 0)
  {
    // read current value and increment it by one
    u8 t = mem.read(PORT_DIV);
    
    // increase it or make it wrap
    if (t == 255) t = 0;
    else ++t;
    
    // write updated value on address by skipping normal procedure
    mem.rawPortWrite(PORT_DIV, t);
    
    // reset counter for next divider increment
    dividerCounter = CYCLES_PER_DIVIDER_INCR + dividerCounter;
  }
}

void Gameboy::requestInterrupt(Interrupt interrupt)
{
  cpu.enableInterrupt(interrupt);
}

void Gameboy::resetDivCounter()
{
  dividerCounter = CYCLES_PER_DIVIDER_INCR;
}

void Gameboy::resetTimerCounter()
{
  timerCounter = timerTicks();
}

u32 Gameboy::timerTicks()
{
  u32 frequency = timerFrequencies[mem.read(PORT_TAC) & 0x03];
  
  return CYCLES_PER_SECOND / frequency;
}

bool Gameboy::isTimerEnabled()
{
  return bit::bit(mem.read(PORT_TAC), 2);
}



void Gameboy::keyPressed(Key key)
{
  // check if key was unpressed to it's changing
  bool isChanging = bit::bit(keysState, key);
  
  // set button bit to 0 (that is ACTIVE)
  keysState = bit::res(keysState, key);
  
  bool isDirectional = key < 4;
  
  u8 joyp = mem.rawPortRead(PORT_JOYP);
  
  if (isChanging && ((isDirectional && !bit::bit(joyp, 4)) || (!isDirectional && !bit::bit(joyp, 5))))
    requestInterrupt(INT_JOYPAD);
}

void Gameboy::keyReleased(Key key)
{
  keysState = bit::set(keysState, key);
}

u8 Gameboy::keyPadState(u8 joyp) const
{  
  // normal buttons
  if (!bit::bit(joyp, 5))
  {
    joyp |= 0x1F;
    joyp &= ((keysState >> 4) & 0x0F) | 0x10;
  }
  // directional buttons
  else if (!bit::bit(joyp, 4))
  {
    joyp |= 0x2F;
    joyp &= (keysState & 0x0F) | 0x20;
  }
  
  return joyp;
}
