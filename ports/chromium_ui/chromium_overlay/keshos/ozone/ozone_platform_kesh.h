// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#ifndef KESHOS_OZONE_OZONE_PLATFORM_KESH_H_
#define KESHOS_OZONE_OZONE_PLATFORM_KESH_H_

#include "ui/ozone/public/ozone_platform.h"

namespace ui {

// Constructor hook consumed by Chromium's generated Ozone constructor list.
OzonePlatform* CreateOzonePlatformKesh();

}  // namespace ui

#endif  // KESHOS_OZONE_OZONE_PLATFORM_KESH_H_
