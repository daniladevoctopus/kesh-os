# 📋 Руководство по передаче проекта: Портирование Wayland и KDE Plasma на KeshOS

> **Для следующего агента / разработчика:**  
> Этот документ содержит полную информацию о текущем состоянии портирования графического стека Linux, Wayland и KDE Plasma на независимое 64-битное ядро KeshOS (Ring 0 / Ring 3).  
> **Фазы 0, 1, 2 и 3 ПОЛНОСТЬЮ ЗАВЕРШЕНЫ, СОБРАНЫ И ВЕРИФИЦИРОВАНЫ.**  
> **Фаза 4 в работе:** QtBase, Qt Widgets, Qt Declarative, QML, Qt Quick и Quick Controls 6.11.2 статически собраны для musl; готовы QWidget- и QML-демо. Нужна runtime-проверка в KeshOS.

---

## 🏛️ 1. Архитектурная картина мира: как устроена система

KeshOS — это независимая операционная система для архитектуры `x86_64` (Long Mode).
В ядре **нет GUI-кода Плазмы**. Ядро KeshOS даёт пользовательскому пространству (Ring 3) Linux-совместимые ручки, поверх которых работает официальный графический стек Linux:

```
+-------------------------------------------------------------------------+
|                        KDE Plasma (plasmashell)                         |
|   (Нижняя панель, меню Kickoff, системный трей, обои, виджеты Plasma)   |  <- Фаза 6
+-------------------------------------------------------------------------+
|                       KDE Frameworks 6 (KF6) & D-Bus                    |  <- Фаза 5
+-------------------------------------------------------------------------+
|                      Qt 6 (QtCore, QtGui, QtQuick/QML)                  |
|          QPA-бэкенды: Wayland Client (qwayland) или LinuxFB (qlinuxfb)  |  <- ФАЗА 4 (ТЕКУЩАЯ)
+-------------------------------------------------------------------------+
|                  Графические библиотеки и шрифтовой стек                |
|           pixman-1 (2D), libxkbcommon (XKB), freetype2 (TTF), zlib      |  <- Фаза 3 [ГОТОВО]
+-------------------------------------------------------------------------+
|                   Wayland Compositor & Wayland Libraries                |
|             libwayland-server, libwayland-client, libffi                |  <- Фаза 2 [ГОТОВО]
+-------------------------------------------------------------------------+
|                   Межпроцессное взаимодействие (IPC)                    |
|       AF_UNIX sockets, SCM_RIGHTS (fd passing), memfd_create, epoll     |  <- Фаза 1 [ГОТОВО]
+-------------------------------------------------------------------------+
|                     Стандартная библиотека C (Libc)                     |
|           musl libc 1.2.5 (SysV AMD64 ABI, TLS IA32_FS_BASE MSR)        |  <- Фаза 0 [ГОТОВО]
+=========================================================================+
|                           ЯДРО KeshOS (Ring 0)                          |
|  - Framebuffer & DRM: /dev/fb0, /dev/dri/card0 (zero-copy mmap VRAM)    |
|  - Ввод (evdev): /dev/input/event0 (клавиатура), /dev/input/event1 (мышь)|
|  - Системные вызовы: быстрый `syscall` (MSR_LSTAR 0xC0000082)           |
|  - Управление памятью: VMM/PMM, 4-уровневые таблицы PML4, mmap, brk     |
+-------------------------------------------------------------------------+
```

---

## ✅ 2. Что уже полностью реализовано и работает (Фазы 0 — 3)

