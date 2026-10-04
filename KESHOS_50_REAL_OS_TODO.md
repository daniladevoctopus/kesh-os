# KeshOS — 50 gates до полноценной современной ОС

**Версия чеклиста:** 2.0
**Дата:** 27 сентября 2026

> Главная цель этого файла — не красиво считать проценты, а точно понимать, **что делать сейчас, что потом и что можно отложить**.

## Правила оценки

- `[x]` **ВЫПОЛНЕНО** — реально существует и для текущего критерия подтверждено кодом/сборкой/реальным тестом.
- `[~]` **ЧАСТИЧНО** — рабочий фундамент есть, но не хватает важной части, безопасности, универсальности или production-grade поведения.
- `[ ]` **НЕ ВЫПОЛНЕНО** — подсистема отсутствует либо пока является заготовкой.

**Ничего не засчитываем за README, название файла, наличие бинарника или красивый интерфейс.**

## Текущая честная оценка

**10 выполнено / 39 частично / 1 не выполнено.**

Консервативная формула этого чеклиста:

- `[x]` = 1.0
- `[~]` = 0.55
- `[ ]` = 0

`(10 × 1 + 39 × 0.55) / 50 = 62.9%`

# **Текущая техническая готовность: ~63%**

Это не «30% кода» и не прогноз. Это только доля закрытых gate'ов из этих 50 больших требований.

---

# Иерархия разработки

## Правило порядка

**Не надо строить верхний этаж, пока фундамент под ним дырявый.**

Рекомендуемая последовательность:

`Boot → Memory → CPU/Interrupts → Process/Userspace Safety → OS ABI → Storage → Input/Graphics → Desktop Userspace → Network/Security → Packages → Installer → Updates/Recovery`

При этом пункты внутри одной фазы можно делать параллельно.

### Что делать первым

**Фазы 1–3:** memory safety, процессная модель, CPU/interrupt foundation и storage architecture.

### Что делать после фундамента

**Фазы 4–5:** нормальный OS API, input/graphics, userspace desktop, network/security.

### Что делать почти в самом конце

**Фаза 6:** package installation, затем installer.

### Что делать последним

**Фаза 7:** production updates + recovery.

> Installer действительно логично делать поздно, но не буквально самым последним: update/recovery зависят от installer/storage/boot layout и должны быть последним большим слоем. При этом installer можно начинать проектировать раньше, чтобы требования к GPT/EFI/filesystem были известны заранее.

---

# PHASE 0 — Уже работающий baseline

Это то, что уже существует и не должно заново переделываться без причины.

### 1. [x] Legacy BIOS boot

**Статус:** подтверждено реальным тестом владельца проекта.

Limine boot path работает в Legacy/BIOS режиме.

**Дальше:** добавить boot regression tests, но функциональный gate закрыт.

---

### 2. [x] UEFI x86_64 boot

**Статус:** подтверждено реальным тестом владельца проекта.

UEFI запуск проверен и работает плавно.

**Дальше:** автоматизировать проверку разных UEFI-конфигураций.

---

### 3. [x] Framebuffer graphics

Реальный framebuffer rendering path есть. Desktop и software rendering используют его для вывода.

---

### 4. [x] Persistent filesystem path на полном HDD

FAT32 read/write/mkdir/delete path реально существует. Владелец проекта отдельно подтвердил работу с полноценным HDD.

Это не закрывает современные AHCI/NVMe/GPT требования, но базовый persistent storage gate для текущей системы есть.

---

### 5. [x] Рабочий основной Desktop

Главный Desktop находится в `src/desktop.c` и сейчас линкуется прямо в `kernel.elf`.

Важное уточнение: экспериментальный виртуальный/Ring3 desktop из VFS в этом gate не учитывается как основной Desktop.

---

# PHASE 1 — Memory и CPU foundation

**Цель:** перестать зависеть от хрупкой модели «kernel всё может, userspace ещё не полностью безопасен».

### 6. [~] Physical Memory Manager

PMM реально выделяет и повторно использует страницы. Free-range metadata защищены IRQ-safe spinlock, освобождённые смежные диапазоны сливаются, а обнуление выделенных страниц вынесено за пределы allocator critical section. Contiguous allocation использует best-fit вместо first-fit для уменьшения внешней фрагментации. Диагностика публикует free/allocated/peak bytes, largest free run, число диапазонов, allocations/frees и failures; краткий отчёт доступен командой `mem`.

**Не хватает:** per-CPU caches, metadata growth beyond the static range ceiling, allocation ownership/leak tracing и stress verification.

**Приоритет:** P0

---

### 7. [~] Virtual Memory Manager

Есть многоуровневые page tables и отдельные user address spaces. Добавлены process-owned anonymous mappings через `SYS_VM_MAP` / `SYS_VM_UNMAP` с W^X, rollback и учётом регионов. Unmap сразу сворачивает и освобождает пустые PT/PD/PDPT levels.

**Не хватает:** полноценного unmap/free, destruction address spaces, shared mappings, demand paging, mmap/brk-подобных механизмов.

**Приоритет:** P0

---

### 8. [~] Page-fault и user-crash handling

Kernel умеет распознавать user-originated fault и завершать пользовательский процесс.

**Проблема:** плохой userspace pointer внутри syscall всё ещё может ударить по kernel execution path.

**Приоритет:** P0

---

### 9. [x] `copy_from_user()` / `copy_to_user()`

**Критический gate.**

Реализованы проверка пользовательского диапазона, страничное копирование через HHDM и bounded string access. Базовая boot-регрессия с этим путём подтверждена владельцем в VirtualBox 29 сентября 2026; отдельные fault-injection тесты остаются нужны.

Пример API:

```c
copy_from_user(dst, user_src, size);
copy_to_user(user_dst, src, size);
user_range_valid(addr, size, flags);
user_strnlen(...);
```

**Приоритет:** P0

---

### 10. [~] NX / W^X / корректные page permissions

CPU NXE включается на syscall initialization; ELF loader отклоняет `PF_W | PF_X`, применяет финальные `R/W/NX` page flags и мапит user stack как writable+NX. VMM теперь централизованно отклоняет любую user mapping или permission transition, одновременно writable и executable, поэтому вызывающая подсистема не может обойти W^X.

Boot security self-test теперь отдельно проверяет, что VMM отвергает user W+X mapping. **Не хватает:** execute-fault проверки NX и полного аудита firmware/kernel-only mappings.

**Приоритет:** P0

---

### 11. [x] Process lifecycle и cleanup

Process spawn/exit/kill уже существуют.

**Не хватает:** гарантированного освобождения address space, pages и всех process resources.

**Приоритет:** P0

---

### 12. [~] Threads

Есть kernel/user thread model, TID, отдельные user contexts, dedicated stacks для дополнительных потоков, scheduler integration и `SYS_THREAD_CREATE` / `SYS_THREAD_EXIT`.

**Не хватает:** runtime SMP-safe execution, per-CPU kernel/TSS separation и boot-tested multithread stress coverage.

**Приоритет:** P0

---

### 13. [~] Per-CPU architecture

Добавлен `cpu_local_t`, CPU topology registry, BSP/AP registration и per-CPU counters/current-thread pointer.

**Не хватает:** отдельного TSS/kernel stack/GS base на CPU и полного SMP-safe interrupt/syscall state.

**Приоритет:** P0

---

### 14. [~] APIC / IOAPIC

Добавлены local APIC enable/EOI, IOAPIC discovery through ACPI MADT, IRQ redirection и PIC-to-APIC switch. Boot переводит PIT/keyboard/mouse IRQ на IOAPIC, учитывает source overrides без дублирования исходного GSI и автоматически возвращает PIC route, если timer interrupt не приходит в bounded probe window.

**Не хватает:** real hardware/runtime verification and fuller interrupt-source policy.

**Приоритет:** P0

---

### 15. [~] SMP / multi-core CPU

Limine SMP response is wired, AP callbacks initialize per-CPU state/local APIC and enter an interrupt-enabled AP loop.

**Не хватает:** verified multi-core runtime, IPI subsystem, SMP scheduler migration and SMP synchronization primitives.

**Приоритет:** P0

---

### 16. [~] Timer / timekeeping

Timer и sleep path есть.

Добавлены nanosecond monotonic clock и realtime clock, который якорится на CMOS RTC при boot. Userspace получает оба clock domain через `kesh_clock_get()`. Если invariant TSC доступен, но CPUID не сообщает его частоту, kernel калибрует его по ACPI PM timer; поддерживаются legacy I/O и extended GAS timer blocks с 24/32-bit wrap mask.

