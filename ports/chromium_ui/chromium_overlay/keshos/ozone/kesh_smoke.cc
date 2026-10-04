// Copyright 2026 SneakDeak Technologies / KeshOS contributors.
//
// Smallest useful Chromium-native UI executable for KeshOS bring-up.
// It deliberately avoids Chrome, Blink, V8, Mojo initialization, GL and the
// full upstream ozone_demo renderer stack. The goal is to prove this path:
//
//   Chromium base + Skia -> OzoneKesh -> KeshOS framebuffer

#include <algorithm>
#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {
namespace {

class KeshSmokeWindow final : public PlatformWindowDelegate {
 public:
  KeshSmokeWindow() {
    PlatformWindowInitProperties properties;
    properties.bounds = gfx::Rect(0, 0, 900, 560);
    window_ = OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, std::move(properties));
  }

  KeshSmokeWindow(const KeshSmokeWindow&) = delete;
  KeshSmokeWindow& operator=(const KeshSmokeWindow&) = delete;
  ~KeshSmokeWindow() override = default;

  bool Start() {
    if (!window_ || widget_ == gfx::kNullAcceleratedWidget)
      return false;

    window_->Show(false);
    size_ = window_->GetBoundsInPixels().size();
    surface_ = OzonePlatform::GetInstance()
                   ->GetSurfaceFactoryOzone()
                   ->CreateCanvasForWidget(widget_);
    if (!surface_ || size_.IsEmpty())
      return false;

    surface_->ResizeCanvas(size_, 1.0f);
    Paint();
    timer_.Start(FROM_HERE, base::Milliseconds(16), this,
                 &KeshSmokeWindow::Tick);
    return true;
  }

  // PlatformWindowDelegate implementation.
  void OnBoundsChanged(const BoundsChange& change) override {
    if (!window_)
      return;
    size_ = window_->GetBoundsInPixels().size();
    if (surface_ && !size_.IsEmpty())
      surface_->ResizeCanvas(size_, 1.0f);
  }

  void OnDamageRect(const gfx::Rect& damaged_region) override {}

  void DispatchEvent(Event* event) override {
    // The full Aura event routing arrives in the next milestone. Keeping this
    // handler makes the smoke window a valid PlatformWindowDelegate today.
  }

  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override {}
  void OnLostCapture() override {}

  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }

  void OnWillDestroyAcceleratedWidget() override {}

  void OnAcceleratedWidgetDestroyed() override {
    widget_ = gfx::kNullAcceleratedWidget;
  }

  void OnActivationChanged(bool active) override {}
  void OnCursorUpdate() override {}

 private:
  void Tick() {
    phase_ = (phase_ + 4) % std::max(1, size_.width());
    Paint();
  }

  void Paint() {
    if (!surface_ || size_.IsEmpty())
      return;

    SkCanvas* canvas = surface_->GetCanvas();
    if (!canvas)
      return;

    // Dark neutral background: visually obvious, cheap to render and close to
    // the target KeshOS/Ash shell palette without depending on fonts yet.
    canvas->clear(SkColorSetRGB(18, 20, 24));

    SkPaint paint;
    paint.setAntiAlias(false);

    // Top shell strip.
    paint.setColor(SkColorSetRGB(35, 39, 46));
    canvas->drawRect(SkRect::MakeXYWH(0, 0, size_.width(), 64), paint);

    // Left launcher rail.
    paint.setColor(SkColorSetRGB(28, 31, 37));
    canvas->drawRect(SkRect::MakeXYWH(20, 92, 72, size_.height() - 120), paint);

    // Three content cards. These are intentionally pure Skia primitives so a
    // successful frame proves Chromium's software canvas reached KeshOS.
    const int card_left = 126;
    const int card_top = 104;
    const int card_gap = 22;
    const int card_width = std::max(100, (size_.width() - card_left - 52 -
                                          card_gap * 2) /
                                         3);
    const int card_height = std::max(100, size_.height() - card_top - 92);

    const SkColor cards[3] = {
        SkColorSetRGB(45, 50, 59),
        SkColorSetRGB(39, 44, 52),
        SkColorSetRGB(48, 53, 62),
    };
    for (int i = 0; i < 3; ++i) {
      paint.setColor(cards[i]);
      canvas->drawRect(
          SkRect::MakeXYWH(card_left + i * (card_width + card_gap), card_top,
                           card_width, card_height),
          paint);
    }

    // Kesh orange accent + moving green heartbeat. The moving bar makes it
    // immediately clear that base::MessagePump/RepeatingTimer is alive too.
    paint.setColor(SkColorSetRGB(255, 159, 10));
    canvas->drawRect(SkRect::MakeXYWH(26, 20, 142, 24), paint);

    paint.setColor(SkColorSetRGB(48, 209, 88));
    const int heartbeat_width = 120;
    const int usable = std::max(1, size_.width() - heartbeat_width);
    const int heartbeat_x = phase_ % usable;
    canvas->drawRect(
        SkRect::MakeXYWH(heartbeat_x, size_.height() - 18, heartbeat_width, 6),
        paint);

    surface_->PresentCanvas(gfx::Rect(size_));
  }

  std::unique_ptr<PlatformWindow> window_;
  std::unique_ptr<SurfaceOzoneCanvas> surface_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  gfx::Size size_;
  int phase_ = 0;
  base::RepeatingTimer timer_;
};

}  // namespace
}  // namespace ui

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager at_exit;
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::UI);

  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  if (!ui::OzonePlatform::InitializeForUI(params))
    return 20;
  ui::OzonePlatform::InitializeForGPU(params);

  ui::KeshSmokeWindow window;
  if (!window.Start())
    return 21;

  base::RunLoop run_loop;
  run_loop.Run();
  return 0;
}
