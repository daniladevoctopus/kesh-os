import React, { useState } from 'react';

export const DownloadsPage: React.FC = () => {
  const [copiedHash, setCopiedHash] = useState<string | null>(null);

  const copyToClipboard = (text: string, id: string) => {
    navigator.clipboard.writeText(text);
    setCopiedHash(id);
    setTimeout(() => setCopiedHash(null), 2000);
  };

  const downloadInfo = {
    title: 'KeshOS 0.8.2 Beta (Service Pack 2) Universal BootCD',
    filename: 'bootcd.iso',
    size: '390 МБ',
    date: '14 сентября 2026 г.',
    architecture: 'x86 (IA-32, 32-бит)',
    md5: 'b8ae81e440d6da5e924f1b2fd05d77b1',
    sha256: '9ec8664ec36d7d44122c870d6cdee2dafd82d568911b5194b90dc74c1e1cd0ee',
  };

  return (
    <div className="cc-page">
      <section className="cc-section" style={{ paddingTop: 32 }}>
        <div className="cc-section-header">
          <span className="cc-section-tag">Дистрибутив</span>
          <h2>Центр загрузки KeshOS</h2>
          <p className="cc-lead" style={{ marginTop: 8 }}>
            Единый официальный гибридный образ BootCD. Подходит для запуска с USB-флешки без установки и для разметки и установки на физический накопитель.
          </p>
        </div>

        {/* Main download frosted-glass panel */}
        <div className="cc-panel">
          <div style={{ display: 'flex', alignItems: 'center', gap: 10, flexWrap: 'wrap', marginBottom: 16 }}>
            <span className="cc-chip cc-chip--accent">
              <span className="cc-chip-dot" />
              Бета-версия • Service Pack 2
            </span>
            <span className="cc-chip">{downloadInfo.architecture}</span>
            <span className="cc-chip">{downloadInfo.size}</span>
            <span className="cc-chip">{downloadInfo.date}</span>
          </div>

          <h3 style={{ fontSize: '1.4rem', marginBottom: 8 }}>
            {downloadInfo.title}
          </h3>
          <p className="cc-muted" style={{ marginBottom: 24, maxWidth: '64ch' }}>
            Образ содержит готовую систему с ядром NT 5.2, драйверами устройств и графическим окружением Explorer. При старте компьютера загрузчик FreeLoader позволяет выбрать прямой вход в оперативную память либо классический мастер разметки диска.
          </p>

          <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexWrap: 'wrap', marginBottom: 28 }}>
            <a
              href={`/${downloadInfo.filename}`}
              download={downloadInfo.filename}
              className="cc-btn cc-btn--primary"
            >
              Скачать образ ({downloadInfo.size})
            </a>
            <a
              href="https://github.com/daniladevoctopus/kesh-os/releases"
              target="_blank"
              rel="noreferrer"
              className="cc-btn"
            >
              Релизы на GitHub
            </a>
          </div>

          {/* Checksums box */}
          <div
            style={{
              padding: 16,
              background: 'var(--cc-surface-1)',
              borderRadius: 'var(--cc-r-s)',
              border: '1px solid var(--cc-line)',
              display: 'flex',
              flexDirection: 'column',
              gap: 10,
              fontSize: '0.88rem',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexWrap: 'wrap' }}>
              <strong style={{ minWidth: 60, color: 'var(--cc-muted)' }}>MD5:</strong>
              <code className="cc-mono">{downloadInfo.md5}</code>
              <button
                type="button"
                className="cc-btn cc-btn--small"
                onClick={() => copyToClipboard(downloadInfo.md5, 'md5')}
                style={{ marginLeft: 'auto' }}
              >
                {copiedHash === 'md5' ? 'Скопировано' : 'Копировать'}
              </button>
            </div>

            <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexWrap: 'wrap' }}>
              <strong style={{ minWidth: 60, color: 'var(--cc-muted)' }}>SHA-256:</strong>
              <code className="cc-mono" style={{ wordBreak: 'break-all' }}>{downloadInfo.sha256}</code>
              <button
                type="button"
                className="cc-btn cc-btn--small"
                onClick={() => copyToClipboard(downloadInfo.sha256, 'sha256')}
                style={{ marginLeft: 'auto' }}
              >
                {copiedHash === 'sha256' ? 'Скопировано' : 'Копировать'}
              </button>
            </div>
          </div>
        </div>

        {/* Boot Modes Table */}
        <div style={{ marginTop: 48 }}>
          <h3>Режимы запуска в образе BootCD</h3>
          <p className="cc-muted" style={{ marginTop: 4, marginBottom: 16 }}>
            При включении ПК FreeLoader предлагает три варианта загрузки:
          </p>

          <div className="cc-table-wrap">
            <table className="cc-table">
              <thead>
                <tr>
                  <th>Пункт меню FreeLoader</th>
                  <th>Назначение и сценарий</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>
                    <strong>KeshOS Live (RAM Boot)</strong>
                  </td>
                  <td>
                    Запуск системы прямо в оперативную память. Жёсткие диски не изменяются. Идеально для быстрой проверки совместимости с ПК, запуска программ и работы в сети.
                  </td>
                </tr>
                <tr>
                  <td>
                    <strong>Install KeshOS (Setup Mode)</strong>
                  </td>
                  <td>
                    Классический мастер разметки и чистой установки на физический HDD/SSD или виртуальный диск.
                  </td>
                </tr>
                <tr>
                  <td>
                    <strong>KeshOS Live (Screen Debug)</strong>
                  </td>
                  <td>
                    Диагностический режим с выводом системных сообщений ядра NT прямо на дисплей для отладки драйверов.
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>

        {/* Writing guide */}
        <div style={{ marginTop: 48 }}>
          <h3>Запись образа на накопители</h3>
          <p className="cc-muted" style={{ marginTop: 4, marginBottom: 16 }}>
            Рекомендуемые способы подготовки установочного носителя:
          </p>

          <div className="cc-card-grid">
            <div className="cc-card">
              <div className="cc-card-title">1. Ventoy (рекомендуется)</div>
              <div className="cc-card-desc">
                Установите Ventoy на USB-флешку один раз. После этого просто скопируйте файл <code>bootcd.iso</code> на флешку как обычный документ.
              </div>
            </div>

            <div className="cc-card">
              <div className="cc-card-title">2. Rufus</div>
              <div className="cc-card-desc">
                Выберите USB-накопитель, укажите файл <code>bootcd.iso</code>, задайте схему раздела <strong>MBR</strong> и файловую систему <strong>FAT32</strong>.
              </div>
            </div>

            <div className="cc-card">
              <div className="cc-card-title">3. Оптический CD/DVD</div>
              <div className="cc-card-desc">
                Запишите ISO на диск на скорости 4x–8x с помощью ImgBurn или встроенных средств записи операционной системы.
              </div>
            </div>
          </div>
        </div>
      </section>
    </div>
  );
};
