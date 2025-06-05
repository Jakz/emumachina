#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace ui
{
  class Window
  {
    std::string _title;
    bool _opened;

    virtual void doRender() = 0;

  public:
    Window(std::string_view title = "") : _title(title), _opened(true) { }
    virtual ~Window() = default;
    void render();
  };

  class WindowManager
  {
  protected:
    std::vector<std::unique_ptr<Window>> _windows;

  public:
    void add(Window* window) { _windows.push_back(std::unique_ptr<Window>(window)); }

    void render()
    {
      for (const auto& window : _windows)
        window->render();
    }
    
    void close()
    {
      _windows.clear();
    }
  };
}