**Не хватает:** per-CPU timers, NTP/RTC correction, timezone policy и более современной архитектуры времени.

---

### 17. [~] ELF64 loader

Реальный static ELF64 loader существует и используется userspace. Он проверяет ABI/version/header sizes, program-table bounds, integer overflow, alignment, user address range, stack collision, segment overlap, допустимые flags и принадлежность entry point исполняемому `PT_LOAD`. Dynamic/interpreter/TLS binaries отклоняются явно; любой failure уничтожает создаваемое address space. User stack отображается writable+NX.

**Не хватает:** совместимого ASLR после устранения регрессий тяжёлых C++ приложений. Dynamic linker/TLS не входят в текущий static KEA ABI.

---

### 18. [~] Syscall ABI

Настоящий `syscall/sysret` path и dispatcher существуют. ABI получил фиксированную версию и feature bitmap через `kesh_abi_info`; неизвестные номера, размеры, enum/flag values и userspace buffers проверяются kernel-side. Исправлен bounded `get_procs`, а spawn больше не подменяет identity вызывающего процесса внутри syscall.

**Не хватает:** стабильной ABI policy, строгой проверки аргументов и безопасного userspace access layer.

---

# PHASE 2 — Настоящая OS API и безопасность

### 19. [~] File descriptor model

Есть per-process таблица изолированных файловых дескрипторов и userspace API `open/close/read/write/seek`. `open` поддерживает create, truncate и append; дескрипторы закрываются при завершении процесса, а read/write хранят file offset. Добавлены `pipe`, `dup`, deterministic `dup2` redirection и immediate `poll` readiness/hangup для file/pipe descriptors. Каталоги открываются отдельным read-only descriptor kind и последовательно читаются через `kesh_readdir`, сохраняя общий offset при `dup`. Generic exec наследует file/pipe descriptors с общими open-file offsets; PTY stdio выдаётся через контролируемый attach path.

**Не хватает:** blocking `poll/select` и полных concurrency semantics общей descriptor table.

---

### 20. [~] IPC

Есть kernel message queues и userspace `ipc_send/ipc_receive`; сообщения принадлежат процессу и удаляются вместе с ним. Typed-вариант передаёт 16-bit message type, фильтрует receive одновременно по sender/type и возвращает оба значения через metadata descriptor. Send отклоняет несуществующие target processes, а очереди и `ipc_last_sender` защищены общим IRQ-safe SMP spinlock. Старый ABI сохранён как type 0. Unidirectional byte pipes интегрированы в FD table, поддерживают `dup`, readiness/hangup и cleanup последнего reader/writer.

Process layer теперь знает process groups и ограниченный signal set (`INT/KILL/TERM/STOP/CONT`). PTY foreground group получает terminal-generated `Ctrl-C`/`Ctrl-Z`, а master может продолжить или завершить foreground job через контролируемый API.

**Не хватает:** userspace signal handlers/masks, blocking wakeups, shared memory и единая object/handle model.

---

### 21. [~] Users / groups / credentials

У каждого процесса есть UID/GID/groups credentials; обычные KEA приложения стартуют как непривилегированный user. Applications могут запросить свои текущие credentials через userspace API.

**Не хватает:** login/session lifecycle, persistent account database, group management и userspace identity API.

---

### 22. [~] Privilege / root model

Root credential выдаётся только KEA v2 package с валидной pinned-repository подписью и явным `ROOT`; legacy/raw binaries всегда остаются UID 1000. Privileged syscall требует и capability, и UID 0. Root может только отзывать capability у процесса через отдельный audited syscall, но не добавлять её произвольно.

**Не хватает:** trusted system image, controlled elevation, authentication, root session и audit trail.

---

### 23. [~] Kernel-enforced KEA permissions

KEA permission bits должны влиять на реальные операции:

- FS
- network
- GUI
- audio
- privileged actions

Проверки KEA permissions есть в syscall dispatcher для FS, network, GUI и privileged actions. Владелец окна, socket и fd дополнительно привязаны к текущему процессу.

Signed KEA v2 packages проверяются в kernel через pinned ECDSA P-256 repository key до запуска ELF payload. Legacy KEA остаются совместимыми, но не могут получить `ROOT` permission.

Audio syscalls требуют `KEA_PERM_SOUND`; root получил audited runtime capability revocation.

**Не хватает:** per-path FS permissions и brokered runtime permission prompt.

**Приоритет:** P0

---

### 24. [~] Application sandbox

Приложения работают в Ring 3 с отдельным address space; syscall dispatcher проверяет KEA capabilities, а GUI windows, fd и sockets изолированы владельцем процесса.

**Не хватает:** filesystem namespaces/quotas, network destinations policy, audit, brokered privileges и sandbox escape test suite.

**Приоритет:** P0/P1

---

### 25. [~] OS-grade CSPRNG

В ядре есть HMAC-DRBG SHA-256, который при старте получает seed из RDSEED или RDRAND; в VirtualBox подтверждена инициализация через RDSEED. Hardware source проходит continuous repetition test и adaptive per-bit health test до инициализации DRBG. Отдельный SHA-256 entropy pool смешивает timestamped input events с консервативной оценкой entropy и инициирует reseed после накопления 256 bits; обязательный reseed также выполняется после каждого 1 MiB выдачи. Экспортируются counters generated bytes, reseeds, pool credit и health failures.

При доступном VFS DRBG смешивает integrity-checked persistent boot seed и сразу ротирует его новым CSPRNG output; новый seed также сохраняется перед clean shutdown.

**Не хватает:** независимого firmware/platform entropy source, per-source adaptive windows и отдельных runtime/fault-injection проверок syscall-интерфейса.

**Приоритет:** P0

---

### 26. [~] Init / service manager

Есть kernel service manager: он запускает process, window, VFS, WM-config и network в порядке зависимостей, отслеживает состояние и умеет перезапускать сервис по API. При boot failed service получает bounded restart attempts. Добавлен userspace-daemon lifecycle: executable path, PID binding, authenticated heartbeat syscall, death/timeout detection и exponential-backoff restart policy; supervisor poll интегрирован в основной event loop. После VFS загружается декларативный `/hdd/etc/services.conf` (`name path dependency heartbeat_ms restart_limit`), а потеря dependency останавливает зависимый daemon до восстановления зависимости.

**Не хватает:** runtime-проверки manifest/restart path и более богатой activation policy.

---

### 27. [~] TTY / PTY subsystem

Есть kernel TTY foundation с отдельными input/output ring buffers для четырёх терминалов. Ring 3 приложение может создать собственный TTY, читать input, писать output, узнавать доступный input и закрыть его; TTY автоматически освобождается при завершении владельца.

Добавлены четыре PTY master/slave pair с process ownership, attach slave к целевому процессу, discoverable controlling PTY, cleanup обоих endpoints, canonical/raw modes, echo, erase и EOF handling. Userspace ABI предоставляет create/attach/read/write/available/set-mode/close/current. Process groups и foreground-group control обеспечивают базовый job control: `Ctrl-C` посылает `SIGINT`, `Ctrl-Z` останавливает foreground group, master может отправить `CONT/TERM/KILL`.

PTY attach автоматически связывает slave process descriptors 0/1/2 с controlling terminal; read/write/poll работают через общий FD ABI. Ручное открытие controlling PTY остаётся доступным для нестандартной раскладки descriptors.

**Не хватает:** generic exec inheritance/redirection, background-access policy, blocking wakeups и полноценных userspace signal handlers/masks.

---

### 28. [~] Kernel logging / diagnostics

Serial logging и panic diagnostics уже существуют. Kernel logger хранит последние 32 записи в memory ring buffer; приложения с FS capability могут безопасно получить полный или отфильтрованный по severity/module снимок. При штатном shutdown журналы ротируются до четырёх поколений, текущий ring сохраняется в `/hdd/KLOG.TXT`, а следующий boot загружает его как отдельный previous-boot snapshot. Terminal показывает начало прошлого журнала командой `lastlog`.

**Не хватает:** безопасной panic-time записи и crash dump с unwinding.

---

# PHASE 3 — Hardware и storage foundation

### 29. [~] PCI / PCIe device model

PCI enumeration и реальные device drivers существуют. Добавлен безопасный обход standard PCI capability list. Configuration backend выбирает ACPI MCFG ECAM для валидных segment-0 bus ranges и сохраняет legacy CF8/CFC fallback; оба пути сериализованы общим IRQ-safe lock.

PCI layer теперь также умеет включить нужные command bits устройства и проверить результат. Block initialization использует это для AHCI/NVMe: включает MMIO и bus mastering до будущего command-driver bring-up.

