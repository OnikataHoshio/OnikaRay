#include "KH_Panel.h"
#include "KH_Editor.h"
#include "Utils/KH_DebugUtils.h"

KH_Canvas::KH_Canvas()
    :Timer(3.0f)
{
    Framebuffer.Create(64, 64);
}

void KH_Canvas::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("Canvas");
    {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        uint32_t viewportWidth = static_cast<uint32_t>(viewportPanelSize.x);
        uint32_t viewportHeight = static_cast<uint32_t>(viewportPanelSize.y);


        if (viewportWidth != Framebuffer.GetWidth() || viewportHeight != Framebuffer.GetHeight())
        {
            if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
                Framebuffer.Rescale(viewportWidth, viewportHeight);
                KH_Editor::Instance().UpdateCanvasExtent(viewportWidth, viewportHeight);
                glViewport(0, 0, KH_Editor::CanvasWidth, KH_Editor::CanvasHeight);

                Timer.Reset();
            }
        }

        Timer.Tick(ImGui::GetIO().DeltaTime);

        if (Timer.HasFinished())
        {
            std::string DebugMessage = std::format("Canvas size has been changed to [{},{}]", viewportWidth, viewportHeight);
            LOG_D(DebugMessage);
            Timer.InActive();
        }

        uint32_t textureID = Framebuffer.GetTextureID();
        ImGui::Image((void*)(intptr_t)textureID, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));

        bIsFocused = ImGui::IsWindowFocused();
        bIsHovered = ImGui::IsWindowHovered();
    }
    ImGui::End();

    ImGui::PopStyleVar();
}


std::vector<KH_LOG_MESSAGE> KH_Console::LogMessages;

void KH_Console::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Console");

    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(LogMessages.size()));

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto& Log = LogMessages[i];
                ImVec4 logColor;
                bool useSpecificColor = true;

                switch (Log.Flag) {
                case KH_LOG_FLAG::Temp:
                    logColor = ImVec4(0.5f, 0.8f, 1.0f, 1.0f); // 浅天蓝色
                    break;
                case KH_LOG_FLAG::Debug:
                    logColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // 翠绿色
                    break;
                case KH_LOG_FLAG::Warning:
                    logColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // 亮黄色/橙色
                    break;
                case KH_LOG_FLAG::Error:
                    logColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // 亮红色
                    break;
                default:
                    useSpecificColor = false;
                    break;
                }

                if (useSpecificColor) {
                    ImGui::TextColored(logColor, "%s", Log.Message.c_str());
                }
                else {
                    ImGui::TextUnformatted(Log.Message.c_str());
                }
                ImGui::Separator();
            }
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();
}

void KH_Setting::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Setting");

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();

}

void KH_GlobalInfo::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("GlobalInfo");

    {
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "(%.2f ms)", 1000.0f / ImGui::GetIO().Framerate);

            static bool vsync = true;
            if (ImGui::Checkbox("V-Sync", &vsync)) {
                glfwSwapInterval(vsync ? 1 : 0);
            }
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::BulletText("GPU: %s", glGetString(GL_RENDERER));
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::Text("Canvas: %dx%d", KH_Editor::CanvasWidth, KH_Editor::CanvasHeight);
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::Text("Speed: %.2f", KH_Editor::Instance().Camera.MovementSpeed);
            ImGui::Text("Fovy : %.2f", KH_Editor::Instance().Camera.Fovy);
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

    }

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();
}

