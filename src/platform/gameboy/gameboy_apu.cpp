#include "gameboy_apu.h"

using namespace gb;

GBSound::GBSound() : bApu(std::unique_ptr<Basic_Gb_Apu>(new Basic_Gb_Apu()))
{

}

void GBSound::start(const int sampleRate)
{
  bApu->set_sample_rate(sampleRate);
  bQueue.start(sampleRate, 2);
}

void GBSound::reset()
{
  bQueue.stop();
}

void GBSound::write(u16 address, u8 value)
{
  bApu->write_register(address + 0xFF10, value);
}

u8 GBSound::read(u16 address)
{
  return bApu->read_register(address + 0xFF10);
}

void GBSound::mute(bool toggle)
{
  bApu->volume(toggle ? 0.0f : 1.0f);
}