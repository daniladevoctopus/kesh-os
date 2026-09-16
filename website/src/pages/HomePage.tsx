import React, { useState, useEffect } from 'react';
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

  // Close modal on Escape
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        setSelectedPhoto(null);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const galleryItems = [
    {
      id: 'desktop',
      title: 'Рабочий стол KeshOS Brownie',
      desc: 'Панель задач, авторские шоколадные обои и ярлыки рабочего пространства.',
      src: screenDesktop,
    },
    {
      id: 'oobe',
      title: 'Мастер настройки OOBE',
      desc: 'Пошаговый конфигуратор первого запуска: сеть, учётная запись и темы.',
      src: screenOobe,
    },
    {
      id: 'startmenu',
      title: 'Меню «Пуск» и утилиты',
      desc: 'Двухуровневое классическое меню и быстрый доступ к программам.',
      src: screenStartmenu,
    },
    {
      id: 'explorer',
      title: 'Проводник файлов',
      desc: 'Управление локальными дисками, дискетами и USB-накопителями.',
      src: screenExplorer,
    },
    {
      id: 'bootloader',
      title: 'Меню загрузчика FreeLoader',
      desc: 'Выбор вариантов запуска (RAM Boot, чистая установка, отладка).',
      src: screenBootloader,
    },
  ];

  return (
    <div className="cc-page">
      {/* Hero Section */}
      <section className="cc-hero">
        <div className="cc-hero-meta">
          <span className="cc-chip cc-chip--accent">
            <span className="cc-chip-dot" />
            0.8.2 Beta • Service Pack 2 (Brownie)
          </span>
          <span className="cc-chip">
            Архитектура x86 (IA-32)
          </span>
        </div>

        <div>
          <h1>KeshOS Workstation</h1>
          <p className="cc-hero-lead" style={{ marginTop: 12 }}>
            Быстрая и компактная операционная система с душой классики и нативной поддержкой Win32. Единый гибридный образ BootCD объединяет мгновенный запуск прямо в оперативную память и установку на диск.
          </p>
        </div>

        <div className="cc-hero-actions">
          <button
            type="button"
            className="cc-btn cc-btn--primary"
            onClick={() => setActiveTab('downloads')}
          >
            Скачать образ (390 МБ)
          </button>
          <button
            type="button"
            className="cc-btn"
            onClick={() => setActiveTab('about')}
          >
            Возможности системы
          </button>
          <a href="#history" className="cc-btn">
            История создания
          </a>
        </div>

        {/* The Slab: Physical display frame under screenshot */}
        <div className="cc-slab">
          <div className="cc-slab-inner">
            <img
              src={screenDesktop}
              alt="Рабочий стол KeshOS Brownie"
              className="cc-slab-img"
            />
            <div className="cc-slab-caption">
              Рабочий стол KeshOS 0.8.2 Beta Service Pack 2 (Brownie)
            </div>
          </div>
        </div>
      </section>

      {/* Fact Strip: .cc-facts */}
      <section className="cc-facts" aria-label="Технические показатели">
        <div className="cc-fact-item">
          <div className="cc-fact-num">128 МБ</div>
          <div className="cc-fact-label">минимум оперативной памяти для запуска</div>
        </div>
        <div className="cc-fact-item">
          <div className="cc-fact-num">5–15 с</div>
          <div className="cc-fact-label">время полной загрузки в RAM-диск</div>
        </div>
        <div className="cc-fact-item">
          <div className="cc-fact-num">390 МБ</div>
          <div className="cc-fact-label">размер универсального гибридного ISO</div>
        </div>
        <div className="cc-fact-item">
          <div className="cc-fact-num">NT 5.2</div>
          <div className="cc-fact-label">совместимость с Win32-программами и драйверами</div>
        </div>
      </section>

      {/* Screenshots Gallery */}
      <section className="cc-section">
        <div className="cc-section-header">
          <span className="cc-section-tag">Интерфейс</span>
          <h2>Снимки экрана KeshOS Brownie</h2>
          <p className="cc-lead" style={{ marginTop: 8 }}>
            Пошаговый мастер первой настройки OOBE, классический проводник и системный загрузчик. Нажмите на любой снимок для детального просмотра.
          </p>
        </div>

        <div className="cc-gallery-grid">
          {galleryItems.map((item) => (
            <div
              key={item.id}
              className="cc-gallery-card"
              onClick={() => setSelectedPhoto(item.src)}
              role="button"
              tabIndex={0}
              onKeyDown={(e) => {
                if (e.key === 'Enter' || e.key === ' ') {
                  setSelectedPhoto(item.src);
                }
              }}
            >
              <img src={item.src} alt={item.title} className="cc-gallery-thumb" />
              <div className="cc-gallery-info">
                <div className="cc-gallery-title">{item.title}</div>
                <div className="cc-gallery-desc">{item.desc}</div>
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* Project History */}
      <section id="history" className="cc-section">
        <div className="cc-section-header">
          <span className="cc-section-tag">Хроника</span>
          <h2>Как рождался KeshOS и кодовое имя «Brownie»</h2>
          <p className="cc-lead" style={{ marginTop: 8 }}>
            Искренний рассказ о пути от первой строчки кода до рабочего стола на реальном компьютере.
          </p>
        </div>

        <div className="cc-panel" style={{ margin: '0 0 32px 0' }}>
          <p style={{ marginBottom: 16 }}>
            Всё началось с простого желания: сделать лёгкую операционную систему, которой приятно пользоваться самим. Без фоновых процессов сбора телеметрии, без навязанных онлайн-аккаунтов и громоздких интерфейсов. Мы хотели вернуть ощущение прямого контроля над компьютером, когда система откликается мгновенно и служит рабочим инструментом.
          </p>
          <blockquote
            style={{
              borderLeft: '3px solid var(--cc-accent)',
              paddingLeft: 16,
              margin: '20px 0',
              fontStyle: 'italic',
              color: 'var(--cc-text)',
            }}
          >
            «Путь от первой строчки кода до загрузки рабочего стола на реальном железе оказался сложнее, чем казалось в начале. Но именно преодоление багов сделало систему надёжной.»
          </blockquote>
          <p style={{ marginBottom: 16 }}>
            Сначала мы пробовали писать собственное ядро на <strong>Rust</strong>. Изучали спецификации OSDev, регистры x86, писали диспетчер памяти и обработчики прерываний. Однако написание ядра — это малая часть дела: для повседневной работы требуются сотни драйверов видеокарт, звуковых чипов, контроллеров накопителей и сети.
          </p>
          <p style={{ marginBottom: 16 }}>
            После экспериментов с SayoriOS и долгой отладки сетевого стека мы перешли на проверенную тридцатью годами архитектуру <strong>Windows NT на базе технологий ReactOS</strong>. Это дало нативную совместимость с программами Win32, богатую базу драйверов и стабильный проводник.
          </p>
          <p>
            Кодовое имя <strong>«Brownie»</strong> выбрано потому, что мы хотели подарить уют и тепло: мягкие кофейно-шоколадные тона, как горячий кофе и свежий брауни прохладной ночью, напоминают о том, что технологии должны приносить радость.
          </p>
        </div>

        {/* Milestone cards */}
        <div className="cc-card-grid">
          <div className="cc-card">
            <span className="cc-chip">Этап 1 • 2024–2025</span>
            <div className="cc-card-title">Микроядро на Rust</div>
            <div className="cc-card-desc">
              Изучение архитектуры x86, переключения режимов реального и защищённого режима, низкоуровневая работа с памятью.
            </div>
          </div>

          <div className="cc-card">
            <span className="cc-chip">Этап 2 • 2025</span>
            <div className="cc-card-title">Опыт SayoriOS</div>
            <div className="cc-card-desc">
              Тестирование графических режимов и выявление архитектурных ограничений для повседневных задач.
            </div>
          </div>

          <div className="cc-card">
            <span className="cc-chip">Этап 3 • 2025–2026</span>
            <div className="cc-card-title">Отладка и первый пинг</div>
            <div className="cc-card-desc">
              Борьба с падениями стека ядра и первый успешный сетевой ICMP-ответ, подтвердивший работу стека.
            </div>
          </div>

          <div className="cc-card">
            <span className="cc-chip cc-chip--accent">Актуальная версия</span>
            <div className="cc-card-title">Бета 0.8.2 • Service Pack 2</div>
            <div className="cc-card-desc">
              Стабильный мастер OOBE, исправление загрузчика для флешек и Ventoy, поддержка одноядерного режима (/NUMPROC=1), универсальный образ BootCD.
            </div>
          </div>
        </div>
      </section>

      {/* Modal image viewer */}
      {selectedPhoto && (
        <div className="cc-modal-overlay" onClick={() => setSelectedPhoto(null)}>
          <div className="cc-modal-content" onClick={(e) => e.stopPropagation()}>
            <div className="cc-modal-header">
              <span style={{ fontWeight: 600, fontSize: '0.94rem' }}>
                Просмотр снимка экрана
              </span>
              <button
                type="button"
                className="cc-btn cc-btn--small"
                onClick={() => setSelectedPhoto(null)}
              >
                Закрыть
              </button>
            </div>
            <img src={selectedPhoto} alt="Снимок экрана KeshOS" className="cc-modal-img" />
          </div>
        </div>
      )}
    </div>
  );
};