### 🔹 Фаза 0: POSIX-совместимость и `musl libc 1.2.5`
- **Файлы ядра:** `kernel/linux_syscall.c`, `kernel/linux_syscall.h`, `kernel/isr_stubs.S`, `kernel/syscall.c`, `kernel/process.c`.
- **Что сделано:**
  - Реализован быстрый вход через ассемблерный стаб `syscall_entry_stub` с сохранением всех 6 регистров System V AMD64 (`%rdi, %rsi, %rdx, %r10, %r8, %r9`).
  - Поддержка `SYS_arch_prctl` (`ARCH_SET_FS` = `0x1002`): запись в MSR `0xC0000100` (`IA32_FS_BASE`) для поточной локальной памяти (TLS).
  - Переключение `FS_BASE` при смене потоков и процессов в планировщике (`process_switch`).
  - Корректная раскладка стека ELF: `argc=1`, `argv[0]`, `NULL`, `envp=NULL`, `auxv` (`AT_PAGESZ=4096`, `AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_ENTRY`, `AT_RANDOM`, `AT_EXECFN`, `AT_NULL`).
  - Полноценный тулчейн `toolchain/musl/`: статическая библиотека `libc.a` (2.4 МБ), `crt1.o`, `crti.o`, `crtn.o` и полные заголовки `toolchain/musl/include/`.
  - Верификация: бинарник `ready/boot/apps/hello.elf` (50 КБ).

### 🔹 Фаза 1: Межпроцессное взаимодействие (IPC) для Wayland
- **Файлы ядра:** `kernel/unix_ipc.c`, `kernel/unix_ipc.h`, `kernel/fd.c`, `kernel/fd.h`.
- **Что сделано:**
  - **`AF_UNIX` потоковые сокеты:** `socket(AF_UNIX, SOCK_STREAM, 0)`, `bind`, `listen`, `connect`, `accept`, `socketpair`, `sendmsg`, `recvmsg`. Поддержка именованных путей (`/tmp/wayland-0`).
  - **`SCM_RIGHTS` (дескрипторный мост):** передача дескрипторов разделяемой памяти между клиентом и композитором через вспомогательные данные (`struct cmsghdr`).
  - **`memfd_create` (анонимная память):** создание дескрипторов в ОЗУ с `ftruncate` и отображением через `mmap` в виртуальное адресное пространство нескольких процессов одновременно.
  - **`epoll`:** `epoll_create1`, `epoll_ctl` (EPOLL_CTL_ADD, MOD, DEL), `epoll_wait` для организации неблокирующего цикла событий (Event Loop).
  - Верификация: бинарник `ready/boot/apps/test_ipc.elf` (46 КБ).

### 🔹 Фаза 2: Сборка библиотек Wayland и Композитор KeshOS
- **Файлы:** `ports/wayland/`, `ports/libffi/`, `ports/wayland_demo.c`.
- **Что сделано:**
  - Собрана статическая библиотека `toolchain/musl/lib/libffi.a` (3.8.0, 99 КБ).
  - Хостовым `wayland-scanner 1.26.0` сгенерированы заголовки и протоколы ядра Wayland: `wayland-server-protocol.h`, `wayland-client-protocol.h`, `wayland-protocol.c`.
  - Собраны статические библиотеки `libwayland-server.a` (158 КБ) и `libwayland-client.a` (118 КБ) (версия 1.26.90).
  - Заголовки установлены в `toolchain/musl/include/wayland/`.
  - В ядре добавлены системные вызовы: `open`, `openat`, `stat`, `fstat` (возврат точного размера `memfd`), `fcntl` (`F_GETFL`, `F_SETFL`, `F_DUPFD`), `poll` и расширения GUI (500–503).
  - Верификация: бинарник `ready/boot/apps/wayland_demo.elf` (230 КБ), команда `wayland` / `wl` в консоли KeshOS.

### 🔹 Фаза 3: Графика, Ввод и Векторные Шрифты (FreeType 2, Pixman, xkbcommon, evdev, DRM)
- **Библиотеки в тулчейне `toolchain/musl/lib/`:**
  - `libpixman-1.a` (1.2 МБ) — 2D-растеризация, альфа-композитинг Портера-Даффа, SIMD (SSE2/SSSE3/MMX).
  - `libxkbcommon.a` (651 КБ) — парсер и компилятор раскладок клавиатуры XKB.
  - `libz.a` (138 КБ) — сжатие/распаковка deflate/inflate.
  - `libfreetype.a` (1.15 МБ) — векторный рендерер глифов TrueType, CFF, субпиксельное сглаживание, SDF.
