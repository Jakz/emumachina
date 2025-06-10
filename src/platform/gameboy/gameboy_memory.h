#pragma once 

#include <cstring>
#include <cstdlib>
#include <memory>

#include "devices/component.h"
#include "common.h"

#define HEADER_CGB_FLAG (0x143)

#define HEADER_CGB_FLAG_CGB_GB (0x80)
#define HEADER_CGB_FLAG_CGB_ONLY (0xC0)

namespace gb
{
  class Emulator;
  class Cartridge;
  
  /*
	  COME INDIRIZZA LO Z80 SUL GAMEBOY
	
	  0000-3FFF*   16KB ROM Bank 00     (in cartridge, fixed at bank 00) (cartridge.h)
	  4000-7FFF*   16KB ROM Bank 01..NN (in cartridge, switchable bank number) (cartridge.h)
	  8000-9FFF   8KB Video RAM (VRAM) (switchable bank 0-1 in CGB Mode)
	  A000-BFFF*   8KB External RAM     (in cartridge, switchable bank, if any) (cartridge.h)
	  C000-CFFF*   4KB Work RAM Bank 0 (WRAM) (memory.h inevitabilmente, tanto è solo zona di lavoro, non si interfaccia a niente)
	  D000-DFFF*   4KB Work RAM Bank 1 (WRAM)  (switchable bank 1-7 in CGB Mode) (memory.h)
	  E000-FDFF*   Same as C000-DDFF (ECHO)    (typically not used) (memory.h)
	  FE00-FE9F   Sprite Attribute Table (OAM)
	  FEA0-FEFF   Not Usable
	  FF00-FF7F   I/O Ports
	  FF80-FFFE   High RAM (HRAM)
	  FFFF        Interrupt Enable Register
	
	  in effetti non so se conviene suddividere in più file, per ora c'è il controllore della cart, magari serve controllore per LCD e per l'I/O generico

  */




  struct MemoryMap
  {
    u8 *ports_table;
  
    devices::Ram vram; // 16kb vram
    devices::Ram wram; // 32kb wram
    devices::Ram oamRam; // 160 bytes OAM (sprite attribute table)
    devices::Ram _paletteRam; // 128 bytes color palette RAM (CGB only, 8 palettes of 16 colors each)
    devices::Ram _ports; // 256 bytes I/O ports (0xFF00-0xFF7F)

    devices::AddressableBank vramBank; // 0x8000-0x9FFF (8kb bank switchable in CGB 0-1)
    devices::AddressableBank wramBank0; // 0xC000 - 0xCFFF - 4kb working RAM
    devices::AddressableBank wramBank1; // 0xD000-0xDFFF - 4kb working RAM (switchable 1-7 in CGB)

    MemoryMap() : vram(16_kb), wram(32_kb), oamRam(160), _paletteRam(128), _ports(256), vramBank(&vram, 8_kb), wramBank0(&wram, 4_kb), wramBank1(&wram, 4_kb)
    {
      /* default bank for wram1 is the second */
      wramBank1.setBank(1);
    }
  
    bool cgbPaletteAutoIncr[2];
  };

  struct HDMA
  {
    u16 src, dest;
    u16 length;
    bool active;
  
    HDMA()
    {
      active = false;
    }
  };

  class Memory : devices::Addressable
  {
    private:
    
      Emulator* emu;
  
      HDMA hdma;
  
      // this function is used to intercept writes to the port addresses that may need custom behavior
      // this is the function as seen by the GB
      inline void trapPortWrite(u16 address, u8 value);
      inline u8 trapPortRead(u16 address);
  
    
      devices::Bus* _bus;

    public:
      MemoryMap memory;


      Memory(devices::Bus* bus);
      ~Memory();
  
      void setEmulator(Emulator* emu) { this->emu = emu; }

      u8 read(u16 address) override;
      void write(u16 address, u8 value) override;
    
      void init();
      
      std::unique_ptr<Cartridge> cart;
  
      u8 readVram0(u16 address);
      u8 readVram1(u16 address);
      devices::Ram& paletteRam();
      devices::Ram& oam();
  
      MemoryMap *memoryMap() { return &memory; }
      HDMA *hdmaInfo() { return &hdma; }
  };

}
