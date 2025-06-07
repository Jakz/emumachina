#include "gameboy_ppu.h"

#include "devices/component.h"
#include "gameboy.h"

using namespace gb;

namespace gb
{
  enum PriorityType : u8
  {
    PRIORITY_NONE = 0,
    PRIORITY_SPRITE = 1,
    PRIORITY_BG = 2,
    PRIORITY_MAYBE_SPRITE = 3
  };
}

enum LCDC_Mask : u8
{
  LCDC_DISPLAY_ENABLE = 7,
  LCDC_WINDOW_TILE_MAP_SELECT = 6,
  LCDC_WINDOW_DISPLAY_ENABLE = 5,
  LCDC_BG_WINDOW_TILE_DATA_SELECT = 4,
  LCDC_BG_TILE_MAP_SELECT = 3,
  LCDC_SPRITE_SIZE = 2,
  LCDC_SPRITE_DISPLAY_ENABLE = 1,
  LCDC_BG_DISPLAY_MODE = 0
};

enum TileAttributeMask : u8
{
  ATTRIB_PRIORITY = 7,
  ATTRIB_FLIP_VERTICAL = 6,
  ATTRIB_FLIP_HORIZONTAL = 5,
  ATTRIB_PALETTE_GB = 4,
  ATTRIB_VRAM_BANK = 3,
  ATTRIB_PALETTE_CGB_MASK = 0x07
};

enum STAT_Mask
{
  STAT_INTERRUPT_COINCIDENCE = 6,
  STAT_INTERRUPT_OAM = 5,
  STAT_INTERRUPT_VBLANK = 4,
  STAT_INTERRUPT_HBLANK = 3,
  STAT_COINCIDENCE_FLAG = 2,
  STAT_BIT1_MODE = 1,
  STAT_BIT0_MODE = 0,
  STAT_MODE_MASK = 0x03
};

GpuGB::GpuGB(devices::Bus* bus, Gameboy* system, const EmuSpec& spec) : bus(bus), system(system), width(spec.displayWidth), height(spec.displayHeight),
bcolors{ccc(28, 31, 26),ccc(17, 24, 14),ccc(4, 13, 11),ccc(1,3,4)}
{
  priorityMap = new PriorityType[width*height];
  
  scanlineCounter = CYCLES_PER_SCANLINE;

  init();
}

GpuGB::~GpuGB()
{
  delete [] buffer;
  delete [] priorityMap;
}

pixel_t GpuGB::ccc(u8 r, u8 g, u8 b)
{
  return (r*8) << 24 | (g*8) << 16 | (b*8) << 8 | 0xFF;
}

void GpuGB::colorsForPalette(DrawLayer layer, u8 index, pixel_t(&palette)[4])
{
  if (system->mode == MODE_GB)
  {
    u8 indices;

    // if layer is monochrome bg or window then we have just a palette to choose from
    // and index passed is ignored
    if (layer == LAYER_BACKGROUND)
      indices = bus->read(PORT_BGP);
    else
    {
      if (index == 0)
        indices = bus->read(PORT_OBP0);
      else
        indices = bus->read(PORT_OBP1);
    }
    
    for (int i = 0; i < 4; ++i)
      palette[i] = bcolors[(indices >> (i*2)) & 0x03];
    
  }
  else if (system->mode == MODE_CGB)
  {
    constexpr size_t BYTES_PER_COLOR = 2;
    constexpr size_t COLORS_PER_PALETTE = 4;
    constexpr size_t PALETTE_SIZE = BYTES_PER_COLOR * COLORS_PER_PALETTE;
    
    // color palette is made by 2 bytes per color, 4 colors so 8 bytes per palette
    // bg color palettes are stored starting at 0x00 of palette ram
    // sprite color palettes are stored starting at 0x40
    // since it's little endian we first fetch lower byte, then higher one and compose together to form the specific color
    u8 offset = (layer == LAYER_BACKGROUND ? 0x00 : 0x40) + index * PALETTE_SIZE;
    
    u16 colors16[4];
    
    for (int i = 0; i < 4; ++i)
      colors16[i] = (mem.paletteRam(offset + 2*i)) | (mem.paletteRam(offset + 2*i + 1) << 8);
    
    // color layout is XXBBBBBGG GGGRRRRR so 5 bits per component = 32*32*32 colors
    // conversion to rgb is made by multiplying by 8 the raw value even it should be improved
    for (int i = 0; i < 4; ++i)
    {
      u8 r = colors16[i] & 0x1F;
      u8 g = (colors16[i] >> 5) & 0x1F;
      u8 b = (colors16[i] >> 10) & 0x1F;
      
      palette[i] = ccc(r,g,b);
    }
  }
}

