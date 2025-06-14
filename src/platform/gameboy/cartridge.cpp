#include "cartridge.h"

using namespace gb;

Cartridge::Cartridge()
{
  status.rom = nullptr;
  status.rom_bank_0 = nullptr;
  status.rom_bank_1 = nullptr;
  status.ram = nullptr;
  status.ram_bank = nullptr;
  status.rtc = nullptr;
  
  status.rtc_override = false;
  status.ram_enabled = false;
  status.flags = 0;
  
  status.current_rom_bank = 1;
  status.current_ram_bank = 0;
}

Cartridge::Cartridge(const path& fileName) : Cartridge()
{
  load(fileName);
}

Cartridge::~Cartridge()
{
  delete [] status.ram;
  delete [] status.rom;
  delete [] status.rtc;
}

/* scrive un byte nella ROM */
void Cartridge::write(u16 address, u8 value)
{
#ifdef DEBUGGER
	if ((status.flags & MBC_SIMPLE) == MBC_SIMPLE)
    status.rom[address] = value;
#endif
  
  if ((status.flags & MBC_MBC1) == MBC_MBC1)
	{
		/* this enables or less the RAM */
		if (address <= 0x1FFF)
		{	
			if ((value & 0x0A) == 0x0A)
				status.ram_enabled = true;
			else
				status.ram_enabled = false;
		}
    /* this switches ROM bank using 5 lower bits of data */
		else if (address <= 0x3FFF)
		{
      // we select 5 lower bits of the value to set the lower part of the bank
      u8 romBank = value & 0x1F;
				
			// if it's zero we should increment by one
			if (romBank == 0)
				++romBank;
      
      // if we are in ROM banking mode we should keep bit 5-6 from current bank
      // because they are set by writing at 0x4000-0x5FFF
      if (status.rom_banking_mode)
        status.current_rom_bank = (status.current_rom_bank & 0x60) | romBank;
      else
        status.current_rom_bank = romBank;
        
			status.rom_bank_1 = &status.rom[16_kb*status.current_rom_bank];
		}
		// in this address space we can either select bit 5-6 for ROM banks or bank 0-3 for RAM
    // according to current banking mode
		else if (address <= 0x5FFF)
		{
			if (status.rom_banking_mode)
			{
        // we mix lower 5 bits of current rom bank with bit 5-6 of the value provided to select rom bank
        status.current_rom_bank = (status.current_rom_bank & 0x1F) | (value & 0x60);
        status.rom_bank_1 = &status.rom[16_kb*status.current_rom_bank];
			}
      else
      {
        status.current_ram_bank = value & 0x03;
        status.ram_bank = &status.ram[status.current_ram_bank*8_kb];
      }
		}
		// switching from ROM banking to RAM banking
		else if (address <= 0x7FFF)
		{
      status.rom_banking_mode = value & 0x01;
			
      // if we entered ROM banking mode we should restore
      // ram bank to 0 since you cannot address anyone other
			if (status.rom_banking_mode)
      {
        status.ram_bank = status.ram;
        status.current_ram_bank = 0;
      }
		}
    // RAM nella cart (più banchi switchabili nel caso, da 8kb l'uno)
    else if (address >= 0xA000 && address <= 0xBFFF)
    {
      // scrive basandosi sull'offset nel banco di RAM selezionato
      if (status.ram_enabled)
        status.ram_bank[address - 0xA000] = value;
    }
	}
  
  
  else if ((status.flags & MBC_MBC2) == MBC_MBC2)
  {
    /* this enables or less the RAM */
		if (address <= 0x1FFF)
		{	
			if ((value & 0x0A) == 0x0A)
				status.ram_enabled = true;
			else
				status.ram_enabled = false;
		}
    else if (address <= 0x3FFF)
		{
      u8 romBank = value & 0x0F;
      
      status.current_rom_bank = romBank;
      status.rom_bank_1 = &status.rom[status.current_rom_bank*16_kb];
    }
    else if (address >= 0xA000 && address <= 0xA1FF)
    {
      // scrive basandosi sull'offset nel banco di RAM selezionato
      if (status.ram_enabled)
        status.ram_bank[address - 0xA000] = value;
    }
  }
  
  else if ((status.flags & MBC_MBC3) == MBC_MBC3)
  {
    /* this enables or less the RAM and the RTC */
    if (address <= 0x1FFF)
    {
      if ((value & 0x0A) == 0x0A)
        status.ram_enabled = true;
      else
        status.ram_enabled = false;
    }
    /* selects rom bank 01h to 7fh (7 bits) */
    else if (address <= 0x3FFF)
    {
      u8 romBank = value & 0x7F;
      
      if (romBank == 0) romBank = 1;
      
      status.current_rom_bank = romBank;
      status.rom_bank_1 = &status.rom[status.current_rom_bank*16_kb];
    }
    /* select RAM bank or RTC register */
    else if (address >= 0x4000 && address <= 0x5FFF)
    {
      if (value < 0x04)
      {
        status.current_ram_bank = value & 0x0F;
        status.ram_bank = &status.ram[status.current_ram_bank*8_kb];
        status.rtc_override = false;
      }
      else if (value >= 0x08 && value <= 0x0C)
      {
        /* selects RTC register and mark that writes/read will occur there */
        rtc.select(value);
        status.rtc_override = true;
      }
    }
    /* RCT latch, this will freeze RTC registers until latched again */
    else if (address >= 0x6000 && address <= 0x7FFF)
    {
      rtc.writeLatch(value);
    }
    else if (address >= 0xA000 && address <= 0xBFFF)
    {
      // scrive basandosi sull'offset nel banco di RAM selezionato
      if (status.ram_enabled)
      {
        if (status.rtc_override)
          rtc.writeData(value);
        else
          status.ram_bank[address - 0xA000] = value;
      }
    }
    
  }
  
  
  else if ((status.flags & MBC_MBC5) == MBC_MBC5)
  {
    /* this enables or less the RAM */
		if (address <= 0x1FFF)
		{	
			if ((value & 0x0A) == 0x0A)
				status.ram_enabled = true;
			else
				status.ram_enabled = false;
		}
    // this sets 8 lower bits of 9bits for rom bank choose
    else if (address <= 0x2FFF)
    {
      status.current_rom_bank = (status.current_rom_bank & 0xFF00) | value;
      status.rom_bank_1 = &status.rom[status.current_rom_bank*16_kb];
    }
    else if (address <= 0x3FFF)
    {
      status.current_rom_bank = (status.current_rom_bank & 0xFF) | ((((u16)value) & 0x01) << 8);
      status.rom_bank_1 = &status.rom[status.current_rom_bank*16_kb];
    }
    else if (address >= 0x4000 && address <= 0x5FFF)
    {
      status.current_ram_bank = value & 0x0F;
      status.ram_bank = &status.ram[status.current_ram_bank*8_kb];
    }
    else if (address >= 0xA000 && address <= 0xBFFF)
    {
      // scrive basandosi sull'offset nel banco di RAM selezionato
      if (status.ram_enabled)
        status.ram_bank[address - 0xA000] = value;
    }
  }

}

