import React, { useState } from 'react';
import type { PageTab } from '../components/Navigation';
import screenDesktop from '../assets/stol.png';
import screenOobe from '../assets/oobe.png';
import screenStartmenu from '../assets/pusk.png';
import screenExplorer from '../assets/explorer.png';
import screenBootloader from '../assets/freeldr.png';

interface HomePageProps {
  setActiveTab: (tab: PageTab) => void;
}

export const HomePage: React.FC<HomePageProps> = ({ setActiveTab }) => {
  const [selectedPhoto, setSelectedPhoto] = useState<string | null>(null);

  // Gallery slots strictly for the current OS (KeshOS Brownie 0.8.2)
  const brownieGallerySlots = [
    {
      id: 'slot_desktop',
      title: '1. Рабочий стол KeshOS Brownie',
      desc: 'Главный рабочий стол: панель задач, фирменные шоколадные обои и ярлыки рабочего пространства.',
      targetFile: 'screen_desktop.png',
      previewSrc: screenDesktop,
      hasImage: true,
    },
    {
      id: 'slot_oobe',
      title: '2. Мастер настройки OOBE',
      desc: 'Пошаговый конфигуратор первого запуска: настройка сети, учетной записи и параметров темы.',
      targetFile: 'screen_oobe.png',
      previewSrc: screenOobe,
      hasImage: true,
    },
    {
      id: 'slot_startmenu',
      title: '3. Меню «Пуск» и программы',
      desc: 'Классическое двухуровневое меню "Пуск", быстрый доступ к утилитам и панели управления.',
      targetFile: 'screen_startmenu.png',
      previewSrc: screenStartmenu,
      hasImage: true,
    },
    {
      id: 'slot_explorer',
      title: '4. Проводник «Мой компьютер»',
      desc: 'Управление локальными дисками, дискетами и съемными USB-накопителями в окне проводника.',
      targetFile: 'screen_explorer.png',
      previewSrc: screenExplorer,
      hasImage: true,
    },
    {
      id: 'slot_real_pc',
      title: '5. Запуск на реальном ноутбуке',
      desc: 'Фотография экрана физического компьютера с запущенной операционной системой KeshOS.',
      targetFile: 'screen_real_pc.png',
      previewSrc: '/images/screen_real_pc.png',
      hasImage: false,
    },
    {
      id: 'slot_bootloader',
      title: '6. Меню загрузчика FreeLoader',
      desc: 'Экран выбора вариантов запуска при включении ПК (RAM Boot / Setup / Screen Debug).',
      targetFile: 'screen_bootloader.png',
      previewSrc: screenBootloader,
      hasImage: true,
    },
  ];

  return (
    <div className="ms-page-home">
      {/* Hero Banner */}
      <div className="ms-hero-card">
        <div style={{ flex: 1 }}>
          <div className="ms-hero-tagline">ПРОЕКТ SNEAKDEAK TECHNOLOGIES • КОДОВОЕ НАЗВАНИЕ: "BROWNIE"</div>
          <h1>KeshOS® Workstation 0.8.2</h1>
          <p className="ms-hero-desc">
            Быстрая, компактная операционная система с душой классики и поддержкой Win32-приложений. 
            Кодовое название текущего выпуска — <strong>«Brownie»</strong>. 
            Единый универсальный гибридный образ BootCD объединяет запуск прямо в оперативную память (RAM Boot) и полную чистую установку на жесткий диск.
          </p>
          <div className="ms-hero-action">
            <button
              className="ms-hero-btn"
              onClick={() => setActiveTab('downloads')}
              type="button"
            >
              Скачать KeshOS BootCD (389 MB)
            </button>
            <a
              href="#history"
              className="win-btn"
              style={{ padding: '6px 14px', textDecoration: 'none', display: 'inline-flex', alignItems: 'center' }}
            >
              Наша история &gt;&gt;
            </a>
          </div>
        </div>

        <div style={{ textAlign: 'center', flexShrink: 0 }}>
          <img
            src={screenDesktop}
            alt="KeshOS Desktop"
            style={{
              width: '240px',
              border: '2px solid #ffffff',
              boxShadow: '0 4px 10px rgba(0,0,0,0.5)',
              borderRadius: '2px',
            }}
            onError={(e) => {
              (e.currentTarget as HTMLImageElement).src = '/images/keshos_logo.png';
            }}
          />
          <div style={{ fontSize: '9px', color: '#ffde7a', marginTop: '4px' }}>
            KeshOS (Кодовое название: Brownie)
          </div>
        </div>
      </div>

      {/* 1. Project History Section (BEFORE the photos) */}
      <div id="history" className="ms-section-header">
        <div className="ms-section-title">
          <span>Наша история: Как рождался KeshOS и кодовое имя «Brownie»</span>
        </div>
        <span style={{ fontSize: '10px', color: '#666' }}>Искренний рассказ от лица создателей</span>
      </div>

      {/* Warm human letter / memo */}
      <div className="ms-story-letter">
        <p>
          <strong>Привет всем, кто заглянул на наш сайт!</strong>
        </p>
        <p>
          Всё началось с простого юношеского желания: сделать собственную операционную систему, которой будет приятно пользоваться нам самим. 
          Без сотен назойливых фоновых процессов, без гигабайтов скрытой телеметрии, без навязанных онлайн-аккаунтов и тяжелых, бездушных интерфейсов. 
          Мы хотели вернуть то самое незабываемое ощущение рубежа 2000-х, когда компьютер слушался тебя с полуслова и был твоим личным верным инструментом, а не витриной для рекламы.
        </p>
        <blockquote>
          «Путь от первой строчки кода до загрузки рабочего стола на реальном компьютере оказался в сотни раз сложнее, чем мы представляли. Но именно трудности закалили наш проект.»
        </blockquote>
        <p>
          Сначала мы с горящими глазами пытались написать всё с чистого листа на <strong>Rust</strong>. Казалось: современный язык, безопасная память, никаких утечек! 
          Мы ночами читали спецификации OSDev, ковыряли ассемблерные загрузчики, настраивали страничную адресацию и обработку прерываний. 
          Но реальность быстро остудила пыл: написать ядро — это лишь 5% всей задачи. Чтобы в системе можно было слушать музыку, смотреть файлы и играть в игры, нужны тысячи совместимых драйверов для видеокарт, звуковых чипов, контроллеров USB и накопителей. В одиночку написать такую экосистему с нуля заняло бы десятилетия.
        </p>
        <p>
          Затем мы попытались взять за основу <strong>SayoriOS</strong>. Надеялись, что чужой готовый фундамент поможет быстрее увидеть работающий интерфейс. 
          Но на практике началась бесконечная война с чужими багами, архитектурными тупиками и странными падениями ядра, которые никто не мог объяснить. 
          Были моменты, когда руки опускались: система упорно падала в тройную перезагрузку или выдавала синий экран смерти прямо на старте.
        </p>
        <p>
          Переломным и самым счастливым моментом стал тот вечер, когда сетевой стек впервые ожил, и в окне терминала пролетели четыре заветные строчки: 
          <code>Reply from 8.8.8.8: bytes=32 time=18ms TTL=118</code>. Мы поняли: система дышит, она умеет общаться с внешним миром!
        </p>
        <p>
          Тогда и пришло зрелое инженерное решение: опереться на великую, проверенную тридцатью годами архитектуру <strong>Windows NT (на базе технологий ReactOS)</strong>. 
          Это открыло прямую совместимость с классическими программами Win32, богатую базу драйверов и любимый проводник Explorer. 
          А кодовое имя <strong>«Brownie»</strong> мы выбрали потому, что хотели подарить людям тепло и уют: благородные кофейно-шоколадные оттенки, как чашка горячего кофе и свежий брауни прохладной ночью, напоминают о том, что технологии должны радовать человека.
        </p>
      </div>

      {/* Timeline milestones */}
      <div className="ms-timeline">
        {/* Milestone 1 */}
        <div className="ms-timeline-item">
          <div className="ms-timeline-header">
            <span className="ms-timeline-title">1. Мечта о микроядре на Rust</span>
            <span className="ms-timeline-badge">ЭТАП 1 • 2024–2025</span>
          </div>
          <p className="ms-timeline-desc">
            Изучение регистров x86, переключения режимов реального и защищенного режима, попытки написать диспетчер памяти с нуля. Огромный багаж низкоуровневых знаний и понимание масштаба задачи.
          </p>
        </div>

        {/* Milestone 2 */}
        <div className="ms-timeline-item">
          <div className="ms-timeline-header">
            <span className="ms-timeline-title">2. Эксперименты на базе SayoriOS</span>
            <span className="ms-timeline-badge">ЭТАП 2 • 2025</span>
          </div>
          <p className="ms-timeline-desc">
            Попытка собрать систему на готовом открытом коде SayoriOS. Тестирование графических режимов, выявление архитектурных ограничений и осознание необходимости более фундаментального фундамента.
          </p>
        </div>

        {/* Milestone 3 */}
        <div className="ms-timeline-item">
          <div className="ms-timeline-header">
            <span className="ms-timeline-title">3. Череда проблем, отладка и выход в сеть</span>
            <span className="ms-timeline-badge">ЭТАП 3 • 2025–2026</span>
          </div>
          <p className="ms-timeline-desc">
            Сотни перезагрузок, борьба с падениями стека ядра Double Fault (0x7F) и долгожданный первый успешный сетевой пинг, подтвердивший жизнеспособность сетевого стека.
          </p>
        </div>

        {/* Milestone 4 */}
        <div className="ms-timeline-item">
          <div className="ms-timeline-header">
            <span className="ms-timeline-title">4. Переход на зрелую NT-совместимую архитектуру</span>
            <span className="ms-timeline-badge">ЭТАП 4 • 2026</span>
          </div>
          <p className="ms-timeline-desc">
            Переход на стандарты ядра Windows NT: нативная поддержка приложений Win32, стабильные видеорежимы VESA, классический проводник Explorer и поддержка реального оборудования.
          </p>
        </div>

        {/* Milestone 5 */}
        <div className="ms-timeline-item">
          <div className="ms-timeline-header">
            <span className="ms-timeline-title">5. KeshOS 0.8.2 Beta: Релиз с кодовым именем "Brownie"</span>
            <span className="ms-timeline-badge" style={{ background: '#ffcc00', color: '#000' }}>АКТУАЛЬНЫЙ РЕЛИЗ</span>
          </div>
          <p className="ms-timeline-desc">
            Рождение версии 0.8.2: авторский шоколадный стиль Brownie, стабильный мастер OOBE, исправление загрузчика FreeLoader под USB на реальных ноутбуках, защита от краха APIC на многоядерных процессорах и универсальный образ BootCD.
          </p>
        </div>
      </div>

      {/* 2. Gallery Section (AFTER the story) */}
      <div className="ms-section-header" style={{ marginTop: '28px' }}>
        <div className="ms-section-title">
          <span>Галерея снимков KeshOS 0.8.2 «Brownie»</span>
        </div>
        <span style={{ fontSize: '10px', color: '#666' }}>Интерфейс актуальной операционной системы</span>
      </div>

      <p style={{ marginBottom: '14px', fontSize: '11px', color: '#333', lineHeight: '1.5' }}>
        Познакомьтесь с актуальным визуальным оформлением нашей системы: от мастера первой настройки OOBE до рабочего стола, проводника и меню мультизагрузки. Нажмите на любой снимок для детального просмотра.
      </p>

      <div className="gallery-slot-grid">
        {brownieGallerySlots.map((slot) => (
          <div key={slot.id} className="gallery-slot-card">
            <div
              className="gallery-slot-preview"
              onClick={() => slot.hasImage && setSelectedPhoto(slot.previewSrc)}
              style={{ cursor: slot.hasImage ? 'pointer' : 'default' }}
            >
              {slot.hasImage ? (
                <img
                  src={slot.previewSrc}
                  alt={slot.title}
                  onError={(e) => {
                    (e.currentTarget as HTMLElement).style.display = 'none';
                  }}
                />
              ) : (
                <>
                  <div className="gallery-slot-placeholder-icon">📷</div>
                  <div style={{ fontSize: '10px', fontWeight: 'bold', color: '#003399' }}>
                    Слот для фото ОС
                  </div>
                  <div className="gallery-slot-tag">
                    {slot.targetFile}
                  </div>
                </>
              )}
            </div>

            <div style={{ fontWeight: 'bold', fontSize: '11px', color: '#003399', marginBottom: '4px' }}>
              {slot.title}
            </div>
            <div style={{ fontSize: '10px', color: '#555', lineHeight: '1.4', flex: 1 }}>
              {slot.desc}
            </div>
          </div>
        ))}
      </div>

      {/* Photo Modal */}
      {selectedPhoto && (
        <div
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0,0,0,0.75)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 9999,
            padding: '20px',
          }}
          onClick={() => setSelectedPhoto(null)}
        >
          <div
            style={{
              background: '#ffffff',
              padding: '10px',
              border: '2px outset #ffffff',
              maxWidth: '90%',
              maxHeight: '90%',
              boxShadow: '0 6px 20px rgba(0,0,0,0.5)',
            }}
            onClick={(e) => e.stopPropagation()}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
              <span style={{ fontWeight: 'bold' }}>Снимок экрана KeshOS Brownie</span>
              <button className="win-btn" onClick={() => setSelectedPhoto(null)}>
                Закрыть ✕
              </button>
            </div>
            <img src={selectedPhoto} alt="KeshOS Preview" style={{ maxWidth: '100%', maxHeight: '75vh', display: 'block' }} />
          </div>
        </div>
      )}

      {/* OS Comparison Table */}
      <div className="ms-card" style={{ marginTop: '20px', marginBottom: '16px' }}>
        <div className="ms-card-header">
          <span>Сравнение технических параметров</span>
        </div>
        <table className="ms-table">
          <thead>
            <tr>
              <th style={{ width: '25%' }}>Параметр</th>
              <th style={{ width: '40%' }}>KeshOS Workstation (Brownie)</th>
              <th style={{ width: '35%' }}>Типичные современные ОС</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td><strong>Потребление RAM</strong></td>
              <td style={{ color: '#006600', fontWeight: 'bold' }}>От 128 MB (полный запуск из памяти)</td>
              <td>От 4 GB до 8 GB ОЗУ</td>
            </tr>
            <tr>
              <td><strong>Время старта</strong></td>
              <td style={{ color: '#006600', fontWeight: 'bold' }}>От 5 до 15 секунд с момента включения</td>
              <td>Минуты фоновых обновлений и служб</td>
            </tr>
            <tr>
              <td><strong>Универсальность</strong></td>
              <td>Один гибридный ISO (Live RAM + Setup)</td>
              <td>Разрозненные установочные образы</td>
            </tr>
            <tr>
              <td><strong>Совместимость</strong></td>
              <td>Нативный запуск приложений Win32</td>
              <td>Отказ от обратной совместимости</td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  );
};
