#include "imgui.h"
#include "raylib.hpp"
#include "rlImGui.h"

#include "Vector2.hpp"
#include "Window.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <algorithm>

#include "raylib.h"

#include "common.h"
#include "devices/component.h"

#include "ui/window.h"
#include "ui/frame_window.h"

namespace devices
{
  struct Machine
  {
  protected:
    Bus _bus;
    std::vector<std::unique_ptr<Component>> _devices;

  public:
    template<typename T, typename... Args>
    T* add(Args&&... args) {
      auto device = std::make_unique<T>(std::forward<Args>(args)...);
      T* raw = device.get();
      _devices.push_back(std::move(device));
      return raw;
    }

    auto& bus() { return _bus; }
  };
}

namespace structures
{
  template<typename T, size_t N>
  struct RingBuffer
  {
    static_assert((N& (N - 1)) == 0, "N must be power of two");

  private:
    std::array<T, N> buffer{};
    size_t head = 0;
    size_t tail = 0;

    size_t mask() const { return N - 1; }

  public:

    bool empty() const { return head == tail; }
    bool full() const { return ((head + 1) & mask()) == tail; }
    size_t size() const { return (head - tail) & mask(); }
    size_t capacity() const { return N - 1; }

    void push(const T& value)
    {
      assert(!full());
      buffer[head] = value;
      head = (head + 1) & mask();
    }

    void push(T&& value)
    {
      assert(!full());
      buffer[head] = std::move(value);
      head = (head + 1) & mask();
    }

    void push(const T* values, size_t count) {
      assert(count <= capacity() - size());

      size_t space_to_end = N - head;
      if (count <= space_to_end) {
        std::copy(values, values + count, buffer.begin() + head);
      }
      else {
        std::copy(values, values + space_to_end, buffer.begin() + head);
        std::copy(values + space_to_end, values + count, buffer.begin());
      }
      head = (head + count) & mask();
    }

    T pop()
    {
      assert(!empty());
      T value = std::move(buffer[tail]);
      tail = (tail + 1) & mask();
      return value;
    }

    void pop(T* out, size_t count)
    {
      assert(count <= size());

      size_t space_to_end = N - tail;
      if (count <= space_to_end) {
        std::copy(buffer.begin() + tail, buffer.begin() + tail + count, out);
      }
      else {
        std::copy(buffer.begin() + tail, buffer.end(), out);
        std::copy(buffer.begin(), buffer.begin() + (count - space_to_end), out + space_to_end);
      }
      tail = (tail + count) & mask();
    }

    void clear()
    {
      head = tail = 0;
    }
  };
}

namespace sounds
{
  struct WaveGenerator
  {
  protected:
    float _frequency;
    float _clock;
    float _phase;

  public:
    WaveGenerator(float frequency = 440.0_hz, float clock = 1.0_mhz) : _frequency(frequency), _clock(clock), _phase(0.0f) { }
    float clock() const { return _clock; }
  };
  
  struct SquareWaveGenerator : public WaveGenerator
  {
  protected:
    float _duty;

  public:
    SquareWaveGenerator(float frequency = 440.0f) : WaveGenerator(frequency), _duty(0.5f) { }

    void setDuty(float duty) { _duty = std::clamp(duty, 0.0f, 1.0f); }

    float next()
    {
      /* increment by a clock cycle */
      _phase += _frequency / _clock;
      if (_phase >= 1.0f)
        _phase -= 1.0f;

      return _phase < _duty ? 1.0f : -1.0f; // Return high or low based on phase
    }
  };

  struct TriangleWaveGenerator : public WaveGenerator
  {
  public:
    TriangleWaveGenerator(float frequency = 440.0f) : WaveGenerator(frequency) { }

    float next()
    {
      /* increment by a clock cycle */
      _phase += _frequency / _clock;
      if (_phase >= 1.0f)
        _phase -= 1.0f;

      return 2.0f * std::abs(_phase - 0.5f) - 1.0f; // Return triangle wave value
    }
  };