// reads a byte from rom
u8 Cartridge::read(u16 address) const
{
  // first 16kb -> rom_bank_0
  if (address <= 0x3FFF)
    return status.rom_bank_0[address];
  // second 16k -> rom_bank_1
  else if (address >= 0x4000 && address <= 0x7FFF)
    return status.rom_bank_1[address-0x4000];
  // external ram 8k -> ram_bank
  else if (address >= 0xA000 && address <= 0xBFFF)
  {
    if (status.rtc_override)
      return rtc.read();
    else
      return status.ram_bank[address-0xA000];
  }

	return 0;
}

u32 Cartridge::romSize()
{
	if (header.rom_size <= 0x07)
		return 0x8000 << header.rom_size;
	else
	{
		/* TODO 0x52, 0x53, 0x54 */
	}
	
	return 0;
}

u32 Cartridge::ramSize()
{
	if ((status.flags & MBC_MBC2) == MBC_MBC2)
    return 512;
  
  switch (header.ram_size)
	{
		case 0x01: return 2_kb;
		case 0x02: return 8_kb;
		case 0x03: return 32_kb;
		default:
		case 0x00: return 0;
	}
}

void Cartridge::init(void)
{
	/* TODO: resetta lo status, sonasega */
}

void Cartridge::load(const path& rom_name)
{
	FILE *in = fopen(rom_name.c_str(), "rb");
  long length = rom_name.length();
  
  status.fileName = rom_name;
	
	fseek(in, 0x100, SEEK_SET);
	fread(&header, sizeof(GB_CART_HEADER), 1, in);
  fseek(in, 0, SEEK_SET);
	
	status.flags = 0x00;
  
  if (header.cgb_flag & 0x80 && rom_name.extension() == "gbc")
    status.flags |= MBC_CGB;
	
	/* in base al cart_type assegna le flag della rom */
	if (header.cart_type == 0x00 || header.cart_type == 0x08 || header.cart_type == 0x09)
		status.flags |= MBC_ROM;
	else if (header.cart_type >= 0x01 && header.cart_type <= 0x03)
		status.flags |= MBC_MBC1;
	else if (header.cart_type == 0x05 || header.cart_type == 0x06)
  {
		status.flags |= MBC_MBC2;
    // built-in 512x4bit ram
    status.flags |= MBC_RAM;
  }
  else if (header.cart_type >= 0x19 && header.cart_type <= 0x1E)
    status.flags |= MBC_MBC5;
		
	if (header.cart_type >= 0x0F && header.cart_type <= 0x13)
		status.flags |= MBC_MBC3;
		
	if (header.cart_type == 0x02 || header.cart_type == 0x03 || header.cart_type == 0x08 || header.cart_type == 0x09 ||
	    header.cart_type == 0x10 || header.cart_type == 0x12 || header.cart_type == 0x13 || header.cart_type == 0x1A ||
      header.cart_type == 0x1B || header.cart_type == 0x1E)
		status.flags |= MBC_RAM;
	
	if (header.cart_type == 0x03 || header.cart_type == 0x06 || header.cart_type == 0x09 || header.cart_type == 0x0F ||
	    header.cart_type == 0x10 || header.cart_type == 0x13)
		status.flags |= MBC_BATTERY;
		
	if (header.cart_type == 0x0F || header.cart_type == 0x10)
		status.flags |= MBC_TIMER;
	
	char *tmp_name = new char[12];
	memcpy(tmp_name, header.title, 11);
	
	tmp_name[11] = 0x00;
	
  printf("\nLOADING ROM\n");
	printf("-------------\n\n");
	printf("Rom name: %s\n", tmp_name);
	printf("Effective file length: %lu\n", length);
	printf("ROM size: %u\n", romSize());
	printf("RAM size: %u\n", ramSize());
  printf("Cart type: %.2x\n", header.cart_type);
	printf("Destination: %s\n", (header.dest_code == 0x00)?("Japanese"):("Not Japanese"));
	printf("Header CRC: %d\n", header.checksum);
	printf("Cart props %d\n", status.flags);



	if ((status.flags & MBC_ROM) == MBC_ROM)
	{
		printf("ROM allocating %ld bytes\r\n", length);
		
		/* il tipo è ROM -> dimensione max 32kb in due blocchi da 16kb */
		if (length > 32_kb)
			printf("ROM format invalid!\r\n");

		status.rom_bank_0 = (u8*)malloc(16_kb);
		
		fread(status.rom_bank_0, 1, 16_kb, in);
		
		status.rom_bank_1 = (u8*)malloc(length - 16_kb);
		
		fread(status.rom_bank_1, 1, length - 16_kb, in);
	}
  else if ((status.flags & MBC_MBC1) == MBC_MBC1)
	{
		unsigned long n = length / 16_kb;
		
		/* allochiamo n banks da 16kb */
		printf("MBC1 allocating %lu x 16kb = %ld bytes\r\n", n, length);

		status.rom = (u8*)calloc(length, sizeof(u8));
    fread(status.rom, 1, n*16_kb, in);
    
		status.rom_bank_0 = status.rom;
    status.rom_bank_1 = &status.rom[16_kb];
	}
  else if ((status.flags & MBC_MBC2) == MBC_MBC2)
  {
    unsigned long n = length / 16_kb;
		
		/* allochiamo n banks da 16kb */
		printf("MBC2 allocating %lu x 16kb = %ld bytes\r\n", n, length);
    
		status.rom = (u8*)calloc(length, sizeof(u8));
    fread(status.rom, 1, n*16_kb, in);
    
		status.rom_bank_0 = status.rom;
    status.rom_bank_1 = &status.rom[16_kb];
  }
  else if ((status.flags & MBC_MBC3) == MBC_MBC3)
  {
    unsigned long n = length / 16_kb;
    
    /* allochiamo n banks da 16kb */
    printf("MBC3 allocating %lu x 16kb = %ld bytes\r\n", n, length);
    
    status.rom = (u8*)calloc(length, sizeof(u8));
    fread(status.rom, 1, n*16_kb, in);
    
    status.rom_bank_0 = status.rom;
    status.rom_bank_1 = &status.rom[16_kb];
  }
  else if ((status.flags & MBC_MBC5) == MBC_MBC5)
  {
    unsigned long n = length / 16_kb;
		
		/* allochiamo n banks da 16kb */
		printf("MBC5 allocating %lu x 16kb = %ld bytes\r\n", n, length);
    
		status.rom = (u8*)calloc(length, sizeof(u8));
    fread(status.rom, 1, n*16_kb, in);
    
		status.rom_bank_0 = status.rom;
    status.rom_bank_1 = &status.rom[16_kb];
  }
  
  if ((status.flags & MBC_RAM) == MBC_RAM)
  {
    u16 size = ramSize();
    
    status.ram = (u8*)calloc(size, sizeof(u8));
    status.ram_bank = status.ram;
    
    /* allochiamo n banks da 8kb */
		printf("RAM allocating %u x 8kb = %u bytes\r\n", size/8_kb, size);
  }
  
  if ((status.flags & MBC_TIMER) == MBC_TIMER)
  {
    status.rtc = new u8[5];
    
    printf("TIMER allocating 5 bytes for RTC\r\n");
  }
		
	fclose(in);
  
  
  if (ramSize() > 0)
  {
    auto savePath = status.fileName.withExtension("sav");

    FILE *in = fopen(savePath.c_str(), "rb");
    
    if (in)
    {
      fread(status.ram, ramSize(), sizeof(u8), in);
      fclose(in);
    }
  }
}

void Cartridge::loadRaw(u8 *code, u32 length)
{
  status.flags |= MBC_ROM | MBC_SIMPLE;
  
  status.rom = (u8*)calloc(32_kb, sizeof(u8));
  status.rom_bank_0 = status.rom;
  status.rom_bank_1 = &status.rom[16_kb];
  
  status.ram = (u8*)calloc(8_kb, sizeof(u8));
  status.ram_bank = status.ram;
  
  u8 jump[4] = {0x00, 0xC3, 0x50, 0x01};
  memcpy(&status.rom_bank_0[0x100], jump, 4);
  memcpy(&status.rom_bank_0[0x150], code, length);
}

void Cartridge::dump()
{
  path out = path("rom.gb");
  out.writeAll(status.rom, 32_kb, sizeof(u8));
}

void Cartridge::dumpSave()
{
  u16 size = ramSize();
  
  if (size > 0)
  {
    path outName = status.fileName.withExtension(".sav");
    outName.writeAll(status.ram, sizeof(u8), size);
  }
}