Добавлено безопасное BAR resource probing с временным отключением I/O/MMIO decode, восстановлением исходной PCI configuration и определением address/size/type/prefetch/64-bit properties. Interrupt API поддерживает MSI и MSI-X table programming, per-vector masking, APIC destination и отключение legacy INTx после успешного включения.

**Не хватает:** централизованного resource allocator, multi-segment/extended PCIe configuration API, hotplug и более универсального driver binding model.

---

### 30. [~] ACPI

Есть ACPI foundation: RSDP и RSDT/XSDT проверяются checksum-ами, а валидные SDT можно находить по signature. MADT уже используется APIC-путём. FADT/DSDT path читает `_S5_` для shutdown, FADT reset register используется для reboot, а PM timer экспортируется как validated legacy I/O или extended GAS counter и применяется для TSC calibration.

**Не хватает:** suspend/resume power states, thermal/battery, полноценного AML interpreter и полного device discovery.

**Приоритет:** P0

---

### 31. [x] Generic block layer

Целевая цепочка:

`filesystem → VFS → block layer → storage driver → hardware`

Добавлен block-device API с range checks, read/write, flush и metadata для ATA/ATAPI, AHCI и NVMe устройств. Операции сериализованы per-device lock; flush реализован через ATA CACHE FLUSH, AHCI FLUSH CACHE EXT и NVMe Flush. FAT32 clean shutdown завершает запись clean bit аппаратным flush. FAT32 и ISO9660 используют только generic block API.

**Приоритет:** P0/P1

---

### 32. [~] GPT / partition manager

Есть read-only GPT parser: он проверяет сигнатуру, CRC заголовка и CRC всего массива partition entries, валидирует границы диска и лимитирует размер metadata до безопасного значения.

GPT parser проверяет CRC и header, и всего partition array; также экспортирует UTF-16 names и распознаёт EFI System, Microsoft Basic Data и Linux filesystem GUID.

**Не хватает:** UI для metadata, создание/изменение таблицы, EFI System Partition handling.

**Приоритет:** P0

---

### 33. [~] AHCI / NVMe

AHCI SATA теперь инициализирует command list/FIS/DMA memory, выполняет IDENTIFY, регистрирует SATA-диск в generic block layer и выполняет сериализованные LBA48 DMA read/write с bounded chunks и timeout/error checks. NVMe path настраивает admin/I/O submission-completion queues, выполняет Identify Controller/Namespace и регистрирует namespace с PRP read/write в block layer.

**Не хватает:** MSI/MSI-X interrupt mode, AHCI NCQ, NVMe multiqueue, hotplug, power management и runtime verification на hardware/VM.

**Приоритет:** P0

---

### 34. [~] VFS abstraction

VFS реально существует и предоставляет hierarchy/read/write/directory APIs. Writable FAT32 и read-only ISO9660 mount discovery опираются на block-device metadata, а не на ATA-specific enumeration.

**Не хватает:** более строгой abstraction boundary, динамических structures, масштабируемых semantics и separation from physical storage implementation.

---

### 35. [~] Filesystem consistency / journal / fsck

FAT32 file replacement already follows copy-on-write order: data chain is written before the directory entry switches. Delete now commits directory removal before freeing clusters, preventing a visible entry from pointing to a reallocated chain after power loss.

После успешной FAT mutation обновляется валидный FAT32 FSInfo free-cluster count, поэтому storage statistics не остаётся намеренно устаревшей после reboot.

FAT32 mount использует стандартный clean-shutdown bit в FAT[1]: активный volume помечается dirty, а штатное выключение синхронизирует FSInfo и возвращает clean state. После обнаружения dirty volume автоматически запускается repair pass: он обходит дерево каталогов, выявляет оборванные/зацикленные chain, завершает их EOC, пересчитывает свободное пространство и освобождает недостижимые orphan clusters. Тот же проход доступен вручную командой `fsck` с отчётом о возвращённых кластерах.

**Не хватает:** полноценного journal/transaction log, масштабируемой очереди каталогов для очень больших деревьев и failure-injection tests.

**Приоритет:** P0/P1

---

### 36. [~] USB stack

Есть корректное PCI обнаружение USB host controllers и их типов UHCI/OHCI/EHCI/xHCI. xHCI path включает PCI MMIO/bus-mastering, выполняет bounded halt/reset и отслеживает питание/подключение root-hub ports.

**Не хватает:** xHCI command/event rings, USB device enumeration, hubs, HID, storage и полноценный hotplug event path.

**Приоритет:** P0

---

# PHASE 4 — Input, graphics и userspace Desktop

### 37. [~] Universal input subsystem

PS/2 keyboard/mouse path есть. Добавлен device-neutral input queue: драйверы PS/2 публикуют key и pointer events через единый API.

**Не хватает:** универсального input device layer, USB HID, hotplug, touchpad, gamepad и multimedia keys.

---

### 38. [~] Audio subsystem

AC'97 теперь использует выделенные PMM DMA pages и передаёт bus-master controller физические, а не HHDM virtual addresses. Controller запускается при boot через sound manager; HDA probe использует HHDM MMIO mapping, но честно не объявляется playback-ready без stream engine. Userspace получил capability-enforced `audio_device`, volume и bounded PCM16 playback syscalls; доступ требует `KEA_PERM_SOUND`.

**Не хватает:** HDA CORB/RIRB и output stream engine, mixer/resampling, shared streams, interrupt-driven refill и runtime hardware verification.

---

### 39. [x] Перенос основного `desktop.c` в userspace

**Выполнено:** основной Desktop вынесен в userspace как независимый процесс Ring 3 (`/apps/shell.kea`). Ядро выступает в роли display server / compositor (`uwindow_render_all`), мапит desktop surface (`__desktop__`), маршрутизирует события ввода мыши/клавиатуры и отрисовывает аппаратный курсор. В случае сбоя или отсутствия shell ядро сохраняет встроенный аварийный fallback.

**Функционал Userspace Shell:**
- Фирменные обои (KeshOS Desert Sunset, Deep Night, Slate, Paper);
- Ярлыки рабочего стола (Explorer, Notepad, Settings, Terminal, Task Manager, Paint, DOOM, Installer);
- Нижний стеклянный док (Dock / Taskbar) с иконками приложений, индикаторами запущенных процессов, системным треем (RAM, звук 70%, часы HH:MM:SS и дата);
- Меню Пуск (Start Menu) с быстрым запуском, сменой обоев, кнопками перезагрузки и выключения;
- Контекстное меню рабочего стола (ПКМ);
- Возможность создавать любые альтернативные оболочки (KDE, tiling, retro) без пересборки ядра.

---

### 40. [~] Multi-monitor / DPI / accessibility

Display subsystem перечисляет до восьми framebuffer outputs от Limine и хранит geometry/stride/bpp/scale для каждого. Основной Desktop пока использует только primary output.

При boot теперь очищаются все валидные framebuffer outputs, поэтому secondary display не сохраняет содержимое firmware до запуска Desktop.

**Не хватает:** rendering на несколько outputs, physical DPI discovery, user scaling controls, keyboard navigation и accessibility APIs.

---

### 41. [ ] Hardware GPU acceleration

Текущий 3D path — software renderer.

SIMD/software rendering не равен GPU driver.

**Приоритет:** P1

---

# PHASE 5 — Network и trusted Internet

### 42. [~] Network drivers

В проекте есть e1000, RTL8139 и virtio-net.

**Не хватает:** большего hardware coverage, modern device support и более универсальной driver/device model.

---

### 43. [~] IPv4 network stack

Есть Ethernet/ARP/DHCP/IPv4/DNS и существующий TCP path. IPv4 ingress теперь проверяет exact v4/IHL form, header checksum, declared total length и отклоняет fragments до появления reassembly; ICMP/UDP payload parsing дополнительно ограничен границами принятого frame.

**Не хватает:** IPv6, более зрелого TCP behavior, полноценной socket layer и большей concurrency robustness.

---

### 44. [~] Sockets API

Есть userspace TCP socket API: `socket`, `connect`, `send`, `recv`, `close`. Handles принадлежат процессу, а сокеты автоматически закрываются при его завершении. UDP sockets используют тот же process-owned handle model, поддерживают `bind`, connected `send/recv`, `sendto/recvfrom`, автоматический ephemeral port и demultiplexing входящих datagram по local port и connected peer. Immediate `kesh_socket_poll` сообщает read/write/hangup readiness без блокировки.

