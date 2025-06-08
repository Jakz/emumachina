#include "gameboy_memory.h"

#include "gameboy_spec.h"
#include "cartridge.h"


using namespace gb;

Memory::Memory() : emu(nullptr)
{
  init();
}

/* dealloca tutto e rialloca nuova fiammante memoria */
void Memory::init()
{
  
	memory.oam_table = new u8[160];
  
  memory.ports_table = new u8[256];
  
  memory.color_palette_ram = new u8[128];
  
  memory.cgbPaletteAutoIncr[0] = false;
  memory.cgbPaletteAutoIncr[1] = false;
  
  memset(memory.oam_table, 0, 160);
  memset(memory.ports_table, 0, 256);
  memset(memory.color_palette_ram, 0, 128);

}

Memory::~Memory()
{
  delete [] memory.oam_table;
  delete [] memory.ports_table;
  delete [] memory.color_palette_ram;
}

u8 Memory::paletteRam(u8 index)
{
  return memory.color_palette_ram[index];
}

u8 *Memory::oam()
{
  return memory.oam_table;
}

u8 Memory::readVram0(u16 address)
{
  return memory.vram[address - 0x8000];
}

u8 Memory::readVram1(u16 address)
{
  return memory.vram[address - 0x8000 + 8_kb];
}

u8 Memory::read(u16 address)
{
  // oam table
  if (address >= 0xFE00 && address <= 0xFE9F)
    return memory.oam_table[address - 0xFE00];
  // not usable
  else if (address >= 0xFEA0 && address <= 0xFEFF)
    return -1;
  // ports + HRAM
  else if (address >= 0xFF00)
    return trapPortRead(address);
	
	return 0;
		
}

void Memory::write(u16 address, u8 value)
{
  // oam table
  if (address >= 0xFE00 && address <= 0xFE9F)
    memory.oam_table[address - 0xFE00] = value;
  // ports + HRAM
  else if (address >= 0xFF00)
    trapPortWrite(address, value);
  
  // not usable
  //else if (address >= 0xFEA0 && address <= 0xFEFF)
}

u8 Memory::trapPortRead(u16 address)
{
  switch (address)
  {
    case PORT_JOYP:
    {
      u8 oldJoyp = rawPortRead(address);
      u8 joyp = emu->keyPadState(oldJoyp);
      rawPortWrite(address, joyp);
      return joyp;
      break;
    }
  }
  
  return rawPortRead(address);
}


