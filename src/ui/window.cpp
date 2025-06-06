#include "window.h"

#include "imgui.h"

using namespace ui;

void Window::render()
{
  //ImGui::SetNextWindowSize(ImVec2(256, 256), ImGuiCond_Once);
  ImGui::Begin(_title.c_str(), &_opened);
  doRender();
  ImGui::End();
}