**Не хватает:** TCP `bind/listen/accept`, blocking poll/wakeup, полноценной datagram queue, нескольких одновременных TCP connections и зрелой error model.

Целевой интерфейс:

`socket / bind / connect / listen / accept / send / recv`

---

### 45. [x] Secure HTTPS trust chain

TLS/HTTPS transport существует. TLS session берёт entropy только из kernel CSPRNG и отказывается стартовать без него. BearSSL minimal X.509 engine использует встроенный system trust anchor ISRG Root X1, проверяет certificate chain, hostname и validity period по RTC. Недоверенная цепочка, несовпадающий hostname и отсутствие достоверного wall clock завершают запрос ошибкой.

Нужно довести до:

- system CA trust store
- certificate chain validation
- hostname verification
- expiration checks
- rejection of untrusted certs
- trust-store updates

Системный trust store расширяется через `/hdd/etc/CASTORE.BIN`: generation, размеры и SHA-256 проверяются до ECDSA P-256 подписи одним из pinned KPM keys. Повреждённое обновление не заменяет встроенный fallback, понижение generation в текущей сессии отклоняется, а root-only syscall выполняет hot reload. `tools/ca-store.py` извлекает RSA anchor из PEM/DER сертификата и создаёт подписанный образ.

**Приоритет:** P0

---

### 46. [~] Browser engine

Собственная web foundation и браузерное приложение существуют.

**Не хватает:** полноценного CSS layout, JavaScript, sandbox, WebAssembly, WebGL/WebGPU, modern storage APIs и широкой compatibility.

---

# PHASE 6 — Packages и installation

### 47. [~] KPM: реальный network downloader + registry

KPM действительно ходит по сети к registry/API, получает каталог, делает HTTPS download `.kea` package и сохраняет его в `/hdd/apps` через VFS. Registry v2 использует immutable tree `packages/<id>/<version>/<id>.kea`, нормализует параметры download API и генерирует индекс из проверенных пакетов.

Нативный клиент проверяет pinned ECDSA P-256 signature, canonical SHA-256 и ELF CRC, разрешает минимальные версии зависимостей, ведёт локальную package database и выполняет recovery-aware install transaction.

То есть:

`real network package downloader = ✅`

`complete package manager = пока нет`

---

### 48. [~] Package security / dependencies / atomic install

Нужны:

- cryptographic signatures
- trusted repository keys
- package identity
- dependency graph
- version constraints
- local package database
- uninstall
- transactions

Реализованы KEA v2 signatures, pinned repository trust key, package identity/version, dependency records и recursive minimum-version install, локальная `/hdd/apps/.kpm.db`, dependency-aware uninstall, transaction journal/backup/recovery и host-side atomic download. Release tooling создаёт ECDSA keys, подписывает пакеты и строит versioned repository index; приватный ключ хранится вне репозитория.

**Не хватает:** SAT solver для альтернативных constraints/conflicts, key rotation/revocation, delta updates, filesystem-wide crash-consistency и runtime regression tests.
- rollback
- atomic install

CRC32 может проверять целостность, но не доказывает авторство пакета.

**Приоритет:** P0/P1

---

### 49. [~] Graphical installer

В live ISO есть отдельное userspace-приложение `Install KeshOS` на главном экране. Оно показывает выбранную файловую систему, честно позволяет выбрать только работающий FAT32 path и по явному действию сохраняет план в `/hdd/KESHOS/INSTALL.TXT`. Это действие не форматирует диск и не меняет boot sector.

После сохранения плана установщик умеет скопировать девять встроенных KEA applications в `/hdd/KESHOS/APPS`. Это сохраняет полезную часть live-system на target FAT32, но ещё не делает её independently bootable.

`ext4` и `Btrfs` показаны как будущие варианты: в текущем дереве нет их форматтера или mount-драйвера, поэтому установщик не выдаёт выбор за рабочую установку.

Installer должен уметь поставить KeshOS на современный пустой диск без ручного шаманства:

1. boot installer
2. detect disk
3. create/read GPT
4. create EFI System Partition
5. create filesystem
6. install kernel/system files
7. install bootloader
8. create user
9. locale/timezone/keyboard
10. first boot/OOBE
11. error handling/recovery path

**Приоритет:** P0 перед массовым распространением.

---

# PHASE 7 — Updates и Recovery — последний большой слой

### 50. [~] Production updates + recovery

Это действительно логично делать последним большим слоем.

Нужны:

- signed OS images
- update manifests
- atomic updates
- A/B slots или эквивалент
- rollback
- recovery environment
- boot failure recovery
- interrupted-update safety
- version migration

Добавлен signed KSU v1 bundle с тем же pinned ECDSA P-256 trust root, что и KPM. Нативный `kpm os-update` загружает до 64 MiB, проверяет canonical SHA-256/signature, повторно проверяет записанный bundle и сохраняет его в неактивный `/hdd/KSA` или `/hdd/KSB` slot. State file хранит active/pending slot, boot-attempt budget и release; доступны `os-confirm` и `os-rollback`. Registry публикует immutable `system/current.ksu`, release tool формирует bundle из kernel image.

**Не хватает:** переключения slot в Limine/EFI boot path, автоматического decrement boot-attempt counter, отдельной recovery environment, migrations и power-failure/runtime regression tests.

**Приоритет:** P0 для массового распространения, но архитектуру лучше учитывать заранее.

---

# Почему Installer / Updates / Recovery в конце

Да, твоя мысль в целом правильная.

### Installer
Его удобно делать **после** того, как уже стабильны:

`memory → processes → storage → GPT → filesystem → bootloader → users/security`

Иначе installer придётся переписывать каждый раз, когда меняется storage architecture.

### Updates
Их ещё позже, потому что update system требует стабильных:

`disk layout + boot chain + package/image format + signatures + recovery`

### Recovery
Фактически это последний safety net поверх всей предыдущей системы.

**Но тестировать аварийные сценарии надо раньше.** Например, ещё до готовой OTA-системы полезно иметь recovery test cases: corrupted filesystem, failed boot, damaged config, interrupted write.

---

# Desktop Migration Plan

## Да: `src/desktop.c` можно перенести в userspace и сохранить нынешний внешний вид

Но переносить файл **целиком один-в-один нельзя**.

Текущий `src/desktop.c` примерно на 2250 строк и смешивает в одном месте:

- desktop UI
- drawing
- mouse/keyboard drivers
- timer
- process spawning
- VFS launching
- window manager logic
- direct framebuffer assumptions
- hardware shutdown/reboot
- kernel-side app hooks
- compositor behavior

Для userspace нужно разделить эти обязанности.

## Целевая архитектура

```text
                    KeshOS Kernel
                         │
        ┌────────────────┼─────────────────┐
        │                │                 │
     display          input          process/syscalls
        │                │                 │
        └────────────────┼─────────────────┘
                         │
                 Window/Compositor API
                         │
                 ┌───────▼────────┐
                 │ Kesh Desktop   │  ← userspace
                 │ shell / WM     │
                 └───────┬────────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
       Files          Terminal       Settings
       Browser        Paint          Music ...
```

## Что оставить в kernel

- framebuffer/display access
- input device collection
- process isolation
- window surface allocation/mapping
- event delivery
- safe composition boundary
- shutdown/reboot syscall
- permissions/security enforcement

## Что вынести из `desktop.c` в userspace

- wallpaper
- top bar
- dock/taskbar
- start menu
- context menu
- volume popup UI
- clock presentation
- theme/accent rendering
- app launcher
- desktop icons
- settings presentation
- z-order policy, если решим держать window manager в userspace
- почти все `draw_*` UI primitives

## Как сохранить сегодняшний внешний вид

Не переписывать дизайн с нуля.

Переносить существующие алгоритмы и constants:

`render_layer_background()`
`render_desktop_icons()`
`render_start_menu()`
`render_volume_popup()`
`render_layer_topbar()`
`render_layer_taskbar()`
`render_desktop_context_menu()`
`dock magnification`
`blur/glass`
`theme/accent settings`

и заменить только зависимость от kernel APIs.

## Главные замены API

| Сейчас в `desktop.c` | Целевой userspace путь |
|---|---|
| `keyboard_poll_event()` | `kesh_poll_event()` |
| `poll_mouse()` / mouse driver | kernel input events |
| `timer_ticks()` / `timer_wait_ticks()` | `kesh_time()` + `kesh_sleep()` |
| `process_spawn_path()` | `kesh_exec()` |
| direct framebuffer access | desktop surface/window framebuffer |
| `uwindow_*` internals | public window/compositor API |
| direct RTC/port I/O | time syscall |
| direct shutdown I/O | dedicated shutdown/reboot syscall |
| kernel app hooks | normal userspace app launching |
| kernel globals for settings | `kesh_get_settings()` / `kesh_set_settings()` |

