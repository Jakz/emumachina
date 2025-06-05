#include "frame_window.h"

#include "imgui.h"


#include "SDL.h"

extern SDL_Renderer* renderer;

void gfx::FrameBuffer::fill(Pixel color)
{
  std::fill(_data.begin(), _data.end(), color);
}

gfx::Texture::Texture(int width, int height) : _opaque(nullptr), _width(width), _height(height)
{
  _opaque = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
}

gfx::Texture::~Texture()
{
  if (_opaque)
    SDL_DestroyTexture(static_cast<SDL_Texture*>(_opaque));
}

void gfx::Texture::update(void* data)
{
  if (_opaque)
    SDL_UpdateTexture(static_cast<SDL_Texture*>(_opaque), nullptr, data, _width * sizeof(gfx::Pixel));
}

using namespace ui;

void FrameWindow::doRender()
{
  if (_autoupdate)
    update();
  
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float scale = std::min(avail.x / 256, avail.y / 256);
  ImVec2 size = ImVec2(256 * scale, 256 * scale);

  // Per centrare:
  ImVec2 cursor = ImGui::GetCursorPos();
  ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) * 0.5f,
    cursor.y + (avail.y - size.y) * 0.5f));

  ImGui::Image((ImTextureID)_texture.opaque(), size);
}

void FrameWindow::update()
{
  _frameBuffer->fill(gfx::Pixel(0, 0, 0, 255));
  
  struct Star { int x; int y; int layer; };
  static std::vector<Star> stars;
  if (stars.empty())
  {
    for (size_t i = 0; i < 100; ++i)
    {
      stars.push_back({ rand() % _frameBuffer->width(), rand() % _frameBuffer->height(), rand() % 3 });
    }
  }
  
  /* update stars */
  for (auto& star : stars)
  {
    star.x -= star.layer + 1;
    if (star.x < 0)
    {
      star.x = _frameBuffer->width() - 1;
      star.y = rand() % _frameBuffer->height();
    }
  }
  
  /* draw stars */
  for (const auto& star : stars)
    _frameBuffer->set(star.x, star.y, gfx::Pixel(255 - star.layer * 80, 255 - star.layer * 80, 255 - star.layer * 80, 255));
  
  _texture.update(_frameBuffer->data());
}
