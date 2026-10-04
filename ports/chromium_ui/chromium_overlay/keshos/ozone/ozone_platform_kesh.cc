// Copyright 2026 SneakDeak Technologies / KeshOS contributors.
// Uses Chromium Ozone interfaces under Chromium's BSD-style license.

#include "keshos/ozone/ozone_platform_kesh.h"

#include <memory>

#include "base/no_destructor.h"
#include "keshos/ozone/kesh_event_source.h"
#include "keshos/ozone/kesh_screen.h"
#include "keshos/ozone/kesh_surface_factory.h"
#include "keshos/ozone/kesh_window.h"
#include "keshos/ozone/kesh_window_manager.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/stub_input_controller.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

class OzonePlatformKesh final : public OzonePlatform {
 public:
  OzonePlatformKesh() = default;
  ~OzonePlatformKesh() override = default;

  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactory* GetCursorFactory() override {
    return cursor_factory_.get();
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    return std::make_unique<KeshWindow>(delegate, window_manager_.get(),
                                        properties.bounds);
  }

  std::unique_ptr<display::NativeDisplayDelegate>
  CreateNativeDisplayDelegate() override {
    return nullptr;
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return std::make_unique<KeshScreen>(
        window_manager_.get(),
        surface_factory_ ? surface_factory_->framebuffer() : nullptr,
        event_source_.get());
  }

  void InitScreen(PlatformScreen* screen) override {}

  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<InputMethodMinimal>(ime_key_event_dispatcher);
  }

  bool IsWindowCompositingSupported() const override {
    return true;
  }

  const PlatformProperties& GetPlatformProperties() override {
    static base::NoDestructor<PlatformProperties> properties;
    return *properties;
  }

 private:
  bool InitializeUI(const InitParams& params) override {
    window_manager_ = std::make_unique<KeshWindowManager>();
    surface_factory_ =
        std::make_unique<KeshSurfaceFactory>(window_manager_.get());

    if (!PlatformEventSource::GetInstance()) {
      event_source_ = std::make_unique<KeshEventSource>(
          surface_factory_->framebuffer());
    }

    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = std::make_unique<StubInputController>();
    cursor_factory_ = std::make_unique<BitmapCursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    return surface_factory_->framebuffer() &&
           surface_factory_->framebuffer()->valid();
  }

  void InitializeGPU(const InitParams& params) override {
    if (!surface_factory_)
      surface_factory_ = std::make_unique<KeshSurfaceFactory>(nullptr);
  }

  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<KeshWindowManager> window_manager_;
  std::unique_ptr<KeshSurfaceFactory> surface_factory_;
  std::unique_ptr<KeshEventSource> event_source_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformKesh() {
  return new OzonePlatformKesh();
}

}  // namespace ui
