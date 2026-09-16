import React from 'react';

export type PageTab = 'home' | 'downloads' | 'about' | 'support' | 'legal';

interface NavigationProps {
  activeTab: PageTab;
  setActiveTab: (tab: PageTab) => void;
}

export const Navigation: React.FC<NavigationProps> = ({ activeTab, setActiveTab }) => {
  const tabs: { id: PageTab; label: string }[] = [
    { id: 'home', label: 'Обзор' },
    { id: 'downloads', label: 'Центр загрузки' },
    { id: 'about', label: 'Возможности' },
    { id: 'support', label: 'База знаний' },
    { id: 'legal', label: 'Лицензия' },
  ];

  return (
    <div className="cc-nav-row">
      <div className="cc-container">
        <nav className="cc-nav-list" aria-label="Разделы сайта">
          {tabs.map((tab) => (
            <button
              key={tab.id}
              className={`cc-nav-item ${activeTab === tab.id ? 'active' : ''}`}
              onClick={() => setActiveTab(tab.id)}
              type="button"
            >
              {tab.label}
            </button>
          ))}
        </nav>
      </div>
    </div>
  );
};
