// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#include "keshos/ozone/kesh_window_manager.h"

#include "base/check.h"
#include "keshos/ozone/kesh_window.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

KeshWindowManager::KeshWindowManager() = default;

KeshWindowManager::~KeshWindowManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

gfx::AcceleratedWidget KeshWindowManager::AddWindow(KeshWindow* window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return windows_.Add(window);
}

void KeshWindowManager::RemoveWindow(gfx::AcceleratedWidget widget,
                                     KeshWindow* window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(window, windows_.Lookup(widget));
  windows_.Remove(widget);
}

KeshWindow* KeshWindowManager::GetWindow(gfx::AcceleratedWidget widget) {
  return windows_.Lookup(widget);
}

gfx::AcceleratedWidget KeshWindowManager::GetWidgetAtPoint(
    const gfx::Point& point) {
  for (base::IDMap<KeshWindow*>::const_iterator it(&windows_);
       !it.IsAtEnd(); it.Advance()) {
    KeshWindow* window = it.GetCurrentValue();
    if (window->IsVisible() && window->GetBoundsInPixels().Contains(point))
      return window->widget();
  }
  return gfx::kNullAcceleratedWidget;
}

}  // namespace ui