  enum class Waveform { Square, Triangle, Sawtooth, Sine };

  class SimpleWaveGenerator : public WaveGenerator
  {
    Waveform _type;

  public:
    SimpleWaveGenerator(Waveform type, float frequency, float clock = 1.0_mhz) 
      : WaveGenerator(frequency, clock), _type(type) { }

    void waveform(Waveform type) { _type = type; }
    void frequency(float frequency) { _frequency = frequency; }

    float next()
    {
      _phase += _frequency / _clock;
      if (_phase >= 1.0f) _phase -= 1.0f;

      switch (_type)
      {
        case Waveform::Square: return (_phase < 0.5f) ? 1.0f : -1.0f;
        case Waveform::Sawtooth: return 2.0f * _phase - 1.0f;
        case Waveform::Triangle: return (_phase < 0.5f) ? (4.0f * _phase - 1.0f) : (3.0f - 4.0f * _phase);
        case Waveform::Sine: return std::sinf(2.0f * PI * _phase);
      }

      return 0.0f;
    }
  };

  struct NoiseGenerator : public WaveGenerator
  {
  protected:
    uint32_t lfsr = 0x7FFFFF;

  public:
    NoiseGenerator(float clock = 1.0_mhz) : WaveGenerator(0.0f, clock) { }

    float next()
    {
      // XOR taps at bit 22 and 17
      uint32_t new_bit = ((lfsr >> 22) ^ (lfsr >> 17)) & 1;
      lfsr = ((lfsr << 1) | new_bit) & 0x7FFFFF;
      uint8_t value = (lfsr >> 15) & 0xFF;

      return (value / 2.0f) - 1.0f;
    }
  };

  namespace filters
  {
    class LowPassFilter
    {
    protected:
      float _cutoff;
      float _sampleRate;
      float _alpha;
      float _prevOutput;

    public:
      LowPassFilter(float cutoff, float sampleRate) :
        _cutoff(cutoff), _sampleRate(sampleRate), _prevOutput(0.0f)
      {
        updateAlpha();
      }

      void updateAlpha()
      {
        float dt = 1.0f / _sampleRate;
        float rc = 1.0f / (2.0f * PI * _cutoff);
        _alpha = dt / (rc + dt);
      }

      float process(float input)
      {
        float output = _prevOutput + (_alpha * (input - _prevOutput));
        _prevOutput = output;
        return output;
      }
    };
  }
  
}

struct Platform
{
protected:
public:
  Platform();

  bool initAudio();
  void closeAudio();

  bool init();

  static void audioCallback(void* userdata, uint8_t* stream, int len);
};

Platform::Platform() { }

bool Platform::initAudio()
{
  return true;
}

void Platform::closeAudio()
{

}

bool Platform::init()
{
  
  return initAudio();
}

Platform platform;

namespace ImGui
{
  template<typename Enum>
  bool RadioButton(const char* label, Enum& current, Enum value) {
    int i = static_cast<int>(current);
    bool changed = ImGui::RadioButton(label, &i, static_cast<int>(value));
    if (changed)
      current = static_cast<Enum>(i);
    return changed;
  }
}

namespace ui::windows
{
  class WaveGeneratorWindow
  {
  protected:
    sounds::SimpleWaveGenerator _generator;    
    sounds::Waveform _waveform;
    float _frequency;
    float _volume;

  public:
    WaveGeneratorWindow() : _generator(sounds::Waveform::Square, 440.0_hz, 1.0_mhz),
      _waveform(sounds::Waveform::Square), _frequency(440.0_hz), _volume(0.5f)
    {
    
    }

    void render();
    float clock() const { return _generator.clock(); }
    float next() { return _generator.next(); }
  };
}

namespace ui
{
  struct UI
  {
    struct
    {
      ui::windows::WaveGeneratorWindow waveGenerator;
    } windows;