- **Подсистемы ядра KeshOS (Ring 0):**
  - **`kernel/evdev.c`, `kernel/evdev.h`:** кольцевые буферы ввода `/dev/input/event0` (клавиатура) и `/dev/input/event1` (мышь), трансляция сканкодов в коды клавиш Linux `KEY_*` и мышиных дельт `REL_X`/`REL_Y`, неблокирующее чтение, поддержка `poll()`.
  - **`kernel/drm_fb.c`, `kernel/drm_fb.h`:** драйверы `/dev/fb0` и `/dev/dri/card0` с ioctl `FBIOGET_VSCREENINFO`, `FBIOGET_FSCREENINFO` и **zero-copy `mmap` физической видеопамяти VRAM** через `process_vm_map_phys`.
  - **`kernel/elf.c`:** поддержка сегментов `PT_TLS` в исполняемых файлах ELF.
- **Векторные шрифты в системе:**
  - В `/boot/fonts/` (образ ISO) скопированы шрифты `OpenSans.ttf` и `Hack.ttf`.
- **Верификационные приложения в `/boot/apps/`:**
  - `gfx_demo.elf` (1.07 МБ) — тест Pixman + xkbcommon + чтение evdev + DRM/fbdev.
  - `font_demo.elf` (1.8 МБ) — антиалиасинг векторного текста FreeType 2, отрисовка карточек с альфа-блендингом Pixman, вывод координат мыши в реальном времени и отрисовка аппаратного курсора в Ring 3.
  - Команды `gfx`, `fontdemo`, `plasma` добавлены в консоль KeshOS Recovery.

---

## 📂 3. Расположение ключевых файлов и директорий

| Путь | Назначение |
|---|---|
| `toolchain/musl/` | Постоянный сисрут musl: `lib/libc.a`, `libpixman-1.a`, `libwayland-*.a`, `libfreetype.a`, `libz.a`, `libxkbcommon.a`, `libffi.a`, `include/` |
| `ports_bin/` | Скомпилированные ELF-бинарники, включая `qt_demo.elf` и `qml_demo.elf` |
| `toolchain/musl/` | musl-sysroot с `libc++`, `libc++abi`, `libunwind`, QtBase и Qt Declarative/Quick 6.11.2 |
| `ports_build/` | Постоянные инкрементальные build-каталоги портов, не удаляемые `build.sh` |
| `assets/fonts/` | Шрифты TrueType (`OpenSans.ttf`, `Hack.ttf`), которые автоматически копируются в `/boot/fonts/` при сборке ISO |
| `plasma-desktop/` | Склонированный пользователем официальный репозиторий KDE Plasma Desktop 6 (`6.8.80`) |
| `kernel/linux_syscall.c` | Реализация системных вызовов Linux POSIX (диспетчер номеров syscalls x86_64) |
| `kernel/unix_ipc.c` | Реализация сокетов `AF_UNIX`, `SCM_RIGHTS`, `memfd_create`, `epoll` |
| `kernel/evdev.c` | Драйвер устройств ввода `/dev/input/event0` и `event1` |
| `kernel/drm_fb.c` | Драйвер кадрового буфера `/dev/fb0` и `/dev/dri/card0` (DRM KMS ioctl + mmap) |
| `kernel/elf.c` | Загрузчик 64-битных ELF в пространство пользователя |
| `tools/make_iso.py` | Скрипт сборки ISO (автоматически упаковывает `ready/`, `ports_bin/`, шрифты и ядро) |
| `build/keshos.iso` | Готовый загрузочный образ ISO (44 МБ, limine UEFI + BIOS) |
| `log.md` | Постоянный подробный бортовой журнал всех шагов и ревизий |
| `KDE_PLASMA_PORTING_ROADMAP.md` | Полная 6-фазная дорожная карта проекта |

