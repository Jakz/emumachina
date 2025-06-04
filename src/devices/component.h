#pragma once

#include <cstdint>

namespace devices
{
  using addr_t = uint16_t;

  struct Component
  {
    virtual ~Component() = default;
    virtual std::string name() const { return ""; }
  };

  struct Memory
  {
    virtual ~Memory() = default;
    virtual uint8_t read(addr_t address) const = 0;
    virtual void write(addr_t address, uint8_t value) = 0;
  };

  struct Rom : public Memory, public Component
  {
  protected:
    std::vector<uint8_t> _data;

  public:
    Rom(size_t size) : _data(size, 0) {}

    uint8_t read(addr_t address) const override
    {
      if (address < _data.size())
        return _data[address];
      return 0xFF;
    }

    virtual void write(addr_t, uint8_t) override
    {
      /* ROMs are typically read - only, so we ignore writes. */
    }

    void load(const std::vector<uint8_t>& data)
    {
      if (data.size() <= _data.size())
        std::copy(data.begin(), data.end(), _data.begin());
    }
  };

  struct Ram : public Memory, public Component
  {
  protected:
    std::vector<uint8_t> _data;

  public:
    Ram(size_t size) : _data(size, 0) {}

    uint8_t read(addr_t address) const override
    {
      if (address < _data.size())
        return _data[address];
      return 0xFF;
    }

    void write(addr_t address, uint8_t value) override
    {
      if (address < _data.size())
        _data[address] = value;
    }
  };

  struct CPU : public Component
  {
  public:
    virtual void reset() = 0;
    virtual void execute() = 0;
  };

  struct Bus
  {
  protected:
    struct BusMapping
    {
      addr_t start, end;
      Memory* device;
    };

    std::vector<BusMapping> _mappings;

  public:
    void map(Memory* device, addr_t start, addr_t end)
    {
      assert(end > start);
      _mappings.push_back({ start, end, device });
    }

    uint8_t read(addr_t address) const
    {
      for (const auto& mapping : _mappings)
      {
        if (address >= mapping.start && address <= mapping.end)
          return mapping.device->read(address - mapping.start);
      }
      return 0xFF;
    }

    void write(addr_t address, uint8_t value)
    {
      for (const auto& mapping : _mappings)
      {
        if (address >= mapping.start && address <= mapping.end)
        {
          mapping.device->write(address - mapping.start, value);
          return;
        }
      }
    }
  };
}