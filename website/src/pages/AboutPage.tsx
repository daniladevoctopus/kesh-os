import React, { useState, useEffect } from 'react';
import screenDesktop from '../assets/stol.png';
import screenOobe from '../assets/oobe.png';
import screenStartmenu from '../assets/pusk.png';
import screenExplorer from '../assets/explorer.png';
import screenBootloader from '../assets/freeldr.png';

export const AboutPage: React.FC = () => {
  const [selectedImage, setSelectedImage] = useState<string | null>(null);

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        setSelectedImage(null);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  const screenshots = [
    {
      title: 'Мастер настройки OOBE Brownie',
      desc: 'Пошаговый мастер первой настройки в тёплой кофейной палитре.',
      src: screenOobe,
    },
    {
      title: 'Рабочий стол KeshOS Brownie',
      desc: 'Классический рабочий стол, панель задач и фирменные обои.',
      src: screenDesktop,
    },
    {
      title: 'Меню «Пуск» и программы',
      desc: 'Классическое меню быстрого доступа к системным утилитам.',
      src: screenStartmenu,
    },
    {
      title: 'Проводник «Мой компьютер»',
      desc: 'Управление локальными накопителями и системными файлами.',
      src: screenExplorer,
    },
    {
      title: 'Загрузчик FreeLoader',
      desc: 'Мультизагрузочное меню с поддержкой USB, CD и RAM Boot.',
      src: screenBootloader,
    },
  ];

  return (
    <div className="cc-page">
      <section className="cc-section" style={{ paddingTop: 32 }}>
        <div className="cc-section-header">
          <span className="cc-section-tag">Возможности</span>
          <h2>Архитектура и характеристики KeshOS</h2>
          <p className="cc-lead" style={{ marginTop: 8 }}>
            KeshOS построена на открытых стандартах архитектуры Windows NT и технологиях ReactOS. Система оптимизирована для быстрого отклика на скромном и современном оборудовании.
          </p>
        </div>

        {/* Architecture Highlights */}
        <div className="cc-card-grid" style={{ marginBottom: 48 }}>
          <div className="cc-card">
            <div className="cc-card-title">Совместимость Win32</div>
            <div className="cc-card-desc">
              Прямой нативный запуск классических программ, офисных пакетов, системных утилит и мультимедийных плееров без эмуляторов.
            </div>
          </div>

          <div className="cc-card">
            <div className="cc-card-title">Ядро NT 5.2</div>
            <div className="cc-card-desc">
              Стабильное ядро с поддержкой драйверов Windows NT/2000/2003, вытесняющей многозадачностью и защитой памяти.
            </div>
          </div>

          <div className="cc-card">
            <div className="cc-card-title">Запуск в RAM (LiveCD)</div>
            <div className="cc-card-desc">
              Загрузка полного образа системы в оперативную память для мгновенного отклика и работы без записи на физический накопитель.
            </div>
          </div>

          <div className="cc-card">
            <div className="cc-card-title">Загрузчик FreeLoader</div>
            <div className="cc-card-desc">
              Автоматическое определение дисков, поддержка USB 2.0/3.0, CD-ROM, PXE-сети и мультизагрузочных конфигураций.
            </div>
          </div>
        </div>

        {/* System Requirements Table */}
        <div style={{ marginBottom: 48 }}>
          <h3>Системные требования</h3>
          <p className="cc-muted" style={{ marginTop: 4, marginBottom: 16 }}>
            Минимальные и рекомендуемые аппаратные ресурсы для комфортной работы:
          </p>

          <div className="cc-table-wrap">
            <table className="cc-table">
              <thead>
                <tr>
                  <th>Компонент</th>
                  <th>Минимальные требования</th>
                  <th>Рекомендуемые требования</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td><strong>Процессор (CPU)</strong></td>
                  <td>x86 совместимый (Pentium II / III / Celeron 400 МГц)</td>
                  <td>Intel Core / AMD Athlon, Ryzen (от 1.5 ГГц)</td>
                </tr>
                <tr>
                  <td><strong>Оперативная память (RAM)</strong></td>
                  <td>128 МБ (режим установки) / 256 МБ (LiveCD)</td>
                  <td>512 МБ – 2 ГБ для работы в памяти</td>
                </tr>
                <tr>
                  <td><strong>Видеоадаптер</strong></td>
                  <td>VGA / SVGA 800×600 (16-битный цвет)</td>
                  <td>VESA 2.0+ / PCI / PCIe видеокарта 1024×768 и выше</td>
                </tr>
                <tr>
                  <td><strong>Накопитель</strong></td>
                  <td>USB-порт 2.0 / 3.0 или CD-ROM привод</td>
                  <td>USB-флешка от 512 МБ (FAT32) или SSD / HDD</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>

        {/* Comparison table */}
        <div style={{ marginBottom: 48 }}>
          <h3>Сравнение параметров</h3>
          <p className="cc-muted" style={{ marginTop: 4, marginBottom: 16 }}>
            Потребление ресурсов и скорость работы в сравнении:
          </p>

          <div className="cc-table-wrap">
            <table className="cc-table">
              <thead>
                <tr>
                  <th>Параметр</th>
                  <th>KeshOS Workstation (Brownie)</th>
                  <th>Типичные современные ОС</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td><strong>Потребление RAM</strong></td>
                  <td><strong style={{ color: 'var(--cc-accent-deep)' }}>От 128 МБ</strong> (полный запуск из памяти)</td>
                  <td className="cc-muted">От 4 ГБ до 8 ГБ ОЗУ</td>
                </tr>
                <tr>
                  <td><strong>Время старта</strong></td>
                  <td><strong style={{ color: 'var(--cc-accent-deep)' }}>5–15 секунд</strong> до рабочего стола</td>
                  <td className="cc-muted">Минуты фоновых служб и автозапуска</td>
                </tr>
                <tr>
                  <td><strong>Фоновая телеметрия</strong></td>
                  <td><strong>Полностью отсутствует</strong></td>
                  <td className="cc-muted">Сотни фоновых процессов аналитики</td>
                </tr>
                <tr>
                  <td><strong>Совместимость</strong></td>
                  <td>Нативный Win32 API</td>
                  <td className="cc-muted">Постепенный отказ от 32-битного ПО</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>

        {/* Screenshot Gallery */}
        <div>
          <h3>Галерея интерфейса</h3>
          <p className="cc-muted" style={{ marginTop: 4, marginBottom: 16 }}>
            Кликните по изображению для просмотра в высоком разрешении:
          </p>

          <div className="cc-gallery-grid">
            {screenshots.map((s, idx) => (
              <div
                key={idx}
                className="cc-gallery-card"
                onClick={() => setSelectedImage(s.src)}
                role="button"
                tabIndex={0}
                onKeyDown={(e) => {
                  if (e.key === 'Enter' || e.key === ' ') {
                    setSelectedImage(s.src);
                  }
                }}
              >
                <img src={s.src} alt={s.title} className="cc-gallery-thumb" />
                <div className="cc-gallery-info">
                  <div className="cc-gallery-title">{s.title}</div>
                  <div className="cc-gallery-desc">{s.desc}</div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* Modal for preview */}
      {selectedImage && (
        <div className="cc-modal-overlay" onClick={() => setSelectedImage(null)}>
          <div className="cc-modal-content" onClick={(e) => e.stopPropagation()}>
            <div className="cc-modal-header">
              <span style={{ fontWeight: 600, fontSize: '0.94rem' }}>
                Просмотр снимка экрана
              </span>
              <button
                type="button"
                className="cc-btn cc-btn--small"
                onClick={() => setSelectedImage(null)}
              >
                Закрыть
              </button>
            </div>
            <img src={selectedImage} alt="Preview" className="cc-modal-img" />
          </div>
        </div>
      )}
    </div>
  );
};