---

## 🛠️ 4. Как собирать и проверять систему

### 1. Сборка всех 7 портированных приложений:
```bash
bash ports/build_ports.sh
```
Все бинарники соберутся через `clang --target=x86_64-unknown-linux-musl --sysroot=toolchain/musl` и лягут в `ports_bin/`.

### 2. Сборка ядра и ISO-образа KeshOS:
```bash
./build.sh
```
Этот скрипт:
- Генерирует `build.ninja` через `generate_ninja.py`.
- Компилирует ядро `build/kernel.elf` и нативные приложения.
- Запускает `tools/make_iso.py`, который формирует файловую структуру `ready/` (включая `/boot/apps/` со всеми портированными бинарниками и `/boot/fonts/` со шрифтами).
- Создаёт `build/keshos.iso` через `xorriso`.
- Запускает `tools/verify_iso.py` для сверки SHA256 ядра и инсталлера.

### 3. Запуск в QEMU:
```bash
./run.sh
```
(или `qemu-system-x86_64 -cdrom build/keshos.iso -m 2G -serial stdio -vga std`)

В консоли KeshOS доступны команды:
- `hello` — проверка libc musl.
- `ipc` — проверка сокетов `AF_UNIX` и `memfd`.
- `wayland` — демонстрация дисплейного сервера Wayland.
- `gfx` — демонстрация Pixman и ввода evdev.
- `fontdemo` / `plasma` — интерактивный запуск векторного рендерера FreeType 2 и курсора мыши.
- `qt` — статический Qt Widgets 6.11.2 через `linuxfb`.
- `qml` — статический Qt Quick/QML 6.11.2 с программным scene graph.

---

## 🎯 5. Что предстоит сделать дальше: ФАЗА 4 (Qt 6 и QPA)

Для запуска KDE Plasma Shell (`plasmashell`) необходим фреймворк **Qt 6** (QtCore, QtGui, QtQuick/QML).  
Вся низкоуровневая инфраструктура для Qt 6 в ядре KeshOS **уже готова**:
- Графика: `/dev/fb0` с zero-copy mmap (подходит для `qlinuxfb` / программного рендерера).
- Ввод: `/dev/input/event0` и `event1` (подходит для Qt `evdevkeyboard` и `evdevmouse`).
- Сокеты: `/tmp/wayland-0` и `memfd` (подходит для `qwayland`).
- Шрифты: FreeType 2 и TrueType шрифты в `/boot/fonts/`.

### Конкретные задачи Фазы 4:

#### Текущий статус

- Собраны `libc++`, `libc++abi` и `libunwind` для `x86_64-unknown-linux-musl`.
- Собран QtBase 6.11.2: Core, Gui, Widgets, Network, linuxfb и evdev.
- Собран Qt Declarative 6.11.2: QML, Qt Quick, Quick Controls, Layouts, Dialogs, Shapes и программный scene graph.
- `qt_demo.elf` и `qml_demo.elf` — статические ELF без `PT_INTERP`, собранные для KeshOS.
- Осталась runtime-проверка в QEMU; недостающие syscall/ioctl нужно закрывать по `serial.log`.
- Первый runtime-проход локализовал падение QML в `QV4::BlockAllocator::allocate`: фиктивный `mprotect` оставлял зарезервированные страницы read-only. Теперь `mprotect` реально меняет PTE, memfd отображения удерживают backing pages, VFS `fstat` возвращает реальный размер файла, а `statx`, `prlimit64`, `getrlimit`, `mremap` fallback и базовые файловые syscall закрыты. ISO с kernel SHA-256 `ea6369835d9a6a1293dfabc4086c8430e2c83ee7b1a9fa97c1b06d517f315e76` прошёл `verify_iso: PASS`; нужен новый runtime-запуск команды `qml`.

