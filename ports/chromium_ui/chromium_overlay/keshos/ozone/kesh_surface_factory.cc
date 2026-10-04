// Copyright 2026 SneakDeak Technologies / KeshOS contributors.
// Software-canvas structure is based on Chromium's public Ozone canvas API.

#include "keshos/ozone/kesh_surface_factory.h"

#include <memory>

#include "base/logging.h"
#include "keshos/ozone/kesh_framebuffer.h"
#include "keshos/ozone/kesh_window.h"
#include "keshos/ozone/kesh_window_manager.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

class KeshCanvasSurface final : public SurfaceOzoneCanvas {
 public:
  KeshCanvasSurface(gfx::AcceleratedWidget widget,
                    KeshWindowManager* window_manager,
                    KeshFramebuffer* framebuffer)
      : widget_(widget),
        window_manager_(window_manager),
        framebuffer_(framebuffer) {}

  ~KeshCanvasSurface() override = default;

  SkCanvas* GetCanvas() override {
    return surface_ ? surface_->getCanvas() : nullptr;
  }

  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override {
    if (viewport_size.IsEmpty()) {
      surface_.reset();
      return;
    }

    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    surface_ = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(viewport_size.width(),
                                   viewport_size.height()),
        &props);
  }

  void PresentCanvas(const gfx::Rect& damage) override {
    if (!surface_ || !framebuffer_ || !framebuffer_->valid())
      return;

    SkPixmap pixmap;
    if (!surface_->peekPixels(&pixmap))
      return;

    gfx::Point origin;
    if (window_manager_) {
      if (KeshWindow* window = window_manager_->GetWindow(widget_))
        origin = window->GetBoundsInPixels().origin();
    }

    framebuffer_->Blit(pixmap, damage, origin);
  }

  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    // Until KeshOS exposes page-flip/vblank events, Chromium uses its fallback
    // timing path.
    return nullptr;
  }

 private:
  const gfx::AcceleratedWidget widget_;
  KeshWindowManager* const window_manager_;
  KeshFramebuffer* const framebuffer_;
  sk_sp<SkSurface> surface_;
};

}  // namespace

KeshSurfaceFactory::KeshSurfaceFactory(KeshWindowManager* window_manager)
    : window_manager_(window_manager),
      framebuffer_(std::make_unique<KeshFramebuffer>()) {
  if (!framebuffer_->Initialize())
    LOG(ERROR) << "OzoneKesh: framebuffer initialization failed";
}

KeshSurfaceFactory::~KeshSurfaceFactory() = default;

std::unique_ptr<SurfaceOzoneCanvas>
KeshSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  return std::make_unique<KeshCanvasSurface>(
      widget, window_manager_, framebuffer_.get());
}

}  // namespace ui