void GpuGB::init()
{
  line = 0;
}

void GpuGB::reset()
{
  init();
}

bool GpuGB::isEnabled()
{
  return bit::bit(bus->peek(PORT_LCDC), LCDC_DISPLAY_ENABLE);
}

void GpuGB::update(u8 cycles)
{
  Mode oldMode = static_cast<Mode>(bus->peek(PORT_STAT) & STAT_MODE_MASK);
  
  scanlineCounter -= cycles;
 
  manageSTAT();
  
  Mode mode = static_cast<Mode>(bus->peek(PORT_STAT) & STAT_MODE_MASK);
  
  if (!isEnabled())
    return;
    
  if (mode == Mode::HBLANK && oldMode != mode && line < VBLANK_START_LINE)
    drawScanline(line);
  
  if (scanlineCounter < CYCLES_PER_SCANLINE - VBLANK_CYCLES_BEFORE_LY_RESET && line == VBLANK_END_LINE)
    bus->poke(PORT_LY, 0);

  // cycles for a scanline expired, move to next one
  if (scanlineCounter <= 0)
  {
    ++line;
    bus->poke(PORT_LY, bus->peek(PORT_LY) + 1);
    
    // reset scanlineCounter to beginning of next line, we subtract the excess cycles just to be more precise
    scanlineCounter = CYCLES_PER_SCANLINE + scanlineCounter;
    
    // if we reached V-BLANK then raise the interrupt
    if (line == VBLANK_START_LINE)
      emu.requestInterrupt(INT_VBLANK);
    // if we got over V-BLANK then reset the counter
    else if (line == VBLANK_END_LINE)
      mem.rawPortWrite(PORT_LY, VBLANK_END_LINE);
    else if (line > VBLANK_END_LINE)
    {
      line = 0;
      bus->poke(PORT_LY, 0);
      setMode(mem.rawPort(PORT_STAT), Mode::OAM_TRANSFER);
      memset(priorityMap, PRIORITY_NONE, width*height*sizeof(PriorityType));
    }
    
  }
}

