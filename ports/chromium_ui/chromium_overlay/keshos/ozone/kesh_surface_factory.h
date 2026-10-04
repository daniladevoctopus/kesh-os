// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#ifndef KESHOS_OZONE_KESH_SURFACE_FACTORY_H_
#define KESHOS_OZONE_KESH_SURFACE_FACTORY_H_

#include <memory>

#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class KeshFramebuffer;
class KeshWindowManager;

class KeshSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit KeshSurfaceFactory(KeshWindowManager* window_manager);
  ~KeshSurfaceFactory() override;

  KeshSurfaceFactory(const KeshSurfaceFactory&) = delete;
  KeshSurfaceFactory& operator=(const KeshSurfaceFactory&) = delete;

  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;

  KeshFramebuffer* framebuffer() const { return framebuffer_.get(); }

 private:
  KeshWindowManager* const window_manager_;
  std::unique_ptr<KeshFramebuffer> framebuffer_;
};

}  // namespace ui

#endif  // KESHOS_OZONE_KESH_SURFACE_FACTORY_H_
