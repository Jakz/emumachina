#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include "SDL.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <algorithm>

constexpr float operator"" _mhz(long double val) { return static_cast<float>(val * 1'000'000.0L); }
constexpr float operator"" _khz(long double val) { return static_cast<float>(val * 1'000.0f); }
constexpr float operator"" _hz(long double val) { return static_cast<float>(val); }

constexpr uint32_t operator"" _kb(unsigned long long val) { return val * 1024; }

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

    void push(const T& value) {
      assert(!full());
      buffer[head] = value;
      head = (head + 1) & mask();
    }

    void push(T&& value) {
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
        case Waveform::Sine: return std::sinf(2.0f * M_PI * _phase);
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
        float rc = 1.0f / (2.0f * M_PI * _cutoff);
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
  SDL_AudioDeviceID _audioDevice;
public:
  Platform();

  bool initAudio();
  void closeAudio();

  bool init();

  static void audioCallback(void* userdata, uint8_t* stream, int len);
};

Platform::Platform() : _audioDevice(0) { }

bool Platform::initAudio()
{
  SDL_AudioSpec want{}, have{};
  want.freq = 44100;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 512;
  want.callback = audioCallback;
  want.userdata = this;

  _audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (!_audioDevice)
  {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    return false;
  }

  SDL_PauseAudioDevice(_audioDevice, 0);

  return true;
}

void Platform::closeAudio()
{
  if (_audioDevice)
    SDL_CloseAudioDevice(_audioDevice);
}

bool Platform::init()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0)
  {
    printf("Error: %s\n", SDL_GetError());
    return false;
  }

#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

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

  // Create window with SDL_Renderer graphics context
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  if (window == nullptr)
  {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  if (renderer == nullptr)
  {
    SDL_Log("Error creating SDL_Renderer!");
    return -1;
  }
  //SDL_RendererInfo info;
  //SDL_GetRendererInfo(renderer, &info);
  //SDL_Log("Current SDL_Renderer: %s", info.name);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
  //io.Fonts->AddFontDefault();
  //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
  //IM_ASSERT(font != nullptr);

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 256, 256);
  

  // Main loop
  bool done = false;
  while (!done)
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
      SDL_Delay(10);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Framebuffer", nullptr);

    void* tex_pixels;
    int tex_pitch;
    if (SDL_LockTexture(texture, NULL, &tex_pixels, &tex_pitch) == 0)
    {
      struct Star { int x, y; };
      static std::vector<Star> stars;
    
      if (stars.empty())
      {
        stars.reserve(256);
        for (int i = 0; i < stars.size(); ++i)
        {
          stars.push_back({ rand() % 256, rand() % 256 });
        }
      }

      // Clear the texture
      uint32_t randomColor = 0xffff00ff;
      std::fill_n(static_cast<uint32_t*>(tex_pixels), (tex_pitch / 4) * 256, randomColor);

      // Draw stars
      /*for (const auto& star : stars)
      {
        int x = star.x;
        int y = star.y;
        if (x >= 0 && x < 256 && y >= 0 && y < 256)
        {
          uint32_t* pixels = static_cast<uint32_t*>(tex_pixels);
          pixels[y * (tex_pitch / sizeof(uint32_t)) + x] = 0xFFFFFFFF; // White star
        }
      }

      SDL_UpdateTexture(texture, NULL, tex_pixels, tex_pitch);*/
      SDL_UnlockTexture(texture);
    }

    ImGui::Image((ImTextureID)texture, ImVec2(256, 256));
    ImGui::End();

    if (false)
    {
      
      ImGui::Begin("RAM Viewer", nullptr);

      const int cols = 16;
      size_t ram_size = 0x10000;
      static uint8_t* ram = nullptr;
      const int total_lines = ram_size / cols;
      
      /* init ram to random values */
      static bool inited = false;

      if (!inited)
      {
        ram = new uint8_t[0x10000];
        for (size_t i = 0; i < ram_size; ++i) {
          ram[i] = static_cast<uint8_t>(rand() % 256);
        }
        inited = true;
      }

      if (ImGui::BeginTable("ramtable", cols + 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
        ImGuiListClipper clipper;
        clipper.Begin(total_lines);

        ImGui::TableSetupColumn("address", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        for (int col = 0; col < cols; ++col)
          ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("ascii", ImGuiTableColumnFlags_WidthFixed, 120.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

        while (clipper.Step())
        {
          for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
          {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%04Xh", row * cols);

            std::string ascii;

            for (int col = 0; col < cols; ++col)
            {
              size_t index = row * cols + col;
              ImGui::TableSetColumnIndex(1 + col);
              /* ImGui::Text("%02X", ram[index]); */

              char buf[3];
              snprintf(buf, sizeof(buf), "%02X", ram[index]);

              ImGui::PushID(index);
              if (ImGui::InputText("##hex", buf, sizeof(buf),
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AlwaysOverwrite | ImGuiInputTextFlags_NoHorizontalScroll)) {

                // This block runs only when the user confirms the change
                unsigned int val;
                if (sscanf(buf, "%02X", &val) == 1)
                {
                  ram[index] = static_cast<uint8_t>(val);
                }
              }
              ImGui::PopID();

              ascii += std::isprint(ram[index]) ? char(ram[index]) : '.';
            }

            ImGui::TableSetColumnIndex(cols + 1);
            ImGui::TextUnformatted(ascii.c_str());
          }
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(1);

        ImGui::EndTable();
      }


      ImGui::End();
    }

    gui.windows.waveGenerator.render();

    // Rendering
    ImGui::Render();
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  // Cleanup
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  platform.closeAudio();

  SDL_Quit();

  return 0;
}