void GpuGB::manageSTAT()
{
  u8 status = bus->peek(PORT_STAT);
  
  if (!isEnabled())
  {
    scanlineCounter = CYCLES_PER_SCANLINE;
    
    // reset current vertical line
    bus->poke(PORT_LY, 0);
    
    // clear current mode by clearing 2 lower bits and then set mode 1
    setMode(status, Mode::VBLANK);
    
    // write status back
    bus->poke(PORT_STAT, status);
    
    return;
  }
  
  Mode currentMode = static_cast<Mode>(status & STAT_MODE_MASK);
  
  Mode mode = currentMode;
  bool willRequestInterrupt = false;
  
  // if we're over line 144 then V-BLANK has started, set mode 1
  if (line >= VBLANK_START_LINE)
  {
    mode = VBLANK;
    willRequestInterrupt = bit::bit(status, STAT_INTERRUPT_VBLANK);
  }
  else
  {
    constexpr u32 boundToMode2 = CYCLES_PER_SCANLINE - CYCLES_PER_OAV_TRANSFER;
    constexpr u32 boundToMode3 = boundToMode2 - CYCLES_PER_OAV_VRAM_TRANSFER;
    
    // we're in mode 2 (oam transfer)
    if (scanlineCounter > boundToMode2)
    {
      mode = OAM_TRANSFER;
      willRequestInterrupt = bit::bit(status, STAT_INTERRUPT_OAM);
    }
    // we're in mode 3 (oam/vram transfer), no interrupt here
    else if (scanlineCounter > boundToMode3)
    {
      mode = OAM_VRAM_TRANSFER;
    }
    // we're in mode 0, which is H-BLANK
    else
    {
      mode = HBLANK;
      
      willRequestInterrupt = bit::bit(status, STAT_INTERRUPT_HBLANK);
      
      // manage HDMA if it's active
      HDMA *hdma = mem.hdmaInfo();
      if (hdma->active)
      {
        --hdma->length;
        
        // transfer 16 bytes
        for (int i = 0; i < 0x10; ++i)
        {
          mem.write(hdma->dest, bus->read(hdma->src));
          ++hdma->dest;
          ++hdma->src;
        }
        
        if (hdma->length == 0)
        {
          hdma->active = false;
          mem.rawPortWrite(PORT_HDMA5, 0xFF);
        }
        else
        {
          mem.rawPortWrite(PORT_HDMA5, hdma->length);
        }
      }
    }
  }
  
  setMode(status, mode);
  
  // if  we switched to a new mode and its interrupt was enabled
  if (willRequestInterrupt && currentMode != mode)
  {
    emu.requestInterrupt(INT_STAT);
  }
  
  // if LY == LYC we should set coincidence bit and request interrupt if the bit is enabled
  if (line == bus->read(PORT_LYC))
  {
    status = bit::set(status, STAT_COINCIDENCE_FLAG);
    
    if (bit::bit(status, STAT_INTERRUPT_COINCIDENCE))
    {
      emu.requestInterrupt(INT_STAT);
    }
  }
  else
  {
    status = bit::res(status, STAT_COINCIDENCE_FLAG);
  }
  
  bus->poke(PORT_STAT, status);
}

void GpuGB::drawScanline(u8 line)
{
  //printf("scanline: %d\n", line);
  
  u8 lcdc = bus->read(PORT_LCDC);
  
  if (!isEnabled())
    return;
  
  /* in gb mode bit 0 enables or disables background */
  if (system->mode == gb::Mode::MODE_GB)
  {
    // if background is enabled draw it
    if (bit::bit(lcdc, LCDC_BG_DISPLAY_MODE))
      drawTiles(line);
    
    // if window is enabled draw it
    if (bit::bit(lcdc, LCDC_WINDOW_DISPLAY_ENABLE))
      drawWindow(line);
    
    // if sprites are enabled draw them
    if (bit::bit(lcdc, LCDC_SPRITE_DISPLAY_ENABLE))
      drawSprites(line);
  }
  /* in cgb mode bit 0 makes background and window lose priority over sprites */
  else if (system->mode == gb::MODE_CGB)
  {
    // if background is enabled draw it
    if (bit::bit(lcdc, LCDC_BG_DISPLAY_MODE))
      drawTiles(line);
    
    // if window is enabled draw it
    if (bit::bit(lcdc, LCDC_WINDOW_DISPLAY_ENABLE))
      drawWindow(line);
    
    // if sprites are enabled draw them
    if (bit::bit(lcdc, LCDC_SPRITE_DISPLAY_ENABLE))
      drawSprites(line);
  }
  /* in cgb in gb mode bit 0 disables both background and window (regardless of bit 5) */
  else if (system->mode == gb::MODE_CGB_IN_GB)
  {
    bool masterEnable = bit::bit(lcdc, LCDC_BG_DISPLAY_MODE);
    
    if (masterEnable)
    {
      drawTiles(line);
      
      // if window is enabled draw it
      if (bit::bit(lcdc, LCDC_WINDOW_DISPLAY_ENABLE))
        drawWindow(line);
    }
    
    // if sprites are enabled draw them
    if (bit::bit(lcdc, LCDC_SPRITE_DISPLAY_ENABLE))
      drawSprites(line);
  }
}

