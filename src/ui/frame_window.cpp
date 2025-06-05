#include "frame_window.h"

#include "imgui.h"

#include "raylib.h"
#include "rlImGui.h"

void gfx::FrameBuffer::fill(Pixel color)
{
  std::fill(_data.begin(), _data.end(), color);
}

gfx::Texture::Texture(int width, int height) : _width(width), _height(height)
{
  /* create texture */
  std::unique_ptr<uint8_t[]> data(new uint8_t[width * height * 4]()); // RGBA format, initialized to zero
  Image img = {
      .data = data.get(),
      .width = width,
      .height = height,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  };

  _texture = LoadTextureFromImage(img);
}

gfx::Texture::~Texture()
{
  UnloadTexture(_texture);
}

void gfx::Texture::update(void* data)
{
  UpdateTexture(_texture, data);
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

  rlImGuiImageSize(&_texture.texture(), size.x, size.y);
}

void FrameWindow::update()
{
  _texture.update(_frameBuffer->data());
}