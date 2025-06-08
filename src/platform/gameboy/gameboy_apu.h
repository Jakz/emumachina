#pragma once

#include "common.h"
#include "devices/component.h"

#include <queue>
#include <cmath>
#include <memory>

#include "blarrg/Basic_Gb_Apu.h"
#include "blarrg/Sound_Queue.h"

namespace gb
{
  class GBSound : public devices::Addressable
  {
    private:
      Sound_Queue bQueue;
      std::unique_ptr<Basic_Gb_Apu> bApu;

    public:
      GBSound();
      void reset();

      void start(const int sampleRate);

      void write(u16 address, u8 value) override;
      u8 read(u16 address) override;

      u8 peek(u16 address) const override { return bApu->read_register(address + 0xFF10); }
      void poke(u16 address, u8 value) override { bApu->write_register(address + 0xFF10, value); }

      void update()
      {
        bApu->end_frame();
        writeBlarggSamples();
      }

      void writeBlarggSamples()
      {
        constexpr int buf_size = 2048;
        static blip_sample_t buf [buf_size];
        // Play whatever samples are available
        long count = bApu->read_samples( buf, buf_size );
        bQueue.write( buf, count );
      }

      void mute(bool toggle);
  };
}

