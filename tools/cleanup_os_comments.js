const fs = require('fs');
const path = require('path');

// Dictionary of custom lowercase slang comments per file
const COMMENTS_MAP = {
  // Kernel core
  'kernel/kernel.c': '// старт ядра, инит всего железа по порядку',
  'kernel/gdt.c': '// гдт дескрипторы для ядра и юзерленда',
  'kernel/gdt.h': '// селекторы сегментов',
  'kernel/syscall.c': '// быстрые сисколлы через msr',
  'kernel/syscall.h': '// номера сисколлов',
  'kernel/memory.c': '// раскидываем физическую память и страницы',
  'kernel/memory.h': '// структуры страниц и аллокатора',
  'kernel/process.c': '// шедулер процессов и переключение контекста',
  'kernel/process.h': '// структуры процессов и потоков',
  'kernel/elf.c': '// парсер эльфов чтоб бинарники запускать',
  'kernel/elf.h': '// структуры заголовков эльфа',
  'kernel/uwindow.c': '// композитор окон, рисуем буферы',
  'kernel/uwindow.h': '// дескрипторы окон',
  'kernel/vfs.c': '// виртуальная файловая система, монтирование',
  'kernel/vfs.h': '// ноды и дескрипторы вфс',
  'kernel/panic.c': '// кернел паника если все легло',
  'kernel/timer.c': '// таймер тикает и ладно',
  'kernel/timer.h': '// апишка таймера',
  'kernel/include/graphics.c': '// базовый вывод в фреймбуфер',
  'kernel/include/graphics.h': '// примитивы графики',
  'kernel/include/idt.c': '// таблица прерываний idt чтоб не крашилось',
  'kernel/include/idt.h': '// структуры idt',
  'kernel/include/pic.c': '// контроллер pic8259',
  'kernel/include/pic.h': '// порты pic',
  'kernel/resources.asm': '; встроенные ресурсы',
  'kernel/isr_stubs.asm': '; стабы прерываний, держим стек ровным',

  // Drivers
  'src/drivers/system/keyboard.c': '// клава ps/2, считываем сканкоды',
  'src/drivers/system/keyboard.h': '// маппинг клавиш',
  'src/drivers/system/mouse.c': '// мышка ps/2, двигаем курсор',
  'src/drivers/system/mouse.h': '// состояние мыши',
  'src/drivers/system/ata.c': '// читаем и пишем сектора на диск',
  'src/drivers/system/ata.h': '// команды ata контроллера',
  'src/drivers/system/fat32.c': '// парсер фат32 чтоб файлы открывать',
  'src/drivers/system/fat32.h': '// структуры fat32',
  'src/drivers/system/iso9660.c': '// драйвер сидюка iso9660',
  'src/drivers/system/iso9660.h': '// структуры iso',
  'src/drivers/system/sound_manager.c': '// звуковой микшер чисто чтоб играло',
  'src/drivers/system/sound_manager.h': '// апишка звука',
  'src/drivers/system/ac97.c': '// кодек ac97',
  'src/drivers/system/ac97.h': '// регистры ac97',
  'src/drivers/system/hda.c': '// чип intel hda',
  'src/drivers/system/hda.h': '// регистры hda',
  'src/drivers/net/e1000.c': '// сетевуха intel e1000',
  'src/drivers/net/e1000.h': '// регистры e1000',
  'src/drivers/net/net_stack.c': '// сетевой стек tcp/ip, пока пилится',
  'src/drivers/net/net_stack.h': '// протоколы сети',
  'src/drivers/net/tls/kesh_tls.c': '// тлс шифрование пакетов',
  'src/drivers/net/tls/kesh_tls.h': '// хедера тлс',

  // Desktop & GUI
  'src/desktop.c': '// рабочий стол, таскбар и отрисовка окон',
  'src/graphics.c': '// низкоуровневая графика',
  'src/gui/desktop.h': '// стейт рабочего стола',
  'src/gui/font.c': '// встроенный растровый шрифт',
  'src/gui/font.h': '// глифы шрифта',
  'src/gui/bmp_loader.c': '// загрузчик bmp картинок',
  'src/gui/bmp_loader.h': '// формат bmp',
  'src/gui/anim/genie_anim.c': '// анимация джинна как на маке',
  'src/gui/anim/genie_anim.h': '// стейт джинна',
  'src/gui/anim/win_chrome.c': '// заголовок окна и кнопки закрытия',
  'src/gui/anim/win_chrome.h': '// оформление окон',
  'src/gui/cursor/cursor.c': '// отрисовка курсора мыши',
  'src/gui/cursor/cursor.h': '// курсор',
  'src/gui/apps/file/file_manager.c': '// файловый менеджер проводник',
  'src/gui/apps/file/file_manager.h': '// стейт проводника',
  'src/gui/apps/music/music_app.c': '// плеер для wav треков',
  'src/gui/apps/music/music_app.h': '// плеер',
  'src/gui/apps/settings/settings_app.c': '// настройки системы',
  'src/gui/apps/settings/settings_app.h': '// настройки',
  'src/gui/apps/terminal/terminal_app.c': '// встроенный терминал',
  'src/gui/apps/terminal/terminal_app.h': '// терминал',
  'src/gui/apps/calc/calc_app.c': '// калькулятор',
  'src/gui/apps/calc/calc_app.h': '// калькулятор',
  'src/gui/apps/calc/calc_logic.c': '// логика вычислений',
  'src/gui/apps/calc/calc_state.h': '// стейт калькулятора',
  'src/gui/apps/about/about_app.c': '// окно о системе',
  'src/gui/apps/about/about_app.h': '// о системе',

  // Userspace apps & lib
  'userspace/lib/crt0.asm': '; входная точка программ',
  'userspace/lib/kesh.c': '// либка для юзерленда, обертки над сисколлами',
  'userspace/apps/explorer/main.cpp': '// проводник на c++',
  'userspace/apps/notepad/main.c': '// блокнот для заметок',
  'userspace/apps/paint/main.c': '// рисовалка',
  'userspace/apps/shell/main.c': '// командный шелл',
  'userspace/apps/taskmgr/main.c': '// диспетчер задач, мониторим процессы',
  'userspace/include/kesh.h': '// апишка keshos для приложений',
  'userspace/include/kea.h': '// формат кеа пакетов',
  'include/kesh/kea.h': '// спецификация kea',
  'kpm/kpm_os.c': '// пакетный менеджер kpm',
  'boot/loading/load_logo.c': '// лого при загрузке',
};

