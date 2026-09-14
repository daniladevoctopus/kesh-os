import React from 'react';
import type { PageTab } from './Navigation';

interface FooterProps {
  setActiveTab: (tab: PageTab) => void;
}

export const Footer: React.FC<FooterProps> = ({ setActiveTab }) => {
  return (
    <footer className="ms-footer">
      <div className="ms-footer-links">
        <a href="#legal" onClick={(e) => { e.preventDefault(); setActiveTab('legal'); }}>
          Условия использования (Terms of Use)
        </a>
        <span>|</span>
        <a href="#legal" onClick={(e) => { e.preventDefault(); setActiveTab('legal'); }}>
          Заявление о конфиденциальности (Privacy Statement)
        </a>
        <span>|</span>
        <a href="#support" onClick={(e) => { e.preventDefault(); setActiveTab('support'); }}>
          Техническая поддержка
        </a>
        <span>|</span>
        <a href="#about" onClick={(e) => { e.preventDefault(); setActiveTab('about'); }}>
          О проекте KeshOS
        </a>
        <span>|</span>
        <a href="https://discord.gg/Gzd6ec6m" target="_blank" rel="noreferrer">
          Официальный Discord
        </a>
      </div>

      <div className="ms-footer-copy">
        <p>
          <strong>© 2026 SneakDeak Technologies.</strong> Создано и разработано командой SneakDeak Technologies для KeshOS Workstation. Все права защищены.
        </p>
        <p style={{ color: '#444', fontSize: '10px', marginTop: '2px' }}>
          Официальный адрес проекта: <strong>kesh.sneakdeak.net</strong>
        </p>
        <p style={{ color: '#777', fontSize: '9px', marginTop: '4px' }}>
          Microsoft, Windows, Windows NT являются зарегистрированными товарными знаками Microsoft Corporation.
          KeshOS является независимой открытой операционной системой на базе NT-архитектуры и ReactOS.
        </p>
      </div>
    </footer>
  );
};