void GpuGB::drawTiles(u8 line)
{
  u8 lcdc = bus->read(PORT_LCDC);
  bool priorityEnabled = false; /* seems to be not the real thing  && emu.mode == MODE_CGB && Utils::bit(lcdc, LCDC_BG_DISPLAY_MODE); */
  
  const u8 *vram = mem.memoryMap()->vram;
  
  u16 tileData;
  u16 tileMap;
  bool isUnsigned = true;
  
  // choose tile data according to bit in lcd control register
  // since in CGB tile data can be in two different banks we just use deltas from 0x8000
  // and access vram directly
  if (bit::bit(lcdc, LCDC_BG_WINDOW_TILE_DATA_SELECT))
    tileData = 0x0000;
  else
  {
    tileData = 0x0800;
    isUnsigned = false;
  }
  
  // if mode is GB then offset of VRAM will be always 0x0000 since there is only one bank of VRAM
  // (this operation is useless, is here just to be precise in behavior)
  if (system->mode == MODE_GB)
    tileData += 0x0000;
  
  // choose tile map according to bit in lcd control register
  if (bit::bit(lcdc, LCDC_BG_TILE_MAP_SELECT))
    tileMap = TILE_MAP2;
  else
    tileMap = TILE_MAP1;
  
  // let's get the scroll values for the background tilemap
  u8 scx = bus->read(PORT_SCX);
  u8 scy = bus->read(PORT_SCY);
  
  u16 tileAddress;
  
  
  pixel_t colors[4];
  
  // if we're in mono gb mode we have just a palette for the background
  if (system->mode == MODE_GB)
    colorsForPalette(LAYER_BACKGROUND, 0, colors);
  
  //printf("line %d: ", line);
  
  // for every pixel of the current line
  for (int i = 0; i < width; ++i)
  {
    // compute the pixel position inside the tilemap and wrap it around edges if needed
    u8 y = (line + scy) % (TILE_MAP_HEIGHT*TILE_HEIGHT);
    u8 x = (i + scx) % (TILE_MAP_WIDTH*TILE_WIDTH);
    
    // with integer division we retrieve the tile coordinate inside the tile map
    u8 tx = x / TILE_WIDTH;
    u8 ty = y / TILE_HEIGHT;
    
    // with the modulo we get the pixel coordinate inside the specific tile
    u8 px = x % TILE_WIDTH;
    u8 py = y % TILE_HEIGHT;
    
    u8 byte1, byte2;
    bool flipX = false, flipY = false;
    
    // now we retrieve the tile address, first we lookup the index in the tile map
    // then we retrieve it from tile data address according to sign of the index identifier
    if (system->mode == MODE_GB)
    {
      u8 index = bus->read(tileMap + TILE_MAP_WIDTH*ty + tx);
      
      if (isUnsigned)
        tileAddress = tileData + TILE_BYTES_SIZE*index;
      else
        tileAddress = tileData + TILE_BYTES_SIZE*(((s8)index)+128);
      
      // get the two bytes for the correct row
      byte1 = vram[tileAddress + py*2];
      byte2 = vram[tileAddress + py*2 + 1];
    }
    // in CGB mode we should read the tile map data from vram bank 1 to read additional tile attributes
    else
    {
      u8 index = mem.readVram0(tileMap + TILE_MAP_WIDTH*ty + tx);
      u8 tileAttributes = mem.readVram1(tileMap + TILE_MAP_WIDTH*ty + tx);
      
      colorsForPalette(LAYER_BACKGROUND, tileAttributes & 0x07 , colors);
      
      u16 vramBank = 0x0000;
      
      // correct tileData according to which bank is set in bit 3
      if (bit::bit(tileAttributes, ATTRIB_VRAM_BANK))
        vramBank = 8_kb;
        

      if (isUnsigned)
        tileAddress = vramBank + tileData + TILE_BYTES_SIZE*index;
      else
        tileAddress = vramBank + tileData + TILE_BYTES_SIZE*(((s8)index)+128);

      flipX = bit::bit(tileAttributes, ATTRIB_FLIP_HORIZONTAL);
      flipY = bit::bit(tileAttributes, ATTRIB_FLIP_VERTICAL);

      if (bit::bit(tileAttributes, ATTRIB_PRIORITY))
        priorityEnabled = true;
      
      if (flipY)
        py = 7 - py;

      byte1 = vram[tileAddress + py*2];
      byte2 = vram[tileAddress + py*2 + 1];
    }
  
    // we can cache the current row and keep on until reaching the last pixel of the current line
    // or the end of the tile
    do {
      // shift them to get the correct bit for x in position 0 and build the palette index by
      // shifting lower bit to the left for the most significant bit of palette
      u8 index;
      if (!flipX)
        index = ((byte2 >> (7 - px)) & 0x01) << 1 | ((byte1 >> (7 - px)) & 0x01);
      else
        index = ((byte2 >> px) & 0x01) << 1 | ((byte1 >> px) & 0x01);
      
      PriorityType priority = PRIORITY_NONE;
      
      if (priorityEnabled)
        priority = PRIORITY_BG;
      else if (index == 0)
        priority = PRIORITY_MAYBE_SPRITE;
      
      u32 color = colors[index];
      
      // place pixel on buffer at (i, line)
      buffer[line*width+i] = color;
      priorityMap[line*width+i] = priority;
      
      // increase current pixel (i), current pixel in tile (px)
      ++px;
      if (px >= TILE_WIDTH)
        break;
      
      ++i;
      if (i >= width)
        break;

    } while (true);
  }
  
  //printf("\n");

}

