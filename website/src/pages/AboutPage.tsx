import React, { useState } from 'react';
import screenDesktop from '../assets/stol.png';
import screenOobe from '../assets/oobe.png';
import screenStartmenu from '../assets/pusk.png';
import screenExplorer from '../assets/explorer.png';
import screenBootloader from '../assets/freeldr.png';

export const AboutPage: React.FC = () => {
  const [selectedImage, setSelectedImage] = useState<string | null>(null);

  const screenshots = [
    {
      title: 'Мастер настройки KeshOS (OOBE Brownie)',
      desc: 'Пошаговый мастер первой настройки в тёплой шоколадной теме.',
      src: screenOobe,
    },
    {
      title: 'Рабочий стол KeshOS Brownie',
      desc: 'Классический рабочий стол, панель задач и фирменные обои.',
      src: screenDesktop,
    },
    {
      title: 'Меню «Пуск» и программы',
      desc: 'Классическое меню "Пуск" и быстрый доступ к утилитам.',
      src: screenStartmenu,
    },
    {
      title: 'Проводник «Мой компьютер»',
      desc: 'Управление дисками и файловой системой в проводнике.',
      src: screenExplorer,
    },
    {
      title: 'Загрузчик FreeLoader',
      desc: 'Мультизагрузочное текстовое меню выбора режимов запуска.',
      src: screenBootloader,
    },
    {
      title: 'Официальный логотип KeshOS',
      desc: 'Фирменная символика проекта KeshOS Workstation.',
      src: '/images/keshos_logo.png',
    },
  ];

  return (
    <div className="ms-page-about">
      <div className="ms-section-header">
        <div className="ms-section-title">
          <span>О системе KeshOS Workstation: Обзор и Возможности</span>
        </div>
        <span style={{ fontSize: '10px', color: '#666' }}>Документация релиза 0.8.2</span>
      </div>

      <p style={{ marginBottom: '14px', fontSize: '12px', lineHeight: '1.6', color: '#222' }}>
        <strong>KeshOS</strong> — это специализированная операционная система, основанная на открытой архитектуре Windows NT и технологиях ReactOS. 
        Она разработана для обеспечения максимальной скорости работы, низких требований к аппаратным ресурсам и прямой совместимости с приложениями Win32.
      </p>

      {/* Architecture Highlights */}
      <div className="ms-grid-2">
        <div className="ms-card">
          <div className="ms-card-header">
            <span>Ключевые технологии архитектуры</span>
          </div>
          <ul style={{ paddingLeft: '18px', lineHeight: '1.6', fontSize: '11px', color: '#333' }}>
            <li><strong>Ядро NT:</strong> Совместимость с системными вызовами и драйверами Windows NT/2000/2003.</li>
            <li><strong>Подсистема Win32:</strong> Нативный запуск классических офисных пакетов, системных утилит и медиаплееров.</li>
            <li><strong>Загрузчик FreeLoader:</strong> Автоматический поиск накопителей, поддержка USB-флешек, CD-ROM, PXE и Limine BIOS/UEFI.</li>
            <li><strong>RAM-диск:</strong> Загрузка образа системы в оперативную память для мгновенного отклика без обращения к диску.</li>
          </ul>
        </div>

        <div className="ms-card">
          <div className="ms-card-header">
            <span>Визуальный стиль "Brownie"</span>
          </div>
          <ul style={{ paddingLeft: '18px', lineHeight: '1.6', fontSize: '11px', color: '#333' }}>
            <li><strong>Кофейно-шоколадная палитра:</strong> Тёплые, комфортные для глаз тона с классической четкостью оформления.</li>
            <li><strong>Качественная типографика:</strong> Оптимизированные системные шрифты и аккуратные векторные бейджи.</li>
            <li><strong>Исправленный OOBE:</strong> Полная стабильность мастера первоначальной настройки без сбоев ядра.</li>
            <li><strong>Управление ресурсами:</strong> Полное отсутствие принудительных фоновых сервисов и обновлений.</li>
          </ul>
        </div>
      </div>

      {/* System Requirements Table */}
      <div className="ms-card" style={{ marginBottom: '18px' }}>
        <div className="ms-card-header">
          <span>Системные требования к оборудованию</span>
        </div>
        <table className="ms-table">
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
              <td>x86 совместимый (Pentium II / III / Celeron 400 MHz)</td>
              <td>Intel Core 2 Duo / Core i3-i9 / AMD Ryzen, Athlon</td>
            </tr>
            <tr>
              <td><strong>Оперативная память (RAM)</strong></td>
              <td>128 MB RAM (для Setup-режима) / 256 MB (для LiveCD)</td>
              <td>512 MB - 2 GB RAM (мгновенная работа в памяти)</td>
            </tr>
            <tr>
              <td><strong>Видеоадаптер</strong></td>
              <td>VGA / SVGA с разрешением 800x600 (16-bit color)</td>
              <td>VESA 2.0+ / PCI / PCIe видеокарта 1024x768 или выше</td>
            </tr>
            <tr>
              <td><strong>Накопитель</strong></td>
              <td>USB-порт 1.1 / 2.0 / 3.0 или CD-ROM привод</td>
              <td>USB-флешка от 512 MB (FAT32) или SSD / HDD</td>
            </tr>
          </tbody>
        </table>
      </div>

      {/* Screenshot Gallery */}
      <div className="ms-section-header">
        <div className="ms-section-title">
          <span>Галерея снимков экрана</span>
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(220px, 1fr))', gap: '14px', marginBottom: '20px' }}>
        {screenshots.map((s, idx) => (
          <div key={idx} className="ms-card" style={{ textAlign: 'center' }}>
            <img
              src={s.src}
              alt={s.title}
              style={{
                width: '100%',
                height: '140px',
                objectFit: 'cover',
                border: '1px solid #7f9db9',
                cursor: 'pointer',
                marginBottom: '6px'
              }}
              onClick={() => setSelectedImage(s.src)}
              onError={(e) => {
                (e.currentTarget as HTMLImageElement).src = '/images/keshos_logo.png';
              }}
            />
            <div style={{ fontWeight: 'bold', fontSize: '11px', color: '#003399', marginBottom: '4px' }}>
              {s.title}
            </div>
            <div style={{ fontSize: '10px', color: '#555' }}>
              {s.desc}
            </div>
          </div>
        ))}
      </div>

      {/* Modal for preview */}
      {selectedImage && (
        <div
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0,0,0,0.7)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 9999,
            padding: '20px'
          }}
          onClick={() => setSelectedImage(null)}
        >
          <div
            style={{
              background: '#ffffff',
              padding: '8px',
              border: '2px outset #ffffff',
              maxWidth: '90%',
              maxHeight: '90%',
              boxShadow: '0 4px 16px rgba(0,0,0,0.5)'
            }}
            onClick={(e) => e.stopPropagation()}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
              <span style={{ fontWeight: 'bold' }}>Просмотр изображения</span>
              <button className="win-btn" onClick={() => setSelectedImage(null)}>Закрыть ✕</button>
            </div>
            <img src={selectedImage} alt="Preview" style={{ maxWidth: '100%', maxHeight: '75vh', display: 'block' }} />
          </div>
        </div>
      )}
    </div>
  );
};
