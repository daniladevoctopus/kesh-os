// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#ifndef KESHOS_OZONE_KESH_FRAMEBUFFER_H_
#define KESHOS_OZONE_KESH_FRAMEBUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "ui/gfx/geometry/rect.h"

class SkPixmap;

namespace ui {

class KeshFramebuffer {
 public:
  KeshFramebuffer();
  ~KeshFramebuffer();

  KeshFramebuffer(const KeshFramebuffer&) = delete;
  KeshFramebuffer& operator=(const KeshFramebuffer&) = delete;

  bool Initialize();
  bool valid() const { return pixels_ != nullptr; }

  int width() const { return width_; }
  int height() const { return height_; }
  int pitch() const { return pitch_; }

  // Copies a local Skia damage rectangle into the physical framebuffer at
  // |destination_origin|. Source and destination are clipped safely.
  void Blit(const SkPixmap& source,
            const gfx::Rect& damage,
            const gfx::Point& destination_origin);

 private:
  int fd_ = -1;
  uint8_t* pixels_ = nullptr;
  size_t mapped_size_ = 0;
  int width_ = 0;
  int height_ = 0;
  int pitch_ = 0;
};

}  // namespace ui

#endif  // KESHOS_OZONE_KESH_FRAMEBUFFER_H_
