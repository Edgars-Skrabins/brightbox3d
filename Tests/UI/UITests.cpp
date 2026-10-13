#include "Level/Level.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/ShaderLibrary.h"
#include "UI/Document.h"
#include "UI/ScreenRenderer.h"
#include "UI/UITestTypes.h"

#include <gtest/gtest.h>

#include <stb_image_write.h>

#include <cstdlib>
#include <filesystem>

using namespace BrightBox3D;
using namespace BrightBox3D::UI;

namespace
{
    const char* HudMarkup = R"(<Screen>
    <Style>
        .hud { padding: 24px; gap: 10px; flex-direction: column; align-items: flex-start; }
        .card { background-color: #202428cc; border-radius: 8px; padding: 12px; gap: 8px; width: 320px; border: 2px #4a8be0; }
        .title { font-size: 28px; color: #ffd166; }
        #leave { background-color: #8a2b2b; }
        Label.small { font-size: 14px; color: #c0c0c0; }
    </Style>
    <Panel class="hud">
        <Panel class="card">
            <Label class="title" text="Hello {Player.m_playerName}" />
            <Label name="health" text="Health {Player.m_health} / {Player.m_maxHealth:0}" />
            <ProgressBar name="bar" value="{Player.m_health}" max="{Player.m_maxHealth}" style="width: 100%;" />
            <Label class="small" text="Volume" />
            <Slider name="volume" value="{Player.m_volume}" min="0" max="1" style="width: 100%;" />
            <Button name="leave" text="Leave Game" onClick="{Player.m_leaveRequested}" />
            <Label name="bad" text="{Player.m_notUI}" />
        </Panel>
    </Panel>
</Screen>)";

    std::filesystem::path OutputDirectory()
    {
        const char* directory = std::getenv("BRIGHT_TEST_OUTPUT");
        return directory ? std::filesystem::path(directory) : std::filesystem::temp_directory_path();
    }
}

TEST(UITests, StyleSheetParsesSelectorsAndSpecificity)
{
    StyleSheet sheet;
    std::string error;
    ASSERT_TRUE(sheet.Parse("Button, .primary:hover { color: #ff0000; } /* c */ .a .b { width: 50%; }", error));
    ASSERT_EQ(sheet.GetRules().size(), 2u);
    EXPECT_EQ(sheet.GetRules()[0].selectors.size(), 2u);
    EXPECT_EQ(sheet.GetRules()[0].selectors[1].specificity, 20);
    EXPECT_EQ(sheet.GetRules()[1].selectors[0].parts.size(), 2u);
    Math::Color color;
    ASSERT_TRUE(ParseColor("#4a8be0cc", color));
    EXPECT_NEAR(color.a, 0.8f, 0.01f);
    EXPECT_EQ(ParseLength("50%").unit, LengthUnit::Percent);
}

TEST(UITests, BindingsLayoutInputAndRendering)
{
    Level::Level level;
    auto* player = static_cast<Tests::UITestPlayer*>(level.CreateObject(Tests::UITestPlayer::StaticType()));
    Document document;
    std::string error;
    ASSERT_TRUE(document.LoadFromText(HudMarkup, error)) << error;
    BindingRoots roots;
    roots.level = &level;
    roots.player = player;

    UIInput input;
    document.Update(roots, input, 640.0f, 360.0f, 0.016f);
    Element* health = document.Find("health");
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->GetText(), "Health 75 / 100");
    EXPECT_FLOAT_EQ(document.Find("bar")->GetValue(), 75.0f);
    EXPECT_FLOAT_EQ(document.Find("bar")->GetMaximum(), 100.0f);
    EXPECT_EQ(document.Find("bad")->GetText(), "");
    EXPECT_GT(health->GetWidth(), 50.0f);
    EXPECT_NEAR(document.Find("leave")->GetY() > health->GetY() ? 1.0f : 0.0f, 1.0f, 0.0f);

    player->m_health = 30;
    document.Update(roots, input, 640.0f, 360.0f, 0.016f);
    EXPECT_EQ(health->GetText(), "Health 30 / 100");

    Element* leave = document.Find("leave");
    float cx = leave->GetX() + leave->GetWidth() * 0.5f;
    float cy = leave->GetY() + leave->GetHeight() * 0.5f;
    input.mouse = { cx, cy };
    input.mousePressed = true;
    input.mouseDown = true;
    document.Update(roots, input, 640.0f, 360.0f, 0.016f);
    EXPECT_TRUE(leave->IsPressed());
    input.mousePressed = false;
    input.mouseDown = false;
    input.mouseReleased = true;
    document.Update(roots, input, 640.0f, 360.0f, 0.016f);
    EXPECT_TRUE(player->m_leaveRequested);

    Element* slider = document.Find("volume");
    input = UIInput();
    input.mouse = { slider->GetX() + slider->GetWidth() * 0.75f, slider->GetY() + slider->GetHeight() * 0.5f };
    input.mousePressed = true;
    input.mouseDown = true;
    document.Update(roots, input, 640.0f, 360.0f, 0.016f);
    input.mousePressed = false;
    document.Update(roots, input, 640.0f, 360.0f, 0.016f);
    EXPECT_NEAR(player->m_volume, 0.75f, 0.02f);

    std::unique_ptr<RHI::Device> device = RHI::Device::Create();
    ASSERT_NE(device, nullptr);
    Rendering::ShaderLibrary shaders(*device, BRIGHT_ENGINE_CONTENT_DIR "/Shaders");
    Rendering::GpuResourceCache resources(*device);
    ScreenRenderer::Get().Initialize(*device, shaders, resources);
    RHI::TextureDesc desc;
    desc.width = 640;
    desc.height = 360;
    desc.format = RHI::Format::RGBA8Unorm;
    desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
    auto target = device->CreateTexture(desc);
    RHI::CommandList& commands = device->BeginFrame();
    RHI::RenderPassDesc clear;
    clear.colors.push_back({ target.get(), 0, 0, RHI::LoadAction::Clear, { 0.18f, 0.32f, 0.45f, 1.0f } });
    commands.BeginRenderPass(clear);
    commands.EndRenderPass();
    ScreenRenderer::Get().RenderDocument(commands, document, target.get(), 1.0f);
    device->EndFrame({});
    std::vector<uint8_t> pixels;
    ASSERT_TRUE(device->ReadTexture(target.get(), pixels));
    ScreenRenderer::Get().Shutdown();
    std::filesystem::path path = OutputDirectory() / "BrightBox3D_UITest.png";
    stbi_write_png(path.string().c_str(), 640, 360, 4, pixels.data(), 640 * 4);
    size_t index = (static_cast<size_t>(cy) * 640 + static_cast<size_t>(cx - leave->GetWidth() * 0.45f)) * 4;
    EXPECT_GT(pixels[index], pixels[index + 2]);
}
