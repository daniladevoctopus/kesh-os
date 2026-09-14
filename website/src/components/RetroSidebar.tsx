import React, { useState, useEffect } from 'react';
import type { PageTab } from './Navigation';

interface RetroSidebarProps {
  setActiveTab: (tab: PageTab) => void;
}

export const RetroSidebar: React.FC<RetroSidebarProps> = ({ setActiveTab }) => {
  const [discordMembers, setDiscordMembers] = useState<number>(15);
  const [discordOnline, setDiscordOnline] = useState<number>(7);

  useEffect(() => {
    // Fetch live Discord member and online count
    fetch('https://discord.com/api/v9/invites/Gzd6ec6m?with_counts=true')
      .then((res) => res.json())
      .then((data) => {
        if (data) {
          if (typeof data.approximate_member_count === 'number') {
            setDiscordMembers(data.approximate_member_count);
          }
          if (typeof data.approximate_presence_count === 'number') {
            setDiscordOnline(data.approximate_presence_count);
          }
        }
      })
      .catch(() => {
        // Fallback to current verified count
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
          <div><strong>Архитектура:</strong> x86 (IA-32, 32-бит)</div>
          <div><strong>Ядро:</strong> NT 5.2 / ReactOS Core</div>
          <div><strong>Образ:</strong> Universal Hybrid BootCD</div>
          <div style={{ marginTop: '6px', fontSize: '10px', color: '#006600', borderTop: '1px solid #e1ecf8', paddingTop: '4px', fontWeight: 'bold' }}>
            ✓ Рабочий LiveCD (запуск прямо в RAM)
          </div>
          <div style={{ fontSize: '10px', color: '#003399' }}>
            ✓ Классическая чистая установка на HDD
          </div>
        </div>
      </div>

      {/* Community Links Box with Real Stats */}
      <div className="ms-sidebar-box">
        <div className="ms-sidebar-box-header">
          <span>Сообщество & Связь</span>
        </div>
        <div style={{ padding: '8px' }}>
          <div style={{ marginBottom: '8px', padding: '6px 8px', background: '#f5f7fc', border: '1px solid #d0d7de', borderRadius: '3px', fontSize: '11px' }}>
            <div style={{ fontWeight: 'bold', color: '#5865F2', marginBottom: '3px' }}>
              SneakDeak Technologies
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ color: '#2da44e', fontWeight: 'bold', display: 'inline-flex', alignItems: 'center', gap: '4px' }}>
                <span style={{ display: 'inline-block', width: '7px', height: '7px', borderRadius: '50%', background: '#2da44e' }}></span>
                {discordOnline} в сети
              </span>
              <span style={{ color: '#57606a' }}>
                👥 {discordMembers} участников
              </span>
            </div>
          </div>

          <a
            href="https://discord.gg/Gzd6ec6m"
            target="_blank"
            rel="noreferrer"
            className="win-btn"
            style={{ width: '100%', justifyContent: 'center', fontWeight: 'bold', marginBottom: '6px' }}
          >
            Войти в Discord
          </a>

          <a
            href="https://github.com/daniladevoctopus/kesh-os"
            target="_blank"
            rel="noreferrer"
            className="win-btn"
            style={{ width: '100%', justifyContent: 'center', fontSize: '10px' }}
          >
            Исходный код на GitHub
          </a>
        </div>
      </div>
    </aside>
  );
};