void GpuGB::drawWindow(u8 line)
{
  u8 lcdc = bus->read(PORT_LCDC);
  bool priorityEnabled = false; /* seems to be not the real thing  && emu.mode == MODE_CGB && Utils::bit(lcdc, LCDC_BG_DISPLAY_MODE); */
  
  u16 tileData;
  u16 tileMap;
  bool isUnsigned = true;
  
  // choose tile data according to bit in lcd control register
  if (bit::bit(lcdc, LCDC_BG_WINDOW_TILE_DATA_SELECT))
    tileData = TILE_DATA1;
  else
  {
    tileData = TILE_DATA2;
    isUnsigned = false;
  }
  
  // choose tile map according to bit in lcd control register
  if (bit::bit(lcdc, LCDC_WINDOW_TILE_MAP_SELECT))
    tileMap = TILE_MAP2;
  else
    tileMap = TILE_MAP1;
  
  // let's get the scroll values for the background tilemap
  s16 wx = bus->read(PORT_WX) - 7;
  s16 wy = bus->read(PORT_WY);
  
  u16 tileAddress;
  
  
  pixel_t colors[4];
  
  bool flipX = false, flipY = false;
  
  // if we're in mono gb mode we have just a palette for the background
  if (system->mode == MODE_GB)
    colorsForPalette(LAYER_BACKGROUND, 0, colors);
  
  // for every pixel of the current line
  for (int i = 0; i < width; ++i)
  {
    PriorityType priority = priorityMap[line*width + i];
    
    if (line >= wy && i >= wx)
    {
      // compute the pixel position inside the tilemap and wrap it around edges if needed
      u8 y = line - wy;
      u8 x = i - wx;
      
      // with integer division we retrieve the tile coordinate inside the tile map
      u8 tx = x / TILE_WIDTH;
      u8 ty = y / TILE_HEIGHT;
      
      // with the modulo we get the pixel coordinate inside the specific tile
      u8 px = x % TILE_WIDTH;
      u8 py = y % TILE_HEIGHT;
      
      u8 byte1, byte2;
      
      // now we retrieve the tile address, first we lookup the index in the tile map
      // then we retrieve it from tile data address according to sign of the index identifier
      if (system->mode == MODE_GB)
      {
        u8 index = bus->read(tileMap + TILE_MAP_WIDTH*ty + tx);
        
        if (isUnsigned)
          tileAddress = tileData + TILE_BYTES_SIZE*index;
        else
          tileAddress = tileData + TILE_BYTES_SIZE*(((s8)index)+128);
        
        // get the two bytes for the correct row
        byte1 = mem.readVram0(tileAddress + py*2);
        byte2 = mem.readVram0(tileAddress + py*2 + 1);
      }
      // in CGB mode we should read the tile map data from vram bank 1 to read additional tile attributes
      else
      {
        u8 index = mem.readVram0(tileMap + TILE_MAP_WIDTH*ty + tx);
        u8 tileAttributes = mem.readVram1(tileMap + TILE_MAP_WIDTH*ty + tx);
        
        if (bit::bit(tileAttributes, ATTRIB_PRIORITY))
          priorityEnabled = true;
        
        colorsForPalette(LAYER_BACKGROUND, tileAttributes & ATTRIB_PALETTE_CGB_MASK , colors);
        
        if (isUnsigned)
          tileAddress = tileData + TILE_BYTES_SIZE*index;
        else
          tileAddress = tileData + TILE_BYTES_SIZE*(((s8)index)+128);

        flipX = bit::bit(tileAttributes, ATTRIB_FLIP_HORIZONTAL);
        flipY = bit::bit(tileAttributes, ATTRIB_FLIP_VERTICAL);
        
        if (flipY)
          py = 7 - py;
        
        // TODO priority flag
        
        // tile is from vram bank 1
        if (bit::bit(tileAttributes, ATTRIB_VRAM_BANK))
        {
          byte1 = mem.readVram1(tileAddress + py*2);
          byte2 = mem.readVram1(tileAddress + py*2 + 1);
        }
        else
        {
          byte1 = mem.readVram0(tileAddress + py*2);
          byte2 = mem.readVram0(tileAddress + py*2 + 1);
        }
      }
      
      // we can cache the current row and keep on until reaching the last pixel of the current line
      // or the end of the tile
      do {
        // shift them to get the correct bit for x in position 0 and build the palette index by
        // shifting lower bit to the left for the most significant bit of palette
        u8 index;
        
        if (!flipX)
          index = ((byte2 >> (7 - px)) & 0x01) << 1 | ((byte1 >> (7 - px)) & 0x01);
        else
          index = ((byte2 >> px) & 0x01) << 1 | ((byte1 >> px) & 0x01);
        
        priority = priorityMap[line*width+i];
        
        if (priorityEnabled)
          priority = PRIORITY_BG;
        else if (index == 0)
          priority = PRIORITY_MAYBE_SPRITE;
        
        u32 color = colors[index];
        
        // place pixel on buffer at (i, line)
        priorityMap[line*width+i] = priority;
        buffer[line*width+i] = color;
        
        // increase current pixel (i), current pixel in tile (px)
        ++px;
        if (px >= TILE_WIDTH)
          break;
        
        ++i;
        if (i >= width)
          break;
        
      } while (true);
    }
  }
}

