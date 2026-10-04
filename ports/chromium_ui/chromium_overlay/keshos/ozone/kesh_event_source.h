// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#ifndef KESHOS_OZONE_KESH_EVENT_SOURCE_H_
#define KESHOS_OZONE_KESH_EVENT_SOURCE_H_

#include "base/timer/timer.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

class KeshFramebuffer;

// Minimal native KeshOS input bridge.
//
// KeshOS already exposes Linux-compatible input_event records through
// /dev/input/event0 (keyboard) and /dev/input/event1 (mouse). This class reads
// those KeshOS device nodes and translates them into Chromium ui::Event
// objects. It does not use a Linux kernel.
class KeshEventSource final : public PlatformEventSource {
 public:
  explicit KeshEventSource(KeshFramebuffer* framebuffer);
  ~KeshEventSource() override;

  KeshEventSource(const KeshEventSource&) = delete;
  KeshEventSource& operator=(const KeshEventSource&) = delete;

  gfx::Point cursor_position() const {
    return gfx::Point(cursor_x_, cursor_y_);
  }

 protected:
  void OnDispatcherListChanged() override;

 private:
  void StartPolling();
  void Poll();
  void DrainKeyboard();
  void DrainMouse();

  void DispatchKey(unsigned int code, int value);
  void DispatchMouseButton(unsigned int code, int value);
  void DispatchMouseMove();
  void DispatchMouseWheel();

  int keyboard_fd_ = -1;
  int mouse_fd_ = -1;

  int screen_width_ = 1;
  int screen_height_ = 1;
  int cursor_x_ = 0;
  int cursor_y_ = 0;

  int keyboard_flags_ = 0;
  int mouse_button_flags_ = 0;
  int wheel_delta_y_ = 0;
  bool mouse_moved_ = false;
  bool polling_started_ = false;

  base::RepeatingTimer poll_timer_;
};

}  // namespace ui

#endif  // KESHOS_OZONE_KESH_EVENT_SOURCE_H_