void Memory::trapPortWrite(u16 address, u8 value)
{
  if (address >= PORT_NR10 && address <= 0xFF3F)
  {
    emu->sound.write(address, value);
  }
  
  switch (address)
  {
    // writing on the divider register will reset it
    //case PORT_LY:
    case PORT_DIV: { 
      value = 0;
      emu->resetDivCounter();
      /* writing on DIV also resets programmable TIMER */
      emu->resetTimerCounter();
      break;
    }
    // writing on the TAC register will start/stop the timer or change its frequency
    case PORT_TAC:
    {
      //u8 oldValue = read(PORT_TAC);
      rawPortWrite(address, value);
      
      // if frequency of the timer has just to change
      emu->resetTimerCounter();
      
      return;
    }
    case PORT_JOYP:
    {
      /* writes to lower 4 bits JOYP are inhibited */
      value = (value & 0xF0) | 0x0F;
      break;
    }
    // switch vram bank in CGB
    case PORT_VBK:
    {      
      if (bit::bit(value, 0))
        memory.vramBank.setBank(1);
      else if (!bit::bit(value, 0))
        memory.vramBank.setBank(0);
      
      break;
    }
    // switch wram slot 1 in CGB  
    case PORT_SVBK:
    {
      u8 bank = value & 0x07;
      if (bank == 0) bank = 1;

      memory.wramBank1.setBank(bank);

      break;
    }
    case PORT_KEY1:
    {
      // if we're asking for a speed switch
      if (bit::bit(value,0))
      {
        // set bit in the speed switch port to be managed by STOP instruction
        rawPortWrite(PORT_KEY1, rawPortRead(PORT_KEY1) | 0x01);
      }
      break;
    }
    // DMA transfer of 160 bytes 
    case PORT_DMA:
    {
      // multiply value by 0x100 to obtain high address
      u16 address = value << 8;
      
      for (int i = 0; i < 160; ++i)
        write(0xFE00+i, read(address+i));
      
      return;
    }
    case PORT_SB:
    {
      //printf("%c", value);
      return;
    }
    case PORT_BGPI:
    {
      // if bit 7 of background palette index is 1 then
      // it should autoincrement its value after a write
      memory.cgbPaletteAutoIncr[0] = (value & 0x80) != 0;
      
      //printf("Selecting palette %02x, autoincrement: %d\n", value & 0x3f, memory.cgbPaletteAutoIncr[0]);
      
      break;
    }
    case PORT_BGPD:
    {
      u8 paletteByte = rawPortRead(PORT_BGPI) & 0x3F;
      
      memory.color_palette_ram[paletteByte] = value;
      
      printf("Writing %02x at palette %02x\n", value, paletteByte);
      
      // TODO maybe index should wrap?
      if (memory.cgbPaletteAutoIncr[0] /*&& paletteByte < 0x40*/)
      {
        rawPortWrite(PORT_BGPI, (paletteByte+1)%0x40);
      }
      
      break;
    }
    case PORT_OBPI:
    {
      // if bit 7 of object palette index is 1 then
      // it should autoincrement its value after a write
      memory.cgbPaletteAutoIncr[1] = (value & 0x80) != 0;
      break;
    }
    case PORT_OBPD:
    {
      u8 paletteByte = 64 + (rawPortRead(PORT_OBPI) & 0x3F);
      
      memory.color_palette_ram[paletteByte] = value;
      
      // TODO maybe index should wrap?
      if (memory.cgbPaletteAutoIncr[1] /*&& paletteByte < 0x80*/)
      {
        rawPortWrite(PORT_OBPI, (paletteByte+1)%0x40);
      }
      
      break;
    }
    /*case PORT_HDMA1:
    case PORT_HDMA2:
    case PORT_HDMA3:
    case PORT_HDMA4:*/
    case PORT_HDMA5:
    {    
      // compose source address from the two source hdma ports
      u16 source = (rawPortRead(PORT_HDMA1)<<8) | rawPortRead(PORT_HDMA2);
      // clamp address, lower 4 bits are ignored (0000-7FF0 or A000-DFF0)
      source &= 0xFFF0;
      
      // compose destination address from the two dest hdma ports
      u16 dest = (rawPortRead(PORT_HDMA3)<<8) | rawPortRead(PORT_HDMA4);
      // clamp address, lower 4 bits are ignored, 3 higher bits are ignored since destination is always VRAM (8000-9FF0)
      dest = (dest & 0x7FF0) | 0x8000;
      
      // length is lower 7 bit of this register + 1 multiplied by 16
      u16 length = ((value & 0x7F) + 1);
      
      // hblank DMA (16 bytes for every HBLANK)
      if (bit::bit(value,7))
      {
        hdma.active = true;
        hdma.src = source;
        hdma.dest = dest;
        hdma.length = length;
        
        // we need to set bit 7 to 0 to warn that HDMA transfer is active
        value &= 0x7F;
      }
      // general purpose HDMA, everything is copied at once
      else
      {
        // if bit 7 is not set but HDMA transfer was active
        // force disable it and set bit 7 to 1 to warn that it is finished
        if (hdma.active)
        {
          hdma.active = false;
          value |= 0x80;
        }
        else
        {
        
          for (int i = 0; i < length * 0x10; ++i)
            write(dest+i, read(source+i));
        
          value = 0xFF;
        }
      }
      break;
    }
    case PORT_LCDC:
    {
      u8 lcdc = rawPortRead(PORT_LCDC);
      
      //value |= 0x80;
      
      if (bit::bit(lcdc, 7) ^ bit::bit(value, 7))
        emu->toggleLcdState();
      
      //printf("LCDC %.2x\n", value); break;

      break;
    }
      
    case PORT_STAT:
    {
      //printf("STAT %.2x\n", value); break;
      break;
    }
  }

  rawPortWrite(address, value);
}


void Memory::rawPortWrite(u16 address, u8 value)
{
  memory.ports_table[address - 0xFF00] = value;
}

u8 Memory::rawPortRead(u16 address) const
{
  if (address >= 0xFF10 && address <= 0xFF3F)
  {
    return emu->sound.read(address);
  }

  return memory.ports_table[address - 0xFF00];
}