// C/C++ comment stripper that protects string/char literals
function stripCComments(code) {
  let result = '';
  let i = 0;
  const n = code.length;

  while (i < n) {
    // String literal
    if (code[i] === '"') {
      result += code[i++];
      while (i < n && code[i] !== '"') {
        if (code[i] === '\\') {
          result += code[i++];
        }
        if (i < n) result += code[i++];
      }
      if (i < n) result += code[i++];
    }
    // Char literal
    else if (code[i] === "'") {
      result += code[i++];
      while (i < n && code[i] !== "'") {
        if (code[i] === '\\') {
          result += code[i++];
        }
        if (i < n) result += code[i++];
      }
      if (i < n) result += code[i++];
    }
    // Block comment /* ... */
    else if (code[i] === '/' && code[i + 1] === '*') {
      i += 2;
      while (i < n && !(code[i] === '*' && code[i + 1] === '/')) {
        i++;
      }
      i += 2;
    }
    // Line comment // ...
    else if (code[i] === '/' && code[i + 1] === '/') {
      i += 2;
      while (i < n && code[i] !== '\n') {
        i++;
      }
    }
    else {
      result += code[i++];
    }
  }

  return cleanBlankLines(result);
}

// Assembly comment stripper
function stripAsmComments(code) {
  const lines = code.split('\n');
  const cleaned = [];
  for (let line of lines) {
    // If line has ;, remove comment part unless in quote
    let inQuote = false;
    let commentIdx = -1;
    for (let i = 0; i < line.length; i++) {
      if (line[i] === '"' || line[i] === "'") inQuote = !inQuote;
      if (!inQuote && (line[i] === ';' || (line[i] === '/' && line[i + 1] === '/'))) {
        commentIdx = i;
        break;
      }
    }
    if (commentIdx !== -1) {
      line = line.substring(0, commentIdx);
    }
    cleaned.push(line);
  }
  return cleanBlankLines(cleaned.join('\n'));
}

function cleanBlankLines(text) {
  const lines = text.split('\n');
  const out = [];
  let blankCount = 0;
  for (const l of lines) {
    const trimmed = l.trim();
    if (trimmed.length === 0) {
      blankCount++;
      if (blankCount <= 1) out.push('');
    } else {
      blankCount = 0;
      out.push(l);
    }
  }
  return out.join('\n').trim() + '\n';
}

function processFile(filePath) {
  const normalized = filePath.replace(/\\/g, '/');
  if (!fs.existsSync(filePath)) return;

  const content = fs.readFileSync(filePath, 'utf8');
  const isAsm = /\.(asm|S|s)$/i.test(filePath);
  let stripped = isAsm ? stripAsmComments(content) : stripCComments(content);

  // Determine comment to add
  let newComment = COMMENTS_MAP[normalized];
  if (!newComment) {
    const base = path.basename(filePath, path.extname(filePath));
    newComment = isAsm ? `; тут ${base.toLowerCase()}` : `// ${base.toLowerCase()}`;
  }

  // Insert comment at top of file
  const finalCode = newComment + '\n' + stripped;
  fs.writeFileSync(filePath, finalCode, 'utf8');
  console.log(`Cleaned: ${normalized} -> [${newComment}]`);
}

// Find all candidate files
function walk(dir) {
  let res = [];
  if (!fs.existsSync(dir)) return res;
  for (const item of fs.readdirSync(dir)) {
    const full = path.join(dir, item);
    if (fs.statSync(full).isDirectory()) {
      if (['doom', 'bearssl', 'node_modules', '.git', 'build', 'dist', 'website', 'fs_include', 'engine'].includes(item)) continue;
      res = res.concat(walk(full));
    } else {
      const norm = full.replace(/\\/g, '/');
      if (/\.(c|h|cpp|asm|S|s)$/i.test(item) && !norm.includes('limine.h')) {
        res.push(full);
      }
    }
  }
  return res;
}

const targetDirs = ['kernel', 'src', 'include', 'userspace', 'kpm'];
const allFiles = targetDirs.flatMap(walk);

console.log(`Processing ${allFiles.length} files...`);
allFiles.forEach(processFile);
console.log('Done!');
