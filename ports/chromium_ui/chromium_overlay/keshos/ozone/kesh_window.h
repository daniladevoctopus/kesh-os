// Copyright 2026 SneakDeak Technologies / KeshOS contributors.
// PlatformWindow implementation shaped after Chromium Ozone platform APIs.

#ifndef KESHOS_OZONE_KESH_WINDOW_H_
#define KESHOS_OZONE_KESH_WINDOW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class KeshWindowManager;

class KeshWindow : public PlatformWindow {
 public:
  KeshWindow(PlatformWindowDelegate* delegate,
             KeshWindowManager* manager,
             const gfx::Rect& bounds);
  ~KeshWindow() override;

  KeshWindow(const KeshWindow&) = delete;
  KeshWindow& operator=(const KeshWindow&) = delete;

  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;

  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;

  void SetTitle(const std::u16string& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;

  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;

  void Activate() override;
  void Deactivate() override;

  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;

  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;

  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  gfx::AcceleratedWidget widget() const { return widget_; }

 private:
  void ApplyBounds(const gfx::Rect& bounds);
  void SetState(PlatformWindowState state);
  void SaveRestoreBounds();
  void RestoreSavedBounds();
  void ZoomToWorkArea();

  raw_ptr<PlatformWindowDelegate> delegate_;
  raw_ptr<KeshWindowManager> manager_;
  gfx::Rect bounds_;
  std::optional<gfx::Rect> restored_bounds_;

  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  PlatformWindowState state_ = PlatformWindowState::kNormal;
  bool visible_ = false;
  bool active_ = false;
  bool capture_ = false;
};

}  // namespace ui

#endif  // KESHOS_OZONE_KESH_WINDOW_H_