void GpuGB::drawSprites(u8 line)
{
  u8 *oam = mem.oam();
  u8 *vram = mem.memoryMap()->vram;
  bool hasBgPriority = false;
  
  
  u16 tileData = 0x0000;
  
  // if mode is GB then tile data is always 0x8000, so 0x0000 since we're reading directly from VRAM
  if (system->mode == MODE_GB)
    tileData = 0x0000;

  // check if sprites are 8x16
  bool doubleSize = bit::bit(bus->read(PORT_LCDC), LCDC_SPRITE_SIZE);
  u8 height = doubleSize ? TILE_HEIGHT*2 : TILE_HEIGHT;
  
  int drawn = 0;
  
  
  for (int i = 0; i < SPRITE_MAX_COUNT && drawn < 10; ++i)
  //for (int i = 0; i < SPRITE_MAX_COUNT; ++i)
  {
    s16 y = oam[4*i];
    s16 x = oam[4*i + 1];
    u8 index = oam[4*i + 2];
    u8 flags = oam[4*i + 3];
    hasBgPriority = bit::bit(flags, ATTRIB_PRIORITY);
    
    // if x or y is outside bounds the sprite is hidden
    if (x == 0 || x >= 168 || y == 0 || y >= 160)
      continue;

    y -= 16;
    x -= 8;
        
    bool flipY = bit::bit(flags, ATTRIB_FLIP_VERTICAL);
    bool flipX = bit::bit(flags, ATTRIB_FLIP_HORIZONTAL);
    
    
    if (emu.mode == MODE_CGB)
    {
      if (bit::bit(flags, ATTRIB_VRAM_BANK))
        tileData = 8_kb;
      else
        tileData = 0;
    }
    
    // TODO: manage bit 7 OBJ-to-BG priority of flags
    
    // if the sprite resides on the line we're drawing
    if (line >= y && line < y+height)
    {
      pixel_t colors[4];
      ++drawn;
      
      // if mode is gb mono then just a bit is used for sprite palette, otherwise
      // lower 3 bits are used
      if (emu.mode == MODE_GB)
        colorsForPalette(LAYER_SPRITE, bit::bit(flags, ATTRIB_PALETTE_GB) ? 1 : 0, colors);
      else
        colorsForPalette(LAYER_SPRITE, flags & ATTRIB_PALETTE_CGB_MASK, colors);
      
      u8 sy;
      u16 spriteAddress;

      // if sprite is 8x8 then handling is easier
      if (!doubleSize)
      {
        // sprites resides just in first tiledata set
        spriteAddress = tileData + TILE_BYTES_SIZE*index;
        
        // row of the sprite that should be drawn
        // if sprite is vertically flipped we then start from the bottom
        sy = flipY ? (TILE_HEIGHT - 1) - (line - y) : (line - y);
                
      }
      else
      {
        sy = flipY ? (TILE_HEIGHT*2 - 1) - (line - y) : (line - y);
        
        // if line is 0-7 then it's upper tile, otherwise it's lower tile
        if (sy < 8)
          spriteAddress = tileData + TILE_BYTES_SIZE*(index & 0xFE);
        else
        {
          spriteAddress = tileData + TILE_BYTES_SIZE*(index | 0x01);
          sy %= 8;
        }
      }
      
      // get the two bytes for the correct row
      u8 byte1 = vram[spriteAddress + sy*2];
      u8 byte2 = vram[spriteAddress + sy*2 + 1];
      
      int sx = 0;
      for ( ; sx < 8 /*&& sx+x < width*/; ++sx)
      {
        if (sx+x >= 0 && sx+x < width)
        {          
          u8 index;
          
          if (!flipX)
            index = ((byte2 >> (7 - sx)) & 0x01) << 1 | ((byte1 >> (7 - sx)) & 0x01);
          else
            index = ((byte2 >> sx) & 0x01) << 1 | ((byte1 >> sx) & 0x01);
          
          u32 color = colors[index];
          
          PriorityType priority = priorityMap[line*width+x+sx];
          if (priority == PRIORITY_BG || (hasBgPriority && priority != PRIORITY_MAYBE_SPRITE))
            continue;
          
          //if (/*priorityMap[line*width+x+sx] == PRIORITY_FORCED ||*/ (priorityMap[line*width+x+sx] == PRIORITY_POSSIBLE))
          //  continue;
          
          if (index != 0)
            buffer[line*width+x+sx] = color;
        }
      }
    }
  }
}
