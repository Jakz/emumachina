#pragma once

#include "common.h"
#include "mos6502_opcodes.h"

#include <array>

namespace mos6502
{

  typedef unsigned char u8;
  typedef unsigned short u16;

  typedef signed char s8;

  const u8 FLAG_Z = 0x80;
  const u8 FLAG_N = 0x40;
  const u8 FLAG_H = 0x20;
  const u8 FLAG_C = 0x10;
  const u8 FLAG_PV = 0x08;
  const u8 FLAG_S = 0x04;
  
  const u8 REGS_HL = 0x06;
  const u8 REG_AF = 0x03;

  class Memory;
  class Emulator;

  enum Interrupt
  {
    INT_VBLANK = 0,
    INT_STAT = 1,
    INT_TIMER = 2,
    INT_SERIAL = 3,
    INT_JOYPAD = 4
  };

  /* registers */
  struct Registers
  {
	  std::array<u8*, 8> rr;
	  std::array<u16*, 4> rrrsp;
    std::array<u16*, 4> rrraf;

    union
    {
      struct { u8 F; u8 A; };
      u16 AF;
    };

    union
    {
      struct { u8 C; u8 B; };
      u16 BC;
    };

    union
    {
      struct { u8 E; u8 D; };
      u16 DE;
    };

    union
    {
      struct { u8 L; u8 H; };
      u16 HL;
    };

	  u16 SP;
	  u16 PC;
  };

  struct Status
  {
    bool interruptsEnabled;
    bool running;
  };

  class Mos6502
  {
  protected:
    using op_func = void (Mos6502::*)();
    std::array<op_func, 256> opcodes;

    devices::Bus* bus;
    
    Registers r;
    Status s;
  
    u8 executeInstruction(u8 opcode);
  
    void storeSingle(u8 reg, u8 value);
    u8 loadSingle(u8 reg);
  
    void storeDoubleSP(u8 reg, u16 value);
    u16 loadDoubleSP(u8 reg);
  
    void storeDoubleAF(u8 reg, u16 value);
    u16 loadDoubleAF(u8 reg);
  
    u16 popDoubleSP();
    void pushDoubleSP(u16 value);
  
    u16 loadDoublePC();
    
    bool isConditionTrue(u8 cond);
    void halt();

    void djnzn() { halted = true; }
  
  public:
    Mos6502(devices::Bus* bus);

  protected:
    void resetFlag(u8 flag);
    void setFlag(u8 flag, u8 value);
    bool isFlagSet(u8 flag);
    
    void add(u8 value);
    void sub(u8 value);
    void adc(u8 value);
    void sbc(u8 value);
    void daa();
  
    bool parity(u8 value);
  
  public:
    void reset();
    u8 executeSingle();
  
    void enableInterrupt(u8 interrupt);
    bool manageInterrupts();
  
    Registers *regs();
    Status *status();
  
    bool halted;
  };
}
