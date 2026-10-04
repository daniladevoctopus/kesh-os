// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#include "keshos/ozone/kesh_framebuffer.h"

#include <algorithm>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "base/logging.h"
#include "keshos/ozone/kesh_linux_abi.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace ui {

KeshFramebuffer::KeshFramebuffer() = default;

KeshFramebuffer::~KeshFramebuffer() {
  if (pixels_)
    munmap(pixels_, mapped_size_);
  if (fd_ >= 0)
    close(fd_);
}

bool KeshFramebuffer::Initialize() {
  if (valid())
    return true;

  fd_ = open("/dev/fb0", O_RDWR);
  if (fd_ < 0) {
    PLOG(ERROR) << "OzoneKesh: open(/dev/fb0) failed";
    return false;
  }

  kesh_abi::FbVarScreenInfo variable = {};
  kesh_abi::FbFixScreenInfo fixed = {};
  if (ioctl(fd_, kesh_abi::kFbIoGetVScreenInfo, &variable) != 0 ||
      ioctl(fd_, kesh_abi::kFbIoGetFScreenInfo, &fixed) != 0) {
    PLOG(ERROR) << "OzoneKesh: framebuffer ioctl failed";
    return false;
  }

  if (variable.bits_per_pixel != 32 || variable.xres == 0 ||
      variable.yres == 0 || fixed.line_length == 0 || fixed.smem_len == 0) {
    LOG(ERROR) << "OzoneKesh: unsupported framebuffer geometry";
    return false;
  }

  width_ = static_cast<int>(variable.xres);
  height_ = static_cast<int>(variable.yres);
  pitch_ = static_cast<int>(fixed.line_length);
  mapped_size_ = fixed.smem_len;

  void* mapping =
      mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (mapping == MAP_FAILED) {
    pixels_ = nullptr;
    PLOG(ERROR) << "OzoneKesh: mmap(/dev/fb0) failed";
    return false;
  }

  pixels_ = static_cast<uint8_t*>(mapping);
  LOG(INFO) << "OzoneKesh framebuffer: " << width_ << "x" << height_
            << " pitch=" << pitch_;
  return true;
}

void KeshFramebuffer::Blit(const SkPixmap& source,
                           const gfx::Rect& damage,
                           const gfx::Point& destination_origin) {
  if (!valid() || source.colorType() != kN32_SkColorType)
    return;

  gfx::Rect source_bounds(0, 0, source.width(), source.height());
  gfx::Rect copy = damage;
  copy.Intersect(source_bounds);
  if (copy.IsEmpty())
    return;

  int src_x = copy.x();
  int src_y = copy.y();
  int dst_x = destination_origin.x() + src_x;
  int dst_y = destination_origin.y() + src_y;
  int copy_w = copy.width();
  int copy_h = copy.height();

  if (dst_x < 0) {
    const int cut = -dst_x;
    src_x += cut;
    copy_w -= cut;
    dst_x = 0;
  }
  if (dst_y < 0) {
    const int cut = -dst_y;
    src_y += cut;
    copy_h -= cut;
    dst_y = 0;
  }

  copy_w = std::min(copy_w, width_ - dst_x);
  copy_h = std::min(copy_h, height_ - dst_y);
  if (copy_w <= 0 || copy_h <= 0)
    return;

  for (int row = 0; row < copy_h; ++row) {
    const void* src = source.addr(src_x, src_y + row);
    void* dst = pixels_ + static_cast<size_t>(dst_y + row) * pitch_ +
                static_cast<size_t>(dst_x) * 4U;
    std::memcpy(dst, src, static_cast<size_t>(copy_w) * 4U);
  }
}

}  // namespace ui