    ui::WindowManager manager;
  };
}

void ui::windows::WaveGeneratorWindow::render()
{
  ImGui::Begin("Waveform Controls");

  ImGui::Text("Select Waveform:");

  ImGui::RadioButton("Square", _waveform, sounds::Waveform::Square);
  ImGui::RadioButton("Triangle", _waveform, sounds::Waveform::Triangle);
  ImGui::RadioButton("Sawtooth", _waveform, sounds::Waveform::Sawtooth);
  ImGui::RadioButton("Sine", _waveform, sounds::Waveform::Sine);
  _generator.waveform(_waveform);

  ImGui::Spacing();
  ImGui::Text("Frequency(Hz)");
  ImGui::SliderFloat("##freqSlider", &_frequency, 20.0f, 20000.0f, "");
  ImGui::SameLine();
  ImGui::InputFloat("##freqText", &_frequency, 20.0f, 20000.0f, "%.1f");

  ImGui::SliderFloat("Volume", &_volume, 0.0f, 1.0f, "%.2f");
  _generator.frequency(_frequency);

  ImGui::End();
}

ui::UI gui;


structures::RingBuffer<float, 1024 * 1024> buffer;
sounds::filters::LowPassFilter filter(4.0_khz, 44100.0f); // Low-pass filter with 1kHz cutoff

void Platform::audioCallback(void* userdata, uint8_t* data, int len)
{
  return;
  
  float* stream = reinterpret_cast<float*>(data);
  len /= sizeof(float);

  float downsampleRatio = gui.windows.waveGenerator.clock() / 44100.0f;
  const size_t requiredSamples = static_cast<size_t>(len * downsampleRatio) + 1;

  /* generate samples */
  for (size_t i = 0; i < requiredSamples; ++i)
  {
    float sample = gui.windows.waveGenerator.next();
    buffer.push(sample);
  }

  float cursor = 0.0f;
  for (int i = 0; i < len; ++i)
  {
    float acc = 0.0f;
    int count = 0;

    while (cursor < 1.0f)
    {
      acc += buffer.pop();
      count++;
      cursor += 1.0f / downsampleRatio;
    }

    cursor -= 1.0f;
    stream[i] = (acc / static_cast<float>(count)) * 0.05f;
    stream[i] = filter.process(stream[i]);
  }
}

// Main code
int main(int, char**)
{
  devices::Machine machine;
  auto* ram = machine.add<devices::Ram>(0x10000); // 64KB RAM
  machine.bus().map(ram, 0x0000, 0xFFFF);
  
  platform.init();

  SetConfigFlags(FLAG_MSAA_4X_HINT);

  raylib::Window bootstrap(1, 1, "Bootstrap");

  int monitor = GetCurrentMonitor();
  raylib::Vector2 screenSize = raylib::Vector2(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
  screenSize.x = std::min(screenSize.x * 0.666f, 1920.0f);
  screenSize.y = screenSize.x * 1.0f / (16.0f / 10.0f);  // 16:9 aspect ratio

  bootstrap.Close();

  raylib::Window window = raylib::Window(screenSize.x, screenSize.y, "Procedurality");

  Image img = GenImagePerlinNoise(256, 256, 0, 0, 10.0f);
  Texture2D tex = LoadTextureFromImage(img);

  SetTargetFPS(60);
  rlImGuiSetup(true);

  auto* frameWindow = new ui::FrameWindow("Framebuffer", 256, 256);
  frameWindow->frameBuffer()->fill(gfx::Pixel(255, 255, 0));
  gui.manager.add(frameWindow);

  while (!WindowShouldClose())
  {
    BeginDrawing();
    ClearBackground(Color{ 0, 12, 31 });

    rlImGuiBegin();

    gui.windows.waveGenerator.render();

    gui.manager.render();

    rlImGuiEnd();

    EndDrawing();
  }


  platform.closeAudio();

  rlImGuiShutdown();
  CloseWindow();

  return 0;
}