## Важнейший первый шаг

Не переносить сразу весь `desktop.c`.

Сначала нужен **нормальный Desktop Surface API**:

```c
int kesh_create_desktop_surface(int width, int height);
uint32_t* kesh_desktop_framebuffer(void);
int kesh_desktop_poll_event(kesh_event_t *ev);
void kesh_desktop_present(void);
int kesh_window_list(...);
int kesh_window_focus(...);
int kesh_window_move(...);
int kesh_window_close(...);
```

Точный API надо спроектировать после стабилизации `copy_from_user/copy_to_user`, потому что иначе мы перенесём Desktop на API, который потом придётся ломать ради безопасности.

## Практический порядок миграции

### Desktop Step 1
Сделать userspace library для:

- rectangles
- rounded rectangles
- alpha blending
- text
- icons
- blur/glass

У вас часть этих primitives уже есть в `userspace/lib/kesh.c`, поэтому это не надо начинать с нуля.

### Desktop Step 2
Добавить отдельный fullscreen desktop surface без прямого доступа userspace к физическому VRAM.

### Desktop Step 3
Перенести:

- wallpaper
- background
- icons
- top bar
- dock
- clock

### Desktop Step 4
Перенести:

- Start menu
- context menu
- volume UI
- settings UI
- app launcher

### Desktop Step 5
Убрать из Desktop прямой вызов `process_spawn_*` и запускать приложения через публичный userspace API.

### Desktop Step 6
Разделить kernel compositor и userspace window manager настолько, чтобы kernel отвечал за безопасность поверхности и доставку событий, а UI-логика жила снаружи.

### Desktop Step 7
Когда новый userspace Desktop визуально совпадёт с нынешним, отключить старый `src/desktop.c`.

---

# После backend foundation — предрелизный бэклог Desktop

Этот список начинается после ближайшего блока P0 из gates 6–10. Он не заменяет фундаментальные backend-задачи.

## Когда вернёмся к предрелизу

### A. [ ] Анимация перетаскивания окон в стиле KWin

Во время drag окно должно оставаться отзывчивым и плавно следовать за курсором, без рывков, старого следа или задержки в один кадр. Визуальная цель — аккуратное динамическое смещение содержимого окна, близкое по ощущению к KWin, но без копирования его кода или дизайна.

**Настройки:** добавить переключатель `Плавное перетаскивание окон` в существующий экран Settings. По умолчанию включён; при выключении остаётся нынешнее прямое перемещение без анимации.

**Критерии готовности:**

- работает для всех окон, которые обслуживает `kernel/uwindow.c`;
- не меняет координаты, фокус, z-order и обработку мыши;
- завершает анимацию сразу при отпускании кнопки;
- не оставляет артефактов при быстром drag;
- выбор из Settings применяется сразу и сохраняется после перезагрузки, когда появится постоянное хранилище настроек.

**Граница:** сначала реализовать для текущего native Desktop. При миграции Desktop в userspace перенести поведение через публичный Window/Compositor API.

### B. [ ] Проверка плавности и деградации desktop-анимаций

Нужен небольшой ручной regression checklist: open/close, drag, dock, Start menu, Genie и переключение всех animation settings. На слабой VM анимации должны либо оставаться плавными, либо отключаться настройкой без поломки управления окнами.

**Критерии готовности:** нет зависаний ввода, потери окон, визуального мусора и заметного роста CPU при простое Desktop.

## После предрелиза — запрос сообщества

### C. [ ] Пользовательская кастомизация как у Linux-дистрибутивов

Дать пользователю контролировать внешний вид и поведение системы без пересборки ОС. Это большой продуктовый слой, поэтому он идёт после стабильного Settings API и userspace Desktop migration.

Планировать по этапам:

1. тема: светлая/тёмная, акцентный цвет, обои, масштаб UI;
2. Desktop: размер и положение dock/taskbar, набор иконок, часы, поведение окон и анимаций;
3. профили и импорт/экспорт настроек;
4. темы/пакеты оформления с метаданными, версией и безопасной установкой через KPM;
5. публичные hooks/API для расширений только после sandbox и permission model.

**Граница безопасности:** темы и расширения не получают прямой доступ к kernel memory, framebuffer, портам ввода-вывода или правам root. Все изменения проходят через settings/theme API.

## Мои предложения для предрелиза

### D. [ ] Сохранение настроек Desktop

Выбор темы, обоев, громкости, поведения dock и анимаций должен храниться в одном versioned config-файле с безопасными значениями по умолчанию при ошибке чтения.

### E. [ ] Безопасный режим Desktop

При удержании выбранной клавиши при старте или при аварийном завершении предыдущей сессии загружать Desktop без сторонних тем и тяжёлых эффектов. Это даст пользователю способ восстановить интерфейс после неудачной настройки.

### F. [ ] Базовая доступность перед публичным релизом

Добавить крупный курсор, системный масштаб интерфейса и управление окнами с клавиатуры. Эти вещи полезнее многих декоративных эффектов и хорошо ложатся в будущий gate 40.

---

# Главное правило Desktop refactor

**Сначала parity, потом deletion.**

То есть:

```text
старый kernel Desktop
        │
        ├── работает
        │
        ▼
новый userspace Desktop
        │
        ├── визуально совпадает
        ├── mouse/keyboard совпадают
        ├── launch apps совпадает
        ├── settings совпадают
        └── window behavior совпадает
                │
                ▼
      удалить старый Desktop
```

Не удалять рабочий `desktop.c`, пока новый userspace Desktop не пройдет parity test.

---

# Следующая конкретная очередь

```text
NOW
 │
 ├─ 1. copy_from_user / copy_to_user
 ├─ 2. VM cleanup + unmap + free
 ├─ 3. NX / page permissions
 ├─ 4. process cleanup
 ├─ 5. threads + per-CPU
 ├─ 6. APIC/IOAPIC
 ├─ 7. SMP
 ├─ 8. block layer
 ├─ 9. GPT
 ├─10. AHCI/NVMe
 ├─11. USB/xHCI
 ├─12. fd + IPC + credentials
 ├─13. KEA permissions + sandbox
 ├─14. sockets
 ├─15. HTTPS trust store
 ├─16. userspace Desktop migration
 ├─17. package install/signatures/dependencies
 ├─18. installer
 └─19. update + recovery
                                                               LATER
```

> **Это порядок приоритетов, а не обещание, что каждый пункт делается строго один за другим.**

---

# Definition of Done для проекта

KeshOS не считается «готовой настоящей ОС» только потому, что загрузилась.

Минимальная планка для сильного первого стабильного релиза должна выглядеть так:

```text
✅ BIOS + UEFI
✅ Memory isolation
✅ safe user pointers
✅ process/thread lifecycle
✅ modern interrupts + SMP
✅ modern storage
✅ USB
✅ filesystem consistency
✅ real userspace API
✅ permissions/sandbox
✅ network + sockets
✅ trusted HTTPS
✅ working userspace Desktop
✅ package installation
✅ signed packages
✅ installer
✅ updates
✅ recovery
```

До этого момента новые фичи можно добавлять, но **фундаментальные P0 gaps имеют приоритет над косметикой**.

---

# Change Log

### 2026-09-27 — v2.0

- Перестроен список из 50 gate'ов в иерархический порядок разработки.
- BIOS и UEFI закреплены как выполненные по реальному тесту владельца.
- Полный HDD/persistent FAT32 path учтён.
- Основной `src/desktop.c` учитывается как рабочий native Desktop.
- Экспериментальный виртуальный/Ring3 desktop не используется как основной Desktop proof.
- KPM описан как реальный network downloader + registry path, но не как полностью готовый installer.
- Добавлена отдельная цель миграции основного Desktop из kernel space в userspace.
- Installer поставлен поздно, а updates/recovery оставлены последним большим слоем.
- Процент пересчитан по фактическим статусам этого файла.



### 2026-09-27 — Gates 6–10 implementation block

- Gate 6 PMM получил free/reuse allocator с split/coalesce и double-free/overlap protection; maturity and broader leak testing remain.
- Gate 7 VMM получил page flags, unmap and recursive user address-space destruction with page-table/resource cleanup.
- Gate 8 fault path was hardened by routing syscall user buffers through validated kernel-copy helpers; target runtime fault testing remains.
- Gate 9 safe user-copy layer is implemented and pointer-bearing syscalls were migrated to it.
- Gate 10 NXE/PTE NX and ELF W^X rejection are implemented, with final per-segment page permissions and NX user stack; full runtime/architectural verification remains.
- Ninja now owns an `iso` target. `build.bat` and `build-iso.ps1` regenerate Ninja before invoking `ninja iso`.
- Native build passes. ISO target wiring passes dry-run. Final ISO packaging is blocked in the current sandbox because `xorriso` is unavailable.


