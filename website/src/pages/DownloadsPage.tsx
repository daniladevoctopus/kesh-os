import React, { useState } from 'react';

export const DownloadsPage: React.FC = () => {
  const [copiedHash, setCopiedHash] = useState<string | null>(null);

  const copyToClipboard = (text: string, id: string) => {
    navigator.clipboard.writeText(text);
    setCopiedHash(id);
    setTimeout(() => setCopiedHash(null), 2500);
  };

  const downloadInfo = {
    id: 'bootcd',
    title: 'KeshOS 0.8.2 Universal BootCD (Hybrid Edition)',
    filename: 'bootcd.iso',
    size: '390 MB',
    date: '14 сентября 2026 г.',
    architectures: 'x86 (IA-32, 32-бит)',
    md5: 'b8ae81e440d6da5e924f1b2fd05d77b1',
    sha256: '9ec8664ec36d7d44122c870d6cdee2dafd82d568911b5194b90dc74c1e1cd0ee',
  };

  return (
    <div className="ms-page-downloads">
      <div className="ms-section-header">
        <div className="ms-section-title">
          <span>Центр загрузки KeshOS (Download Center)</span>
        </div>
        <span style={{ fontSize: '10px', color: '#666' }}>Официальный дистрибутив операционной системы</span>
      </div>

      {/* Main Single Download Card */}
      <div
        className="ms-card"
        style={{
          marginBottom: '16px',
          border: '2px solid #003399',
          position: 'relative',
        }}
      >
        <div
          style={{
            position: 'absolute',
            top: '-10px',
            right: '12px',
            background: '#ffcc00',
            color: '#000000',
            fontWeight: 'bold',
            fontSize: '10px',
            padding: '2px 8px',
            border: '1px solid #cc9900',
            borderRadius: '2px',
          }}
        >
          ЕДИНЫЙ УНИВЕРСАЛЬНЫЙ ОБРАЗ (HYBRID)
        </div>

        <div className="ms-card-header">
          <span>{downloadInfo.title}</span>
          <span style={{ marginLeft: 'auto', fontSize: '11px', color: '#666' }}>
            Размер: <strong>{downloadInfo.size}</strong>
          </span>
        </div>

        <p style={{ marginBottom: '12px', color: '#333', lineHeight: '1.6' }}>
          Теперь вам не нужно выбирать между разными версиями: единый гибридный образ <strong>BootCD</strong> включает в себя как возможность мгновенной работы без установки прямо из оперативной памяти (Live RAM-диск), так и полный мастер чистой установки на жесткий диск или SSD. Образ оптимизирован для платформы {downloadInfo.architectures}.
        </p>

        {/* Action Row */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            background: '#f4f8fd',
            padding: '10px 14px',
            border: '1px solid #d8e5f5',
            flexWrap: 'wrap',
            gap: '12px',
            marginBottom: '14px',
          }}
        >
          <div>
            <div>Файл: <code>{downloadInfo.filename}</code> ({downloadInfo.size})</div>
            <div style={{ fontSize: '10px', color: '#666', marginTop: '2px' }}>
              Дата сборки: {downloadInfo.date} | Архитектура: {downloadInfo.architectures} | Зеркало: Сервер друга &amp; Домен
            </div>
          </div>
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            <a
              href={`/${downloadInfo.filename}`}
              download={downloadInfo.filename}
              className="ms-hero-btn"
              style={{ fontSize: '12px', padding: '6px 18px' }}
            >
              Скачать BootCD ({downloadInfo.size})
            </a>
            <a
              href="https://github.com/daniladevoctopus/kesh-os/releases"
              target="_blank"
              rel="noreferrer"
              className="win-btn"
              style={{ fontSize: '11px', padding: '5px 12px', textDecoration: 'none' }}
            >
              GitHub Releases
            </a>
          </div>
        </div>

        {/* What's inside section */}
        <div style={{ marginBottom: '14px' }}>
          <strong style={{ color: '#003399', fontSize: '12px' }}>
            Что входит в состав универсального образа:
          </strong>
          <table className="ms-table" style={{ marginTop: '6px' }}>
            <thead>
              <tr>
                <th style={{ width: '30%' }}>Режим загрузки</th>
                <th style={{ width: '70%' }}>Назначение и особенности</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td><strong>KeshOS Live (RAM Boot)</strong></td>
                <td>
                  Запуск системы прямо в оперативную память вашего ПК. Не затрагивает жесткие диски, позволяет тестировать совместимость, выходить в сеть и работать с программами без установки.
                </td>
              </tr>
              <tr>
                <td><strong>Install KeshOS (Setup Mode)</strong></td>
                <td>
                  Классический мастер установки на физический или виртуальный HDD/SSD. Включает разметку дисков, форматирование (FAT32/Btrfs) и установку системных файлов.
                </td>
              </tr>
              <tr>
                <td><strong>KeshOS Live (Screen Debug)</strong></td>
                <td>
                  Диагностический режим с подробным выводом системных сообщений загрузчика и ядра NT прямо на дисплей для быстрого выявления неполадок.
                </td>
              </tr>
            </tbody>
          </table>
        </div>

        {/* Simulated FreeLoader Menu Preview */}
        <div style={{ marginBottom: '14px' }}>
          <div style={{ fontSize: '10px', color: '#666', marginBottom: '4px' }}>
            При загрузке компьютера с флешки или диска вы увидите меню выбора:
          </div>
          <div
            style={{
              background: '#000080',
              color: '#ffffff',
              fontFamily: 'Courier New, monospace',
              fontSize: '11px',
              padding: '10px 14px',
              border: '2px inset #ffffff',
              lineHeight: '1.4',
            }}
          >
            <div style={{ color: '#ffff00', borderBottom: '1px solid #4444aa', paddingBottom: '4px', marginBottom: '6px' }}>
              KeshOS Boot Media - FreeLoader v5.2
            </div>
            <div style={{ background: '#00aaaa', color: '#000000', padding: '1px 4px' }}>
              &gt; KeshOS Live (RAM Boot)
            </div>
            <div style={{ padding: '1px 4px' }}>
              &nbsp;&nbsp;KeshOS Live (Screen Debug)
            </div>
            <div style={{ padding: '1px 4px' }}>
              &nbsp;&nbsp;Install KeshOS (Setup Mode)
            </div>
          </div>
        </div>

        {/* Checksums */}
        <div style={{ fontSize: '10px', color: '#555', background: '#fafafa', padding: '8px 10px', border: '1px solid #eee' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
            <span style={{ width: '55px' }}><strong>MD5:</strong></span>
            <code>{downloadInfo.md5}</code>
            <button
              className="win-btn"
              style={{ fontSize: '9px', padding: '1px 6px' }}
              onClick={() => copyToClipboard(downloadInfo.md5, 'md5')}
            >
              {copiedHash === 'md5' ? 'Скопировано!' : 'Копировать'}
            </button>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ width: '55px' }}><strong>SHA-256:</strong></span>
            <code style={{ fontSize: '9px' }}>{downloadInfo.sha256}</code>
            <button
              className="win-btn"
              style={{ fontSize: '9px', padding: '1px 6px' }}
              onClick={() => copyToClipboard(downloadInfo.sha256, 'sha256')}
            >
              {copiedHash === 'sha256' ? 'Скопировано!' : 'Копировать'}
            </button>
          </div>
        </div>
      </div>

      {/* Writing Instructions */}
      <div className="ms-card">
        <div className="ms-card-header">
          <span>Инструкция по записи образа на накопители</span>
        </div>
        <ol style={{ paddingLeft: '20px', lineHeight: '1.8', fontSize: '11px', color: '#333' }}>
          <li>
            <strong>Вариант 1 (Ventoy, наиболее удобный):</strong> Установите Ventoy на USB-флешку один раз. После этого просто скопируйте файл <code>bootcd.iso</code> на флешку как обычный документ.
          </li>
          <li>
            <strong>Вариант 2 (Rufus):</strong> Запустите Rufus, выберите USB-накопитель и файл <code>bootcd.iso</code>. Выберите схему <em>MBR</em> и файловую систему <em>FAT32</em>.
          </li>
          <li>
            <strong>Вариант 3 (Оптический CD/DVD диск):</strong> Запишите ISO на диск на скорости 4x–8x с помощью ImgBurn, UltraISO или встроенных средств записи.
          </li>
        </ol>
      </div>
    </div>
  );
};
