// Copyright 2026 SneakDeak Technologies / KeshOS contributors.
//
// Tiny userspace ABI definitions shared with KeshOS kernel compatibility
// devices.  OzoneKesh intentionally does not include Linux kernel UAPI
// headers: KeshOS is the platform and these layouts are part of our own
// compatibility contract.

#ifndef KESHOS_OZONE_KESH_LINUX_ABI_H_
#define KESHOS_OZONE_KESH_LINUX_ABI_H_

#include <stdint.h>

namespace ui::kesh_abi {

constexpr unsigned long kFbIoGetVScreenInfo = 0x4600UL;
constexpr unsigned long kFbIoGetFScreenInfo = 0x4602UL;

struct FbBitfield {
  uint32_t offset;
  uint32_t length;
  uint32_t msb_right;
};

struct FbVarScreenInfo {
  uint32_t xres;
  uint32_t yres;
  uint32_t xres_virtual;
  uint32_t yres_virtual;
  uint32_t xoffset;
  uint32_t yoffset;
  uint32_t bits_per_pixel;
  uint32_t grayscale;
  FbBitfield red;
  FbBitfield green;
  FbBitfield blue;
  FbBitfield transp;
  uint32_t nonstd;
  uint32_t activate;
  uint32_t height;
  uint32_t width;
  uint32_t accel_flags;
  uint32_t pixclock;
  uint32_t left_margin;
  uint32_t right_margin;
  uint32_t upper_margin;
  uint32_t lower_margin;
  uint32_t hsync_len;
  uint32_t vsync_len;
  uint32_t sync;
  uint32_t vmode;
  uint32_t rotate;
  uint32_t colorspace;
  uint32_t reserved[4];
};

struct FbFixScreenInfo {
  char id[16];
  uint64_t smem_start;
  uint32_t smem_len;
  uint32_t type;
  uint32_t type_aux;
  uint32_t visual;
  uint16_t xpanstep;
  uint16_t ypanstep;
  uint16_t ywrapstep;
  uint32_t line_length;
  uint64_t mmio_start;
  uint32_t mmio_len;
  uint32_t accel;
  uint16_t capabilities;
  uint16_t reserved[2];
};

struct Timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

struct InputEvent {
  Timeval time;
  uint16_t type;
  uint16_t code;
  int32_t value;
};

constexpr uint16_t kEvSyn = 0x00;
constexpr uint16_t kEvKey = 0x01;
constexpr uint16_t kEvRel = 0x02;
constexpr uint16_t kSynReport = 0;

constexpr uint16_t kRelX = 0x00;
constexpr uint16_t kRelY = 0x01;
constexpr uint16_t kRelWheel = 0x08;

constexpr uint16_t kBtnLeft = 0x110;
constexpr uint16_t kBtnRight = 0x111;
constexpr uint16_t kBtnMiddle = 0x112;
constexpr uint16_t kBtnSide = 0x113;
constexpr uint16_t kBtnExtra = 0x114;
constexpr uint16_t kBtnForward = 0x115;
constexpr uint16_t kBtnBack = 0x116;

}  // namespace ui::kesh_abi

#endif  // KESHOS_OZONE_KESH_LINUX_ABI_H_