1. **Поддержка C++ рантайма для KeshOS:**
   - Для C++ программ требуется стандартная библиотека C++ (`libc++` или `libstdc++`).
   - На хосте установлен Arch Linux с пакетами `qt6-base (6.11.2)`, `extra-cmake-modules (6.30.0)` и компилятором `clang++` / `g++`.
   - Проверено: статически скомпилированный бинарник C++ с `g++ -static` уже содержит все сегменты `PT_LOAD` и `PT_TLS` и поддерживается ядром KeshOS.

2. **Выбор стратегии интеграции Qt 6:**
   - **Вариант А (Прямой QPA-бэкенд для KeshOS):**
     Написать минимальный плагин платформы Qt (QPA — Qt Platform Abstraction): `qkeshos` или использовать встроенный плагин `qlinuxfb` (`-platform linuxfb:fb=/dev/fb0`).
     Плагин `qlinuxfb` в Qt открывает `/dev/fb0`, вызывает `FBIOGET_VSCREENINFO`, делает `mmap` и рисует виджеты и QML через программный растровый движок (`QPaintEngine::Raster`). Это работает со 100% совместимостью без необходимости сложного 3D GPU драйвера!
   - **Вариант Б (Wayland QPA — `qwayland`):**
     Запуск приложений Qt с флагом `-platform wayland`. Приложение подключается к нашему Wayland-серверу на `/tmp/wayland-0`, запрашивает буфер через `wl_shm_create_pool` (`memfd_create`), рисует окно и композитор KeshOS отображает его на экране.

3. **Динамический линковщик (`PT_INTERP`):**
   - Если запускать динамические бинарники Qt (собранные с общими библиотеками `.so`), ядру KeshOS в `kernel/elf.c` нужно поддержать заголовок `PT_INTERP`:
     1. Прочитать путь интерпретатора (например, `/lib64/ld-linux-x86-64.so.2` или `ld-musl-x86_64.so.1`).
     2. Отобразить интерпретатор в память (например, по адресу `0x7F0000000000`).
     3. Передать в `auxv` адрес базы интерпретатора `AT_BASE`, заголовки `AT_PHDR`, `AT_ENTRY`.
     4. Передать управление на точку входа интерпретатора (`rip = interp_entry`).
   - Альтернативно: использовать статическую сборку Qt (`-static`) либо самодостаточные бандлы.

4. **Тестовое приложение Qt 6 на KeshOS:**
   - В директории `ports/qt_demo/` создан минимальный проект `ports/qt_demo/main.cpp` и `CMakeLists.txt`.
   - Задача: скомпилировать и успешно запустить первое окно `QWidget` / `QML` на экране KeshOS с выводом через `/dev/fb0` или `/tmp/wayland-0`.

---

## 🔮 6. Последующие фазы (5 и 6)

- **Фаза 5 (D-Bus и KF6):**
  - Запуск локального D-Bus демона (`dbus-daemon --session`) поверх наших `AF_UNIX` сокетов.
  - Подключение базовых библиотек KDE Frameworks 6 (`KConfig`, `KCoreAddons`, `KWindowSystem`).
- **Фаза 6 (KDE Plasma Desktop Shell):**
  - Запуск `plasmashell` из репозитория `./plasma-desktop`.
  - Поддержка протокола `wlr-layer-shell` (или `kde-layer-shell`) для прикрепления нижней панели задач к низу экрана и меню Kickoff.
  - Обои рабочего стола, панель задач, виджет часов и системный трей.

---

> 💡 **Совет следующему агенту:**  
> Всегда проверяй целостность сборки через `./build.sh` (должен завершаться с `verify_iso: PASS`).  
> Подробные детали каждой ревизии и решений записаны в [log.md](file:///home/danila/Рабочий%20стол/keshos/log.md).  
> Все портированные бинарники складывай в `ports_bin/` — скрипт `tools/make_iso.py` автоматически заберёт их в ISO.