### 2026-09-27 — Gates 11–15 implementation pass

- Process cleanup promoted to `[x]` after unifying address-space/window/thread resource teardown.
- Added initial kernel/user thread subsystem with TIDs, dedicated stacks for extra threads and thread syscalls.
- Added per-CPU topology/bookkeeping and Limine SMP discovery.
- Added xAPIC/IOAPIC setup with MADT discovery and PIC fallback transition.
- Added AP bring-up callbacks through the Limine SMP interface.
- `ninja -j4` passed and `kernel.elf` linked.
- `ninja iso` could not complete because `xorriso` is unavailable in the sandbox; no ISO/QEMU runtime result is claimed.


### 2026-09-29 — Linux build + boot regression correction

- Identified a real regression path in the new APIC/IOAPIC transition: PIC was masked unconditionally after APIC initialization, so a failed/unsupported IOAPIC path could stop PIT timer IRQs and leave the graphical boot loop stuck.
- APIC initialization is now fail-safe and retains the legacy PIC/timer path unless APIC + IOAPIC setup is positively validated.
- IOAPIC routing uses the actual BSP LAPIC ID; Limine x2APIC mode is deferred until a dedicated x2APIC backend exists.
- Limine SMP AP `goto_address` startup is deliberately deferred until per-CPU GDT/TSS and scheduler state are ready. Gate 15 therefore remains `[~]` rather than being overstated as complete.
- Added `build.sh` for CachyOS/Linux, including `--install-deps`, cross-platform tool discovery, and a single `./build.sh` command for kernel/userspace + bootable ISO.
- Build wrappers and README documentation were updated accordingly.
- Native `ninja -j4` still passes. Sandbox ISO/QEMU runtime verification remains unavailable because `xorriso` and QEMU are not installed in the sandbox.

### 2026-09-29 — Gates 6–10 hardening pass

- VMM теперь отклоняет повторное mapping страницы и user mapping за пределами user virtual address space.
- Освобождение user address space сначала переключает CR3 на kernel page table, затем освобождает старые page tables.
- `copy_from_user()` и `copy_to_user()` дополнительно проверяют user permissions на всех уровнях page table.
- ELF loader принимает только поддерживаемый `ET_EXEC`, проверяет alignment program segments, entry point и отклоняет overlap страниц с несовместимыми правами. Это сохраняет W^X на уровне целой страницы.
- Каждое userspace-окно получает отдельный framebuffer virtual address range и NX mapping.
- Runtime-проверка в `memory_security_selftest()` теперь подтверждает, что VMM не разрешает повторный mapping и user mapping за `USER_VA_LIMIT`.
- `ninja build/kernel.elf`, `ninja iso` и `tools/verify_iso.py` прошли; целевой QEMU regression test остаётся обязательным.

### 2026-09-29 — VirtualBox backend regression

- Владелец проекта подтвердил успешную загрузку и работу Desktop в VirtualBox после memory/VMM/process hardening.
- Gates 6–8, 10 и 12–15 сохраняют статус `[~]`: для них нужны отдельные проверки allocator pressure, user-fault recovery, W^X, multithreading и SMP.

### 2026-09-29 — VirtualBox security/RNG regression

- Свежий `serial.log` подтверждает запуск актуального ядра: сообщение о fallback clock содержит новую проверку invariant TSC.
- `memory_security_selftest()` прошёл, тестовый Ring 3 процесс выполнил syscall и корректно вернулся в kernel mode.
- HMAC-DRBG успешно инициализирован с источником `RDSEED`; системный таймер продолжает работать по PIT, поскольку VirtualBox не предоставил частоту invariant TSC.
- Desktop продолжает отрисовку, а в логе нет `FATAL`, `PANIC`, `ERROR` или `FAIL`.
- Gate 25 переведён в `[~]`. Проверка прав KEA, `SYS_GETRANDOM` из приложения и остальные security gates пока не подтверждены отдельными сценариями.

### 2026-09-29 — Gates 26–35 foundation pass

- Добавлены service manager, TTY ring-buffer foundation, in-memory ring buffer kernel logs, ACPI table validation, generic ATA/ATAPI block API и read-only GPT parser.
- PCI теперь умеет безопасно обходить standard capability list; boot log отдельно сообщает об обнаруженном AHCI/NVMe controller, но драйверы для них ещё не реализованы.
- `ninja -j4 build/kernel.elf`, `ninja iso` и `tools/verify_iso.py` прошли. Нужен VirtualBox smoke test: boot должен дойти до Desktop с сообщениями `acpi`, `block` и `service` в `serial.log`.
- Gates 33 и 35 остаются `[ ]`: обнаружение AHCI/NVMe не является storage driver, а FAT32 write path ещё не имеет журнала или fsck.

### 2026-09-29 — Gates 36–45 network/input foundation pass

- USB discovery теперь находит именно PCI USB controllers и различает UHCI/OHCI/EHCI/xHCI по `prog_if`.
- PS/2 keyboard и mouse публикуют события через общий kernel input queue, сохраняя текущую desktop input path.
- Добавлен ограниченный userspace TCP socket API. В текущем сетевом backend одновременно может быть одно активное TCP connection; это ограничение намеренно зафиксировано до переработки TCP state model.
- TLS больше не строит entropy из TSC: используется kernel HMAC-DRBG, а при его недоступности HTTPS запрос отклоняется.
- `ninja -j4 build/kernel.elf`, `ninja iso` и `tools/verify_iso.py` прошли. Нужны VirtualBox smoke tests boot/input и отдельный network test для socket/TLS path.

### 2026-09-30 — Process API and sandbox foundation pass

- Реализованы per-process file descriptors с `open/close/read/write/seek` и cleanup на `exit/kill`.
- Реализованы process-owned IPC message queues с userspace send/receive API и cleanup при завершении процесса.
- Kernel credentials отделяют UID 0 от KEA metadata; `SYS_KILL` больше не может завершить чужой процесс без root credential.
- `ninja -j4 build/kernel.elf`, `ninja iso` и `tools/verify_iso.py` прошли.
- QEMU GUI attempts с `q35/qemu64` завершились до создания окна и без kernel serial output; это не засчитывается как runtime result. Нужна проверка образа в VirtualBox.

### 2026-09-30 — Package persistence and live installer pass

- KPM теперь проверяет downloaded KEA header/bounds/CRC и записывает пакет в `/hdd/apps` через VFS; cryptographic origin доверенным пока не считается, потому что signature/trust-key layer отсутствует.
- В ISO добавлено отдельное Ring 3 приложение `KeshInstaller`, доступное с главного Desktop. Оно ведёт через preflight и выбор filesystem.
- Для реально доступного FAT32 backend установщик по явной кнопке создаёт `/hdd/KESHOS/INSTALL.TXT` с persistent installation plan. Он не форматирует раздел и не меняет boot data.
- ext4 и Btrfs отражены в UI как недоступные до появления реальных драйверов форматирования и монтирования.
- GPT parser теперь до публикации partition entries проверяет также CRC всего entry array, а не только CRC заголовка.
- `python3 generate_ninja.py`, `ninja -j4 build/keshos.iso` и `python3 tools/verify_iso.py` прошли; ядро в образе имеет SHA-256 `90b5a0e39ed60d1a9842d806d36409452e115a2d69effb2458e49194bb5e5a34`.

### 2026-09-30 — Storage, display and installer expansion

- PCI API получил checked enable для MMIO/bus mastering; AHCI и NVMe controller probe использует его и логирует BAR before future command-driver initialization.
- Display layer очищает все framebuffer outputs, обнаруженные Limine, до старта primary Desktop.
- VFS теперь публикует все встроенные KEA applications, включая Settings, About и Browser.
- `KeshInstaller` копирует девять live KEA packages на выбранный FAT32 target после явного сохранения install plan. GPT, EFI partition, formatter и bootloader installation пока не реализованы.
- `python3 generate_ninja.py`, `ninja -j4 build/keshos.iso` и `python3 tools/verify_iso.py` прошли; проверка подтверждает актуальные kernel и installer внутри ISO.

### 2026-09-30 — Limine boot regression fix

