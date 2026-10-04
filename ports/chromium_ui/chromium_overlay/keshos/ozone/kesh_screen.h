// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#ifndef KESHOS_OZONE_KESH_SCREEN_H_
#define KESHOS_OZONE_KESH_SCREEN_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/display/display_list.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

class KeshFramebuffer;
class KeshWindowManager;

class KeshScreen : public PlatformScreen {
 public:
  KeshScreen(KeshWindowManager* window_manager, KeshFramebuffer* framebuffer);
  ~KeshScreen() override;

  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  bool IsScreenSaverActive() const override;
  base::TimeDelta CalculateIdleTime() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  raw_ptr<KeshWindowManager> window_manager_;
  raw_ptr<KeshFramebuffer> framebuffer_;
  display::DisplayList display_list_;
};

}  // namespace ui

#endif  // KESHOS_OZONE_KESH_SCREEN_H_
