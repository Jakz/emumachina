#pragma once

#include "window.h"

#include "raylib.h"

namespace gfx
{
  union Pixel
  {
    uint32_t value;
    struct { uint8_t r, g, b, a; };

    Pixel(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t alpha = 255)
      : r(red), g(green), b(blue), a(alpha) {}
  };

  struct FrameBuffer
  {
  protected:
    int _width;
    int _height;
    std::vector<Pixel> _data;

  public:
    FrameBuffer(int width, int height)
      : _width(width), _height(height), _data(width * height) {}

    Pixel& pixel(int x, int y) { return _data[y * _width + x]; }
    const Pixel& pixel(int x, int y) const { return _data[y * _width + x]; }

    int width() const { return _width; }
    int height() const { return _height; }
    Pixel* data() { return _data.data(); }

    void fill(Pixel color);
  };

  struct Texture
  {
  protected:
    int _width, _height;
    Texture2D _texture;

  public:
    Texture(int width, int height);
    Texture(const Texture&) = delete;
    ~Texture();

    void update(void* data);
    const Texture2D& texture() const { return _texture; }
  };
}

namespace ui
{
  class FrameWindow : public Window
  {
  protected:
    std::unique_ptr<gfx::FrameBuffer> _frameBuffer;
    gfx::Texture _texture;
    bool _autoupdate;

    void doRender() override;

  public:
    FrameWindow(std::string_view title, int width, int height)
      : Window(title), _frameBuffer(std::make_unique<gfx::FrameBuffer>(width, height)), _texture(width, height), _autoupdate(true) {}

    gfx::FrameBuffer* frameBuffer() { return _frameBuffer.get(); }
    const gfx::FrameBuffer* frameBuffer() const { return _frameBuffer.get(); }

    void update();
  };
}