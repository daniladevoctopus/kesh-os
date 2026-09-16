import React, { useState } from 'react';

interface SupportPageProps {
  initialSearchQuery?: string;
}

export const SupportPage: React.FC<SupportPageProps> = ({ initialSearchQuery = '' }) => {
  const [filterText, setFilterText] = useState(initialSearchQuery);

  const kbArticles = [
    {
      id: 'KB082001',
      title: 'Устранение ошибки «Unable to load second stage loader» при загрузке с USB',
      date: '14 сентября 2026 г.',
      appliesTo: 'KeshOS 0.8.2 Brownie, FreeLoader',
      symptoms:
        'При включении ПК загрузка через Syslinux выдает серию сообщений: «Failed to load: multi(0)disk(0)cdrom(0)\\rosload.exe», после чего появляется сообщение «Unable to load second stage loader. Press any key».',
      cause:
        'BIOS материнской платы эмулирует флешку как диск 0x80 без активного флага в таблице разделов MBR. Старая версия загрузчика ошибочно переключалась на пустой CD-ROM привод.',
      resolution:
        'В версии KeshOS 0.8.2 эта проблема полностью решена: FreeLoader автоматически сканирует разделы rdisk(0..3)partition(1..4) и superfloppy (partition 0). Используйте актуальный универсальный образ bootcd.iso.',
    },
    {
      id: 'KB082002',
      title: 'Предотвращение сбоя ядра HAL1_INITIALIZATION_FAILED на многоядерных CPU',
      date: '11 сентября 2026 г.',
      appliesTo: 'Процессоры Intel Core, AMD Ryzen, SMP ACPI',
      symptoms:
        'При старте ядра системы на современных многоядерных ПК с поддержкой APIC происходит зависание ядра.',
      cause:
        'Многоядерная подсистема SMP ACPI в ранних версиях требует строгой синхронизации тактовой частоты ядер.',
      resolution:
        'В конфигурационный файл bootcd.ini добавлен проверенный параметр /NUMPROC=1. Он предписывает ядру безопасно использовать одно вычислительное ядро для обеспечения 100% стабильности на любых современных CPU.',
    },
    {
      id: 'KB082003',
      title: 'Настройка разрешения дисплея и поддержка драйверов VESA / VBEMP',
      date: '08 сентября 2026 г.',
      appliesTo: 'Видеоподсистема Win32k, VBE 2.0+',
      symptoms:
        'Экран по умолчанию запускается в базовом разрешении 800×600.',
      cause:
        'Использование универсального графического режима до установки фирменного драйвера видеокарты.',
      resolution:
        'Откройте Панель управления → Экран → Параметры. Выберите требуемое разрешение (1024×768, 1280×1024 или выше). Универсальный видеодрайвер поддерживает большинство современных мониторов.',
    },
    {
      id: 'KB082004',
      title: 'Работа с сетевыми подключениями и веб-браузером',
      date: '05 сентября 2026 г.',
      appliesTo: 'Сетевой стек TCP/IP, Ethernet адаптеры',
      symptoms:
        'Настройка интернет-соединения и выход в сеть из KeshOS.',
      cause:
        'Информационная справка по сетевой конфигурации.',
      resolution:
        'KeshOS автоматически получает сетевой IP-адрес по протоколу DHCP при наличии совместимого сетевого адаптера (Realtek, Intel PRO/1000, AMD PCnet). В системе предустановлены утилиты проверки сети ping и веб-браузер.',
    },
  ];

  const filtered = kbArticles.filter((art) => {
    const q = filterText.toLowerCase();
    return (
      art.id.toLowerCase().includes(q) ||
      art.title.toLowerCase().includes(q) ||
      art.symptoms.toLowerCase().includes(q) ||
      art.resolution.toLowerCase().includes(q)
    );
  });

  return (
    <div className="cc-page">
      <section className="cc-section" style={{ paddingTop: 32 }}>
        <div className="cc-section-header">
          <span className="cc-section-tag">Справка</span>
          <h2>База знаний и техническая поддержка</h2>
          <p className="cc-lead" style={{ marginTop: 8 }}>
            Руководства по решению типичных вопросов установки, совместимости оборудования и конфигурации ядра.
          </p>
        </div>

        {/* Search bar */}
        <div style={{ marginBottom: 28, maxWidth: 460 }}>
          <input
            type="text"
            className="cc-search-input"
            style={{ width: '100%', height: 42 }}
            placeholder="Фильтр по статьям: USB, CPU, экран, KB..."
            value={filterText}
            onChange={(e) => setFilterText(e.target.value)}
          />
        </div>

        {/* Accordions: <details> with <dl> per Cupcake spec § 6 */}
        <div className="cc-accordion-list">
          {filtered.length > 0 ? (
            filtered.map((art) => (
              <details key={art.id} className="cc-accordion" open={filtered.length === 1}>
                <summary className="cc-accordion-summary">
                  <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexWrap: 'wrap' }}>
                    <code className="cc-mono" style={{ color: 'var(--cc-accent-deep)', fontWeight: 600 }}>
                      {art.id}
                    </code>
                    <span>{art.title}</span>
                  </div>
                  <span className="cc-faint" style={{ flexShrink: 0 }}>
                    {art.date}
                  </span>
                </summary>

                <div className="cc-accordion-content">
                  <dl className="cc-accordion-dl">
                    <dt className="cc-accordion-dt">Применимо к:</dt>
                    <dd className="cc-accordion-dd">{art.appliesTo}</dd>

                    <dt className="cc-accordion-dt">Симптомы:</dt>
                    <dd className="cc-accordion-dd">{art.symptoms}</dd>

                    <dt className="cc-accordion-dt">Причина:</dt>
                    <dd className="cc-accordion-dd">{art.cause}</dd>

                    <dt className="cc-accordion-dt">Решение:</dt>
                    <dd className="cc-accordion-dd" style={{ fontWeight: 500 }}>
                      {art.resolution}
                    </dd>
                  </dl>
                </div>
              </details>
            ))
          ) : (
            <div className="cc-panel" style={{ textAlign: 'left' }}>
              <h3 style={{ marginBottom: 8 }}>По этому запросу статей нет</h3>
              <p className="cc-muted" style={{ marginBottom: 16 }}>
                Если вашей проблемы нет в базе знаний, задайте вопрос разработчикам в нашем Discord-сообществе.
              </p>
              <a
                href="https://discord.gg/Gzd6ec6m"
                target="_blank"
                rel="noreferrer"
                className="cc-btn cc-btn--discord cc-btn--small"
              >
                Задать вопрос в Discord
              </a>
            </div>
          )}
        </div>

        {/* Direct Help Callout */}
        <div className="cc-panel" style={{ marginTop: 48 }}>
          <h3 style={{ marginBottom: 8 }}>Остались вопросы?</h3>
          <p className="cc-muted" style={{ marginBottom: 20, maxWidth: '64ch' }}>
            Разработчики и участники сообщества KeshOS общаются на сервере SneakDeak Technologies. Вы можете прислать лог-файл <code>debugkesh.log</code> или задать вопрос по установке.
          </p>
          <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap' }}>
            <a
              href="https://discord.gg/Gzd6ec6m"
              target="_blank"
              rel="noreferrer"
              className="cc-btn cc-btn--discord"
            >
              Перейти в Discord
            </a>
            <a
              href="https://github.com/daniladevoctopus/kesh-os/issues"
              target="_blank"
              rel="noreferrer"
              className="cc-btn"
            >
              Открыть Issue на GitHub
            </a>
          </div>
        </div>
      </section>
    </div>
  );
};