- Исправлен boot-blocker в VirtualBox: `acpi.c` и `apic.c` одновременно объявляли одинаковый Limine RSDP request, поэтому Limine останавливался с `Conflict detected for request ID` до запуска ядра.
- Единственный RSDP request оставлен в ACPI subsystem. APIC теперь получает validated MADT через `acpi_find_table()`.
- `ninja -j4 build/keshos.iso` и `python3 tools/verify_iso.py` прошли. Нужен новый VirtualBox boot test этого ISO.

### 2026-09-30 — OS API follow-up

- File descriptors получили create/truncate/append semantics, POSIX-like `dup` с общим file offset и отклоняют path, который не помещается в FD table.
- IPC сохраняет PID отправителя последнего принятого сообщения для receiver process.
- Userspace получил read-only credentials API; DRBG получает hardware reseed после каждого 1 MiB output.
- Kernel log ring доступен приложениям с FS capability через bounded `kesh_diagnostics_read()` syscall.
- Эти изменения расширяют gates 19, 20, 21 и 25, но не закрывают их: pipes, account database и entropy pool ещё отсутствуют.

### 2026-09-30 — Visual system and icon pass

- Добавлен ICO engine для 32-bit bitmap ICO: native applications загружают и рисуют иконки с alpha из VFS.
- В `/icons` опубликованы двенадцать read-only ICO файлов. Три пользовательских исходника из `ico/*.png` сконвертированы в ICO; для остальных приложений создан единый нейтральный набор.
- Boot Desktop, Dock, Shell, Explorer и Notepad больше не используют старые текстовые теги или цветные плитки для своих обновлённых иконок.
- Settings получил темы Paper, Slate и Linen, а также настройки motion, скорости переходов, плотности интерфейса и отображения секунд. Выбранная тема сохраняется как системная настройка.
- `ninja -j4 build/keshos.iso` и `tools/verify_iso.py` прошли. Headless QEMU boot дошёл до `Native Desktop Active`; в serial log нет `PANIC`, `FATAL` или `ERROR`.

### 2026-09-30 — Memory, time, ACPI and AHCI implementation pass

- PMM free-range metadata получил IRQ-safe SMP lock; clearing allocated memory no longer holds the allocator lock.
- Userspace ABI получил anonymous `VM_MAP` / `VM_UNMAP` regions с process ownership, NX/W^X и rollback.
- Timekeeping получил monotonic/realtime nanosecond API; realtime инициализируется из CMOS RTC.
- ACPI получил FADT/DSDT `_S5_` shutdown и reset-register reboot path; Desktop пробует ACPI shutdown до emulator fallbacks.
- AHCI SATA диски регистрируются в block layer и получают LBA48 DMA read/write path.
- NVMe получил polling admin/I/O queues, Identify Controller/Namespace и PRP read/write с регистрацией namespace в block layer.
- FAT32 переведён с прямых ATA calls на generic block I/O; VFS может монтировать FAT32 с MBR/GPT на ATA, AHCI или NVMe device.
- По прямой договорённости с владельцем сборка и runtime-тесты этого pass не запускались.

### 2026-09-30 — Signed KPM, IPC and controller foundation pass

- KEA v2 получил canonical SHA-256 и ECDSA P-256 signature; kernel сверяет package с pinned repository key до запуска payload. Legacy KEA читаются для совместимости, но не получают root capability.
- KPM получил dependency records/minimum versions, recursive install, локальную БД, dependency-aware remove, transaction journal, backup и recovery. Windows CLI скачивает во временный файл и публикует его только после той же cryptographic verification.
- Registry переведён на versioned immutable tree и индекс, генерируемый только из проверенных пакетов. API нормализует id/version, блокирует path traversal и сверяет размер артефакта.
- Приватный signing key создан вне workspace в `~/.config/keshos/kpm-root-private.json` с правами `0600`; в проекте находятся только public trust anchor и release tooling.
- FD/IPC layer получил byte pipes, `dup`-aware lifetime, immediate readiness/hangup poll и userspace ABI.
- PCI получил MSI programming API; xHCI discovery теперь выполняет bounded controller reset и root-port scan. FAT32/VFS получили same-directory rename primitive.
- Gate 48 переведён из `[ ]` в `[~]`; точный пересчёт заголовков даёт 7 завершённых, 41 частичный и 2 отсутствующих gate, то есть 59.1%. До 80% честно не дотягивает: оставшиеся gates требуют значительно более крупных подсистем и runtime-подтверждения.
- По прямой договорённости с владельцем сборка, syntax checks и runtime-тесты этого pass не запускались.

### 2026-09-30 — Signed A/B update foundation

- Добавлен KSU v1 system-update container с pinned ECDSA P-256 signature и canonical SHA-256.
- KPM получил команды `os-update`, `os-confirm` и `os-rollback`; stage transaction повторно проверяет записанный bundle и ведёт active/pending slot state с boot-attempt budget.
- Registry получил `/api/system-update`; текущий development kernel опубликован как подписанный `system/current.ksu` размером 37 799 752 bytes.
- Gate 50 переведён из `[ ]` в `[~]`; точный пересчёт составляет 7 завершённых, 42 частичных и 1 отсутствующий gate, или 60.2%.
- Загрузчик ещё не выбирает pending slot, поэтому автоматический boot rollback пока не считается реализованным. Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — PTY and controlling-terminal foundation

- Добавлены PTY master/slave pairs с отдельными process owners и автоматическим cleanup при exit.
- Master поддерживает canonical/raw input, echo, erase и EOF; slave output возвращается master endpoint.
- Shell-like owner может прикрепить slave к существующему процессу, после чего процесс получает его через `kesh_pty_current()` и использует userspace read/write API.
- Gate 27 остаётся `[~]`: для завершения нужны standard-FD redirection, blocking wakeups, process groups, job control и signals.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — Process groups and terminal job control

- Process model получил process-group identity и ограниченные kernel actions `INT/KILL/TERM/STOP/CONT` с остановкой/возобновлением scheduler eligibility.
- PTY хранит foreground process group; attach автоматически выбирает группу slave process.
- Canonical master преобразует `Ctrl-C` в `SIGINT`, `Ctrl-Z` в `SIGSTOP`; authenticated master API меняет foreground group и посылает `CONT/TERM/KILL`.
- Userspace ABI получил `getpgid/setpgid`, PTY foreground get/set и foreground signal API.
- Gates 20 и 27 остаются `[~]`: отсутствуют userspace handlers/masks, blocking waits, standard-FD redirection и полная background terminal policy.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — PTY stdio bridge

- FD object model получил PTY descriptor kind с read/write/poll/dup/close и cleanup semantics.
- PTY attach атомарно резервирует у slave process fd 0/1/2 как ссылки на один controlling-terminal object; ручной `kesh_pty_open_fd()` сохранён для нестандартной раскладки.
- Это связывает PTY/job-control path с существующим userspace file-descriptor ABI без выдачи master endpoint дочернему процессу.
- Gates 19 и 27 остаются `[~]`: generic exec inheritance/redirection и blocking waits ещё не реализованы.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — FAT32 dirty recovery and fsck

- FAT32 теперь выставляет стандартный dirty bit на mount и clean-shutdown bit только после синхронизации FSInfo при штатном выключении.
- Dirty mount автоматически запускает обход всего достижимого directory tree, исправляет оборванные и циклические FAT chains и освобождает недостижимые orphan clusters.
- Repair pass заново считает свободные кластеры и публикует результат в FSInfo; для ручного запуска добавлена команда Terminal `fsck`.
- Gate 35 остаётся `[~]`: полноценного metadata journal и failure-injection verification ещё нет.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — PMM fragmentation and diagnostics pass

- Contiguous PMM allocation переведён на best-fit, а освобождение продолжает сливать соседние диапазоны и отклонять overlap/double-free.
- Добавлены peak usage, allocation/free/failure counters, exact free bytes, largest contiguous run и количество свободных диапазонов.
- Terminal получил команду `mem` для быстрой проверки свободной и крупнейшей непрерывной памяти.
- Gate 6 остаётся `[~]`: per-CPU caches, ownership tracing и stress verification ещё не реализованы.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — Persistent boot diagnostics

- Штатное выключение сохраняет memory log ring в FAT32 до финальной отметки clean shutdown.
- Следующий boot загружает предыдущий журнал отдельно от текущего ring; Terminal получил команду `lastlog`.
- Gate 28 остаётся `[~]`: panic-time persistence, module filtering, rotation и crash unwinding ещё не реализованы.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — Directory file descriptors

