import React from 'react';
import type { PageTab } from './Navigation';

interface FooterProps {
  setActiveTab: (tab: PageTab) => void;
}

export const Footer: React.FC<FooterProps> = ({ setActiveTab }) => {
  return (
    <footer className="cc-footer">
      <div className="cc-container">
        <div className="cc-footer-content">
          <div>
            <div style={{ fontWeight: 600, color: 'var(--cc-text)', marginBottom: 4 }}>
              KeshOS Workstation
            </div>
            <p className="cc-faint" style={{ maxWidth: '48ch' }}>
              Быстрая, компактная операционная система с душой классики и поддержкой Win32. Проект сообщества SneakDeak Technologies.
            </p>
          </div>

          <ul className="cc-footer-links">
            <li>
              <a
                href="#about"
                onClick={(e) => {
                  e.preventDefault();
                  setActiveTab('about');
                }}
              >
                О системе
              </a>
            </li>
            <li>
              <a
                href="#downloads"
                onClick={(e) => {
                  e.preventDefault();
                  setActiveTab('downloads');
                }}
              >
                Загрузка
              </a>
            </li>
            <li>
              <a
                href="#support"
                onClick={(e) => {
                  e.preventDefault();
                  setActiveTab('support');
                }}
              >
                База знаний
              </a>
            </li>
            <li>
              <a
                href="#legal"
                onClick={(e) => {
                  e.preventDefault();
                  setActiveTab('legal');
                }}
              >
                Лицензия
              </a>
            </li>
            <li>
              <a href="https://discord.gg/Gzd6ec6m" target="_blank" rel="noreferrer">
                Discord
              </a>
            </li>
            <li>
              <a href="https://github.com/daniladevoctopus/kesh-os" target="_blank" rel="noreferrer">
                GitHub
              </a>
            </li>
          </ul>
        </div>

        <div style={{ marginTop: 32, paddingTop: 20, borderTop: '1px solid var(--cc-line)' }}>
          <p className="cc-faint" style={{ fontSize: '0.8rem', maxWidth: 'none' }}>
            © 2026 SneakDeak Technologies. Домен проекта: kesh.sneakdeak.net. KeshOS — независимая открытая операционная система на базе NT-архитектуры.
          </p>
        </div>
      </div>
    </footer>
  );
};
