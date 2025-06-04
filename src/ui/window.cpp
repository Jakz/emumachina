#include "window.h"

#include "imgui.h"

using namespace ui;

void Window::render()
{
  ImGui::Begin(_title.c_str(), &_opened);
  doRender();
  ImGui::End();
}