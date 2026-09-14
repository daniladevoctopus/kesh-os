import React, { useState } from 'react';

interface SupportPageProps {
  initialSearchQuery?: string;
}

export const SupportPage: React.FC<SupportPageProps> = ({ initialSearchQuery = '' }) => {
  const [activeKb, setActiveKb] = useState<string | null>('KB082001');
  const [filterText, setFilterText] = useState(initialSearchQuery);

  const kbArticles = [
    {
      id: 'KB082001',
      title: 'Устранение ошибки "Unable to load second stage loader" при загрузке с USB-флешки',
      date: '14 сентября 2026 г.',
      appliesTo: 'KeshOS 0.8.2 Beta, FreeLoader i386',
      symptoms:
        'При включении ПК загрузка через Syslinux выдает серию сообщений: "Failed to load: multi(0)disk(0)cdrom(0)\\rosload.exe", после чего появляется сообщение "Unable to load second stage loader. Press any key".',
      cause:
        'BIOS материнской платы эмулирует флешку как диск 0x80 без активного флага в таблице разделов MBR. Старая версия загрузчика ошибочно переключалась на внутренний пустой CD-ROM привод.',
      resolution:
        'В версии KeshOS 0.8.2 эта проблема полностью решена. Загрузчик автоматически сканирует разделы rdisk(0..3)partition(1..4) и superfloppy (partition 0). Используйте свежий универсальный образ bootcd.iso от сентября 2026 года.',
    },
    {
      id: 'KB082002',
      title: 'Предотвращение сбоя ядра (HAL1_INITIALIZATION_FAILED) на современных процессорах',
      date: '11 сентября 2026 г.',
      appliesTo: 'Многоядерные процессоры Intel Core i3-i9, AMD Ryzen',
      symptoms:
        'При старте ядра системы на современных многоядерных ПК с поддержкой APIC происходит зависание или синий экран.',
      cause:
        'Многоядерная подсистема SMP ACPI в ранних сборках требует строгой синхронизации тактовой частоты ядер.',
      resolution:
        'В конфигурационный файл bootcd.ini добавлен параметр /NUMPROC=1. Он предписывает ядру безопасно использовать одно вычислительное ядро для обеспечения 100% стабильности на любых современных CPU.',
    },
    {
      id: 'KB082003',
      title: 'Настройка разрешения дисплея и поддержка драйверов VESA / VBEMP',
      date: '08 сентября 2026 г.',
      appliesTo: 'KeshOS Workstation, Видеоподсистема Win32k',
      symptoms:
        'Экран по умолчанию запускается в базовом разрешении 800x600.',
      cause:
        'Отсутствие специализированного видеодрайвера производителя видеокарты.',
      resolution:
        'Откройте Панель управления -> Экран -> Параметры. Выберите требуемое разрешение (1024x768, 1280x1024 или выше). Универсальный драйвер VBE / VESA поддерживает большинство современных мониторов и видеокарт.',
    },
    {
      id: 'KB082004',
      title: 'Работа с сетевыми подключениями и веб-браузером',
      date: '05 сентября 2026 г.',
      appliesTo: 'Сетевой стек TCP/IP, Ethernet контроллеры',
      symptoms:
        'Как настроить интернет-соединение и просматривать сайты в KeshOS?',
      cause:
        'Информационная справка по сетевым компонентам.',
      resolution:
        'KeshOS автоматически получает сетевой IP-адрес по протоколу DHCP при наличии совместимого сетевого адаптера (Realtek, Intel PRO/1000, AMD PCnet). В системе предустановлены утилиты проверки сети ping и веб-браузер.',
    },
  ];

  const filteredArticles = kbArticles.filter(
    (a) =>
      a.title.toLowerCase().includes(filterText.toLowerCase()) ||
      a.id.toLowerCase().includes(filterText.toLowerCase()) ||
      a.symptoms.toLowerCase().includes(filterText.toLowerCase())
  );

  return (
    <div className="ms-page-support">
      <div className="ms-section-header">
        <div className="ms-section-title">
          <span>База знаний (KeshOS Knowledge Base)</span>
        </div>
        <span style={{ fontSize: '10px', color: '#666' }}>Статьи технической поддержки</span>
      </div>

      {/* Filter box */}
      <div style={{ marginBottom: '14px', display: 'flex', gap: '8px', alignItems: 'center' }}>
        <span>Фильтр по ключевым словам или номеру статьи:</span>
        <input
          type="text"
          className="ms-search-input"
          placeholder="Например: USB, BSOD, KB082001..."
          value={filterText}
          onChange={(e) => setFilterText(e.target.value)}
          style={{ width: '220px' }}
        />
        {filterText && (
          <button className="win-btn" onClick={() => setFilterText('')}>
            Очистить
          </button>
        )}
      </div>

      {/* Articles List */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', marginBottom: '20px' }}>
        {filteredArticles.map((article) => {
          const isExpanded = activeKb === article.id;
          return (
            <div key={article.id} className="ms-kb-box">
              <div
                className="ms-kb-header"
                style={{ cursor: 'pointer' }}
                onClick={() => setActiveKb(isExpanded ? null : article.id)}
              >
                <div>
                  <span className="ms-kb-id">{article.id}</span>: <strong>{article.title}</strong>
                </div>
                <div style={{ display: 'flex', gap: '10px', alignItems: 'center' }}>
                  <span className="ms-kb-date">{article.date}</span>
                  <button className="win-btn" style={{ padding: '1px 6px', fontSize: '10px' }}>
                    {isExpanded ? '▲ Свернуть' : '▼ Читать'}
                  </button>
                </div>
              </div>

              {isExpanded && (
                <div style={{ fontSize: '11px', lineHeight: '1.6', marginTop: '10px' }}>
                  <p style={{ color: '#555', marginBottom: '8px' }}>
                    <strong>Применимо к:</strong> {article.appliesTo}
                  </p>

                  <div style={{ marginBottom: '8px' }}>
                    <strong style={{ color: '#8b0000' }}>ПРИЗНАКИ (SYMPTOMS):</strong>
                    <p style={{ marginTop: '2px', background: '#fdfbf2', padding: '6px', borderLeft: '3px solid #8b0000' }}>
                      {article.symptoms}
                    </p>
                  </div>

                  <div style={{ marginBottom: '8px' }}>
                    <strong style={{ color: '#003399' }}>ПРИЧИНА (CAUSE):</strong>
                    <p style={{ marginTop: '2px', background: '#f5f8fc', padding: '6px', borderLeft: '3px solid #003399' }}>
                      {article.cause}
                    </p>
                  </div>

                  <div>
                    <strong style={{ color: '#006600' }}>РЕШЕНИЕ (RESOLUTION):</strong>
                    <p style={{ marginTop: '2px', background: '#f4fbf4', padding: '6px', borderLeft: '3px solid #006600' }}>
                      {article.resolution}
                    </p>
                  </div>
                </div>
              )}
            </div>
          );
        })}
      </div>

      {/* Community Support Box */}
      <div className="ms-card">
        <div className="ms-card-header">
          <span>Служба сообщества и обратная связь</span>
        </div>
        <p style={{ marginBottom: '10px', fontSize: '11px', lineHeight: '1.5', color: '#333' }}>
          Сообщество разработчиков KeshOS оказывает оперативную техническую помощь. 
          Вы можете задать вопрос в нашем официальном Discord-сервере или передать лог-файл <code>debugkesh.log</code> разработчикам для анализа.
        </p>
        <a
          href="https://discord.gg/Gzd6ec6m"
          target="_blank"
          rel="noreferrer"
          className="ms-hero-btn"
          style={{ fontSize: '11px', padding: '5px 14px' }}
        >
          Перейти в Discord сообщество KeshOS
        </a>
      </div>
    </div>
  );
};
