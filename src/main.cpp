// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL2.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include <stdio.h>
#include <SDL.h>

#include <string>
#include <vector>
#include <memory>

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

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

// Main code
int main(int, char**)
{
  devices::Machine machine;
  auto* ram = machine.add<devices::Ram>(0x10000); // 64KB RAM
  machine.bus().map(ram, 0x0000, 0xFFFF);
  
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

  // Create window with SDL_Renderer graphics context
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, window_flags);
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

  // Our state
  bool show_demo_window = false;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

      ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
      ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
      ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me"))
        show_another_window = false;
      ImGui::End();
    }

    if (true)
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
  SDL_Quit();

  return 0;
}
