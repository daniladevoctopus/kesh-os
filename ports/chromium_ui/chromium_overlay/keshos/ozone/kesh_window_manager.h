// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#ifndef KESHOS_OZONE_KESH_WINDOW_MANAGER_H_
#define KESHOS_OZONE_KESH_WINDOW_MANAGER_H_

#include "base/containers/id_map.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {

class KeshWindow;

class KeshWindowManager {
 public:
  KeshWindowManager();
  ~KeshWindowManager();

  KeshWindowManager(const KeshWindowManager&) = delete;
  KeshWindowManager& operator=(const KeshWindowManager&) = delete;

  gfx::AcceleratedWidget AddWindow(KeshWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget, KeshWindow* window);
  KeshWindow* GetWindow(gfx::AcceleratedWidget widget);
  gfx::AcceleratedWidget GetWidgetAtPoint(const gfx::Point& point);

 private:
  base::IDMap<KeshWindow*> windows_;
  base::ThreadChecker thread_checker_;
};

}  // namespace ui

#endif  // KESHOS_OZONE_KESH_WINDOW_MANAGER_H_
