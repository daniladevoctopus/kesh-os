// Copyright 2026 SneakDeak Technologies / KeshOS contributors.

#include "keshos/ozone/kesh_event_source.h"

#include <algorithm>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "keshos/ozone/kesh_framebuffer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {

namespace {

constexpr base::TimeDelta kPollInterval = base::Milliseconds(4);

int MouseButtonFlag(unsigned int code) {
  switch (code) {
    case BTN_LEFT:
      return EF_LEFT_MOUSE_BUTTON;
    case BTN_MIDDLE:
      return EF_MIDDLE_MOUSE_BUTTON;
    case BTN_RIGHT:
      return EF_RIGHT_MOUSE_BUTTON;
    case BTN_BACK:
    case BTN_SIDE:
      return EF_BACK_MOUSE_BUTTON;
    case BTN_FORWARD:
    case BTN_EXTRA:
      return EF_FORWARD_MOUSE_BUTTON;
    default:
      return EF_NONE;
  }
}

}  // namespace

KeshEventSource::KeshEventSource(KeshFramebuffer* framebuffer) {
  if (framebuffer && framebuffer->valid()) {
    screen_width_ = std::max(1, framebuffer->width());
    screen_height_ = std::max(1, framebuffer->height());
  }

  cursor_x_ = screen_width_ / 2;
  cursor_y_ = screen_height_ / 2;

  // KeshOS evdev reads are already non-blocking from the kernel side and
  // return EAGAIN when a queue is empty, so O_RDONLY is sufficient.
  keyboard_fd_ = open("/dev/input/event0", O_RDONLY);
  if (keyboard_fd_ < 0)
    PLOG(ERROR) << "OzoneKesh: failed to open /dev/input/event0";

  mouse_fd_ = open("/dev/input/event1", O_RDONLY);
  if (mouse_fd_ < 0)
    PLOG(ERROR) << "OzoneKesh: failed to open /dev/input/event1";
}

KeshEventSource::~KeshEventSource() {
  poll_timer_.Stop();
  if (keyboard_fd_ >= 0)
    close(keyboard_fd_);
  if (mouse_fd_ >= 0)
    close(mouse_fd_);
}

void KeshEventSource::OnDispatcherListChanged() {
  StartPolling();
}

void KeshEventSource::StartPolling() {
  if (polling_started_)
    return;
  polling_started_ = true;
  poll_timer_.Start(FROM_HERE, kPollInterval, this, &KeshEventSource::Poll);
}

void KeshEventSource::Poll() {
  DrainKeyboard();
  DrainMouse();
}

void KeshEventSource::DrainKeyboard() {
  if (keyboard_fd_ < 0)
    return;

  input_event event = {};
  while (read(keyboard_fd_, &event, sizeof(event)) ==
         static_cast<ssize_t>(sizeof(event))) {
    if (event.type == EV_KEY)
      DispatchKey(event.code, event.value);
  }
}

void KeshEventSource::DrainMouse() {
  if (mouse_fd_ < 0)
    return;

  input_event event = {};
  while (read(mouse_fd_, &event, sizeof(event)) ==
         static_cast<ssize_t>(sizeof(event))) {
    switch (event.type) {
      case EV_REL:
        if (event.code == REL_X) {
          cursor_x_ = std::clamp(cursor_x_ + event.value, 0, screen_width_ - 1);
          mouse_moved_ = true;
        } else if (event.code == REL_Y) {
          cursor_y_ =
              std::clamp(cursor_y_ + event.value, 0, screen_height_ - 1);
          mouse_moved_ = true;
        } else if (event.code == REL_WHEEL) {
          wheel_delta_y_ += event.value * MouseWheelEvent::kWheelDelta;
        }
        break;

      case EV_KEY:
        DispatchMouseButton(event.code, event.value);
        break;

      case EV_SYN:
        if (event.code == SYN_REPORT) {
          if (mouse_moved_)
            DispatchMouseMove();
          if (wheel_delta_y_ != 0)
            DispatchMouseWheel();
        }
        break;

      default:
        break;
    }
  }
}

void KeshEventSource::DispatchKey(unsigned int code, int value) {
  if (value < 0 || value > 2)
    return;

  const DomCode dom_code = KeycodeConverter::EvdevCodeToDomCode(code);
  if (dom_code == DomCode::NONE)
    return;

  DomKey dom_key = DomKey::NONE;
  KeyboardCode key_code = DomCodeToUsLayoutKeyboardCode(dom_code);

  // Resolve the key once using the pre-event modifier state so we can identify
  // whether this key itself changes a modifier.
  DomCodeToUsLayoutDomKey(dom_code, keyboard_flags_, &dom_key, &key_code);
  const int modifier_flag = ModifierDomKeyToEventFlag(dom_key);

  const bool pressed = value != 0;
  const bool repeat = value == 2;

  if (dom_key == DomKey::CAPS_LOCK) {
    if (pressed && !repeat)
      keyboard_flags_ ^= EF_CAPS_LOCK_ON;
  } else if (modifier_flag != EF_NONE) {
    if (pressed)
      keyboard_flags_ |= modifier_flag;
    else
      keyboard_flags_ &= ~modifier_flag;
  }

  // Re-resolve printable meaning with the post-event modifier state.
  DomKey resolved_key = dom_key;
  KeyboardCode resolved_code = key_code;
  DomCodeToUsLayoutDomKey(dom_code, keyboard_flags_, &resolved_key,
                          &resolved_code);

  int flags = keyboard_flags_ | mouse_button_flags_;
  if (repeat)
    flags |= EF_IS_REPEAT;

  KeyEvent key_event(pressed ? EventType::kKeyPressed
                             : EventType::kKeyReleased,
                     resolved_code, dom_code, flags, resolved_key,
                     base::TimeTicks::Now());
  DispatchEvent(&key_event);
}

void KeshEventSource::DispatchMouseButton(unsigned int code, int value) {
  const int changed = MouseButtonFlag(code);
  if (changed == EF_NONE)
    return;

  const bool pressed = value != 0;
  if (pressed)
    mouse_button_flags_ |= changed;
  else
    mouse_button_flags_ &= ~changed;

  // Chromium keeps the changed button bit on release events too.
  const int flags = keyboard_flags_ | mouse_button_flags_ | changed;
  const gfx::Point location(cursor_x_, cursor_y_);
  MouseEvent mouse_event(pressed ? EventType::kMousePressed
                                 : EventType::kMouseReleased,
                         location, location, base::TimeTicks::Now(), flags,
                         changed);
  DispatchEvent(&mouse_event);
}

void KeshEventSource::DispatchMouseMove() {
  mouse_moved_ = false;
  const gfx::Point location(cursor_x_, cursor_y_);
  MouseEvent mouse_event(EventType::kMouseMoved, location, location,
                         base::TimeTicks::Now(),
                         keyboard_flags_ | mouse_button_flags_, 0);
  DispatchEvent(&mouse_event);
}

void KeshEventSource::DispatchMouseWheel() {
  const int delta = wheel_delta_y_;
  wheel_delta_y_ = 0;
  const gfx::Point location(cursor_x_, cursor_y_);
  MouseWheelEvent wheel_event(gfx::Vector2d(0, delta), location, location,
                              base::TimeTicks::Now(),
                              keyboard_flags_ | mouse_button_flags_, 0);
  DispatchEvent(&wheel_event);
}

}  // namespace ui
