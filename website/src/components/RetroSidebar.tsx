import React, { useState, useEffect } from 'react';
import type { PageTab } from './Navigation';

interface RetroSidebarProps {
  setActiveTab: (tab: PageTab) => void;
}

export const RetroSidebar: React.FC<RetroSidebarProps> = ({ setActiveTab }) => {
  const [visitorCount, setVisitorCount] = useState<string>('0001428');

  useEffect(() => {
    // Persistent visitor counter logic
    const BASE_COUNT = 1428;
    const STORAGE_KEY = 'kesh_visitor_count';
    const SESSION_KEY = 'kesh_session_tracked';

    let current = parseInt(localStorage.getItem(STORAGE_KEY) || `${BASE_COUNT}`, 10);
    if (isNaN(current) || current < BASE_COUNT) {
      current = BASE_COUNT;
    }

    // Only increment once per browser session
    if (!sessionStorage.getItem(SESSION_KEY)) {
      current += 1;
      localStorage.setItem(STORAGE_KEY, current.toString());
      sessionStorage.setItem(SESSION_KEY, 'true');
    }

    setVisitorCount(current.toString().padStart(7, '0'));

    // Try live counter API asynchronously for cross-device count (fail-safe)
    fetch('https://api.counterapi.dev/v1/keshos_sneakdeak_net/visits/up')
      .then((res) => res.json())
      .then((data) => {
        if (data && typeof data.count === 'number') {
          const total = BASE_COUNT + data.count;
          setVisitorCount(total.toString().padStart(7, '0'));
          localStorage.setItem(STORAGE_KEY, total.toString());
        }
      })
      .catch(() => {
        // Silently use localStorage fallback
      });
  }, []);

  return (
    <aside className="ms-sidebar">
      {/* Quick Navigation Box */}
      <div className="ms-sidebar-box">
        <div className="ms-sidebar-box-header">
          <span>Навигация по разделам</span>
        </div>
        <ul className="ms-sidebar-list">
          <li>
            <a href="#home" onClick={(e) => { e.preventDefault(); setActiveTab('home'); }}>
              Главная страница
            </a>
          </li>
          <li>
            <a href="#downloads" onClick={(e) => { e.preventDefault(); setActiveTab('downloads'); }}>
              <strong>Центр загрузки (BootCD)</strong>
            </a>
          </li>
          <li>
            <a href="#about" onClick={(e) => { e.preventDefault(); setActiveTab('about'); }}>
              О системе и возможностях
            </a>
          </li>
          <li>
            <a href="#support" onClick={(e) => { e.preventDefault(); setActiveTab('support'); }}>
              База знаний (KB Articles)
            </a>
          </li>
          <li>
            <a href="#legal" onClick={(e) => { e.preventDefault(); setActiveTab('legal'); }}>
              Лицензия и условия
            </a>
          </li>
        </ul>
      </div>

      {/* Release Spec Box */}
      <div className="ms-sidebar-box">
        <div className="ms-sidebar-box-header">
          <span>Спецификация выпуска</span>
        </div>
        <div style={{ padding: '8px', fontSize: '11px', lineHeight: '1.6', color: '#333' }}>
          <div><strong>Проект:</strong> KeshOS Workstation</div>
          <div><strong>Кодовое название:</strong> Brownie</div>
          <div><strong>Выпуск:</strong> 0.8.2 Beta</div>
          <div><strong>Разработчик:</strong> SneakDeak Technologies</div>
          <div><strong>Архитектура:</strong> x86 (IA-32)</div>
          <div><strong>Ядро:</strong> NT 5.2 Compatible</div>
          <div><strong>Образ:</strong> Universal Hybrid BootCD</div>
          <div style={{ marginTop: '6px', fontSize: '10px', color: '#666', borderTop: '1px solid #e1ecf8', paddingTop: '4px' }}>
            Объединяет Live-режим (RAM) и мастер установки Setup в едином ISO.
          </div>
        </div>
      </div>

      {/* Community Links Box */}
      <div className="ms-sidebar-box">
        <div className="ms-sidebar-box-header">
          <span>Сообщество & Связь</span>
        </div>
        <div style={{ padding: '8px' }}>
          <img
            src="/images/discord_banner.png"
            alt="KeshOS Discord"
            style={{ width: '100%', borderRadius: '3px', border: '1px solid #4752c4', marginBottom: '8px', display: 'block' }}
            onError={(e) => {
              (e.currentTarget as HTMLElement).style.display = 'none';
            }}
          />
          <a
            href="https://discord.gg/Gzd6ec6m"
            target="_blank"
            rel="noreferrer"
            className="win-btn"
            style={{ width: '100%', justifyContent: 'center', fontWeight: 'bold' }}
          >
            Войти в Discord
          </a>
        </div>
      </div>

      {/* Real Visitor Counter */}
      <div className="ms-sidebar-box" style={{ textAlign: 'center', padding: '10px 8px' }}>
        <div style={{ fontSize: '10px', color: '#555', marginBottom: '6px', fontWeight: 'bold' }}>
          Реальный счётчик обращений:
        </div>
        <div className="retro-counter-box" title="Реальное количество посещений">{visitorCount}</div>
        <div style={{ fontSize: '9px', color: '#888', marginTop: '4px' }}>
          kesh.sneakdeak.net
        </div>
      </div>
    </aside>
  );
};