- FD object model получил отдельный read-only directory kind, открываемый флагом `KESH_FD_DIRECTORY`.
- Новый syscall/userspace API `kesh_readdir()` возвращает по одной directory entry и сохраняет позицию в общем объекте, поэтому `dup` разделяет directory offset.
- Directory descriptors интегрированы в close, process cleanup и readiness polling.
- Gate 19 остаётся `[~]`: generic exec inheritance/redirection, blocking poll/select и полные concurrency semantics ещё не готовы.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — UDP socket path

- Network stack получил bounded generic UDP transmit и validated inbound datagram dispatch рядом с DHCP/DNS consumers.
- Process-owned sockets поддерживают UDP, local-port `bind`, connected peer, ephemeral ports и отдельный receive buffer на handle.
- Userspace ABI получил `KESH_SOCKET_UDP`, `kesh_socket_bind`, `kesh_socket_sendto` и `kesh_socket_recvfrom`; существующие connect/send/recv/close работают для обоих transport protocols.
- Gate 44 остаётся `[~]`: TCP server API, blocking wakeups, multi-datagram queues и multi-connection TCP engine ещё не готовы.
- IPv4 ingress одновременно усилен проверкой checksum/length/fragment state до передачи пакета protocol handlers.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — Deterministic FD redirection

- Добавлен POSIX-like `dup2` в kernel FD table и userspace ABI: target descriptor атомарно заменяется ссылкой на тот же shared object/offset.
- Это позволяет явно переназначать stdin/stdout/stderr поверх file, pipe или PTY objects; ownership cleanup и reference counting остаются общими.
- Gates 19 и 27 остаются `[~]`: descriptor inheritance через generic exec и blocking wait policy ещё отсутствуют.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — CSPRNG entropy pool and health checks

- RDSEED/RDRAND проходят continuous repetition и adaptive per-bit startup tests до создания HMAC-DRBG.
- Добавлен SHA-256 entropy pool: timestamped keyboard/pointer events получают консервативный credit и вызывают DRBG reseed при 256 bits.
- Reseed допускает hardware entropy, накопленный pool или оба источника; доступны diagnostic counters генерации, reseed и health failures.
- Gate 25 остаётся `[~]`: нет persistent boot seed, второго platform source и runtime fault-injection verification.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — AC97 DMA and audio permission path

- AC97 BDL и PCM buffer переведены на PMM-backed DMA pages; hardware получает проверенные 32-bit physical addresses вместо kernel virtual pointers.
- Sound manager инициализируется после PCI discovery и не выдаёт HDA за playback-ready, пока stream engine отсутствует.
- Добавлены SOUND-capability syscalls для device query, volume и bounded PCM16 playback; HDA MMIO probe использует HHDM mapping.
- Gate 38 остаётся `[~]`: HDA stream engine, mixer/resampler, interrupt refill и runtime verification ещё не готовы.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — PCI BAR resources and MSI-X

- PCI API получил безопасное BAR size probing с сохранением command register и исходных BAR values, включая 64-bit MMIO resources.
- Resource descriptor публикует address, size, I/O-vs-MMIO, prefetchability и 64-bit width для будущего allocator/driver binding.
- Добавлено MSI-X table programming с bounds check, vector mask/unmask, APIC destination и disable API; существующий MSI path сохранён.
- Gate 29 остаётся `[~]`: ECAM, централизованный resource allocator, hotplug и generic driver binding ещё отсутствуют.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — ISO9660 generic block migration

- ISO9660 больше не включает ATA API и не ищет ATA drive slots напрямую.
- Optical mount discovery выбирает read-only block devices с 2048-byte sectors; volume, directory и file reads идут через generic `block_read` с его range checks.
- Gates 31 и 34 остаются `[~]`, но filesystem-to-physical-storage abstraction теперь едина для FAT32 и ISO9660.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — Typed IPC and SMP-safe queues

- IPC messages получили 16-bit type, а receive — независимые sender/type filters и возврат фактической metadata через userspace descriptor.
- Send отклоняет несуществующие target processes; старые IPC syscalls сохранены и отображаются на type 0.
- Все queue mutation и `ipc_last_sender` защищены IRQ-safe atomic spinlock для SMP execution.
- Gate 20 остаётся `[~]`: shared memory, blocking wakeups, userspace signal handlers и единая handle model ещё не готовы.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — ACPI PM timer calibration

- FADT parser получил legacy и extended GAS access к ACPI PM timer с корректной 24/32-bit маской счётчика.
- Invariant TSC без CPUID frequency теперь калибруется по стандартным 3.579545 MHz PM timer ticks; monotonic clock сохраняет непрерывность при переключении источника.
- Gates 16 и 30 остаются `[~]`: per-CPU timers, correction policy, suspend/resume, thermal/battery и полноценный AML ещё отсутствуют.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — PCIe ECAM configuration backend

- PCI layer читает ACPI MCFG, валидирует alignment/ranges и использует ECAM MMIO для доступных segment-0 bus ranges с прозрачным CF8/CFC fallback.
- Legacy и ECAM configuration accesses защищены общим IRQ-safe spinlock; выбранный backend публикуется boot diagnostics.
- Gate 29 остаётся `[~]`: multi-segment extended configuration API, resource allocator, hotplug и generic driver binding ещё отсутствуют.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — 70-percent foundation implementation pass

- VMM получил централизованный user W^X invariant; static ELF loader теперь отклоняет unsupported ABI, dynamic/TLS payloads, overlaps, stack collisions, malformed flags и entry вне executable segment.
- Syscall ABI публикует version/features/max syscall, ограничивает неизвестные номера и исправляет bounded process-table copy; process spawn больше не меняет identity caller внутри exec.
- IOAPIC route активируется с MADT override handling и автоматически возвращается на PIC, если PIT probe не подтверждает interrupt delivery.
- Exec наследует file/pipe descriptors; signed ROOT KEA получает UID 0, а audited root API умеет отзывать capabilities.
- CSPRNG получил persistent boot seed rotation, service manager — supervised userspace daemons с heartbeat/backoff, logging — severity/module filters и четыре поколения журналов.
- Block layer получил per-device serialization и hardware flush для ATA, AHCI и NVMe; FAT32 clean shutdown завершает clean-bit transaction flush-командой.
- Gates оставлены `[~]` до проверки владельцем нового boot/runtime path и завершения остающихся требований; процент искусственно не повышался.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-01 — Trusted HTTPS and persistent service manifests

- Удалён permissive X.509 callback: HTTPS использует BearSSL chain/hostname/validity verification с системным ISRG Root X1 и fail-closed RTC policy.
- TLS entropy buffer очищается после передачи движку, а BearSSL validation error сохраняется в diagnostics.
- Service manager загружает bounded декларативный userspace manifest после старта VFS, применяет heartbeat/restart limits и распространяет runtime failure зависимости на daemon.
- Boot memory security self-test проверяет централизованный запрет user W+X mapping.
- Gates остаются `[~]` до проверки владельцем и реализации CA update path, execute-fault NX test и оставшихся production requirements.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-03 — Signed CA updates and stack ASLR

- CA trust store получил подписанный `CASTORE.BIN`, SHA-256/ECDSA verification через pinned KPM key, generation policy, встроенный fallback и root-only hot reload syscall.
- Добавлен host tool `tools/ca-store.py`, создающий update image из RSA PEM/DER certificate и KPM signing key.
- Static ELF loader рандомизирует положение 64 KiB user stack в отдельном диапазоне и возвращает фактический stack top process layer.
- Gates 17, 31 и 45 были закрыты кодом; первоначальный пересчёт этого прохода составлял 62.9%.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-03 — Explorer stability rollback and CPU fallback

- Рандомизация user stack временно откачена после сообщения владельца о немедленном закрытии Dolphin; сохранены NX, W^X и строгая ELF validation.
- About теперь использует CPUID vendor string, если extended CPU brand leaf существует, но возвращает пустую строку.
- Gate 17 возвращён в `[~]`; честный пересчёт составляет 9 завершённых, 40 частичных и 1 отсутствующий gate, или 62.0%.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.

### 2026-10-03 — Explorer resume and CPU identification fixes

- `serial.log` локализовал падение Dolphin в `ExplorerApp::render`: после scheduler resume указатель приложения в callee-saved регистре возвращался нулевым.
- Dolphin теперь перезагружает адрес своего `ExplorerApp` из памяти на каждой итерации, поэтому отрисовка не зависит от сохранности указателя между yield.
- About получает очищенные CPUID brand/vendor/family/model/stepping через новый kernel syscall вместо ненадёжного прямого заполнения буфера в Ring 3.
- Сборка, syntax checks и runtime-тесты по договорённости не запускались.
