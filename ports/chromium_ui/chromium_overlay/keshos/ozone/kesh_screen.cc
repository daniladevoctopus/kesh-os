// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#include "keshos/ozone/kesh_screen.h"

#include "base/check.h"
#include "base/time/time.h"
#include "keshos/ozone/kesh_framebuffer.h"
#include "keshos/ozone/kesh_window.h"
#include "keshos/ozone/kesh_window_manager.h"
#include "ui/display/display.h"

namespace ui {

namespace {
constexpr int64_t kKeshPrimaryDisplayId = 1;
}

KeshScreen::KeshScreen(KeshWindowManager* window_manager,
                       KeshFramebuffer* framebuffer)
    : window_manager_(window_manager), framebuffer_(framebuffer) {
  const int width = framebuffer_ && framebuffer_->valid()
                        ? framebuffer_->width()
                        : 1;
  const int height = framebuffer_ && framebuffer_->valid()
                         ? framebuffer_->height()
                         : 1;

  display::Display primary(kKeshPrimaryDisplayId,
                           gfx::Rect(0, 0, width, height));
  primary.set_work_area(primary.bounds());
  primary.set_device_scale_factor(1.0f);
  display_list_.AddDisplay(primary, display::DisplayList::Type::PRIMARY);
}

KeshScreen::~KeshScreen() = default;

const std::vector<display::Display>& KeshScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display KeshScreen::GetPrimaryDisplay() const {
  auto it = display_list_.GetPrimaryDisplayIterator();
  CHECK(it != display_list_.displays().end());
  return *it;
}

display::Display KeshScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return GetPrimaryDisplay();
}

gfx::Point KeshScreen::GetCursorScreenPoint() const {
  // M2 will update this from /dev/input/event1.
  return gfx::Point();
}

gfx::AcceleratedWidget KeshScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  return window_manager_ ? window_manager_->GetWidgetAtPoint(point)
                         : gfx::kNullAcceleratedWidget;
}

display::Display KeshScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

display::Display KeshScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

bool KeshScreen::IsScreenSaverActive() const {
  return false;
}

base::TimeDelta KeshScreen::CalculateIdleTime() const {
  return base::Seconds(0);
}

void KeshScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void KeshScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
