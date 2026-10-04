// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#include "keshos/ozone/kesh_window.h"

#include "base/check.h"
#include "keshos/ozone/kesh_window_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ui {

KeshWindow::KeshWindow(PlatformWindowDelegate* delegate,
                       KeshWindowManager* manager,
                       const gfx::Rect& bounds)
    : delegate_(delegate), manager_(manager), bounds_(bounds) {
  CHECK(delegate_);
  CHECK(manager_);
  widget_ = manager_->AddWindow(this);
  delegate_->OnAcceleratedWidgetAvailable(widget_);
}

KeshWindow::~KeshWindow() {
  if (manager_)
    manager_->RemoveWindow(widget_, this);
}

void KeshWindow::Show(bool inactive) {
  visible_ = true;
  if (!inactive)
    Activate();
}

void KeshWindow::Hide() {
  visible_ = false;
}

void KeshWindow::Close() {
  delegate_->OnClosed();
}

bool KeshWindow::IsVisible() const {
  return visible_;
}

void KeshWindow::PrepareForShutdown() {}

void KeshWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  ApplyBounds(bounds);
}

gfx::Rect KeshWindow::GetBoundsInPixels() const {
  return bounds_;
}

void KeshWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  ApplyBounds(delegate_->ConvertRectToPixels(bounds));
}

gfx::Rect KeshWindow::GetBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(bounds_);
}

void KeshWindow::SetTitle(const std::u16string& title) {
  // Ash/Views owns decoration for the first KeshOS milestone.
}

void KeshWindow::SetCapture() {
  capture_ = true;
}

void KeshWindow::ReleaseCapture() {
  if (capture_) {
    capture_ = false;
    delegate_->OnLostCapture();
  }
}

bool KeshWindow::HasCapture() const {
  return capture_;
}

void KeshWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  if (!delegate_->CanFullscreen())
    return;

  if (fullscreen) {
    if (state_ != PlatformWindowState::kFullScreen)
      SaveRestoreBounds();
    ZoomToWorkArea();
    SetState(PlatformWindowState::kFullScreen);
  } else if (state_ == PlatformWindowState::kFullScreen) {
    RestoreSavedBounds();
    SetState(PlatformWindowState::kNormal);
  }
}

void KeshWindow::Maximize() {
  if (!delegate_->CanMaximize())
    return;
  if (state_ != PlatformWindowState::kMaximized &&
      state_ != PlatformWindowState::kFullScreen) {
    SaveRestoreBounds();
    ZoomToWorkArea();
    SetState(PlatformWindowState::kMaximized);
  }
}

void KeshWindow::Minimize() {
  if (state_ != PlatformWindowState::kMinimized)
    SetState(PlatformWindowState::kMinimized);
  Hide();
}

void KeshWindow::Restore() {
  RestoreSavedBounds();
  SetState(PlatformWindowState::kNormal);
  Show(false);
}

PlatformWindowState KeshWindow::GetPlatformWindowState() const {
  return state_;
}

void KeshWindow::Activate() {
  if (!active_) {
    active_ = true;
    delegate_->OnActivationChanged(true);
  }
}

void KeshWindow::Deactivate() {
  if (active_) {
    active_ = false;
    delegate_->OnActivationChanged(false);
  }
}

void KeshWindow::SetUseNativeFrame(bool use_native_frame) {}

bool KeshWindow::ShouldUseNativeFrame() const {
  return false;
}

void KeshWindow::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  // Cursor pixels will be connected to the KeshOS cursor service in M2.
}

void KeshWindow::MoveCursorTo(const gfx::Point& location) {
  // Cursor warping is not exposed by KeshOS yet.
}

void KeshWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  // Pointer confinement is intentionally deferred.
}

void KeshWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  restored_bounds_ = delegate_->ConvertRectToPixels(bounds);
}

gfx::Rect KeshWindow::GetRestoredBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(restored_bounds_.value_or(bounds_));
}

void KeshWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                const gfx::ImageSkia& app_icon) {}

void KeshWindow::SizeConstraintsChanged() {}

void KeshWindow::ApplyBounds(const gfx::Rect& bounds) {
  const bool origin_changed = bounds_.origin() != bounds.origin();
  bounds_ = bounds;
  delegate_->OnBoundsChanged({origin_changed});
}

void KeshWindow::SetState(PlatformWindowState state) {
  if (state_ == state)
    return;
  const PlatformWindowState old = state_;
  state_ = state;
  delegate_->OnWindowStateChanged(old, state_);
}

void KeshWindow::SaveRestoreBounds() {
  if (!restored_bounds_)
    restored_bounds_ = bounds_;
}

void KeshWindow::RestoreSavedBounds() {
  if (!restored_bounds_)
    return;
  gfx::Rect saved = *restored_bounds_;
  restored_bounds_.reset();
  ApplyBounds(saved);
}

void KeshWindow::ZoomToWorkArea() {
  if (!display::Screen::Get())
    return;
  ApplyBounds(delegate_->ConvertRectToPixels(
      display::Screen::Get()->GetDisplayMatching(bounds_).work_area()));
}

}  // namespace ui
