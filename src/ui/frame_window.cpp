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
  _texture.update(_frameBuffer->data());
}