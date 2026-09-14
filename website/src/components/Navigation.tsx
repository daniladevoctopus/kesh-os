import React from 'react';

export type PageTab = 'home' | 'about' | 'downloads' | 'support' | 'legal';

interface NavigationProps {
  activeTab: PageTab;
  setActiveTab: (tab: PageTab) => void;
}

export const Navigation: React.FC<NavigationProps> = ({ activeTab, setActiveTab }) => {
  const tabs: { id: PageTab; label: string }[] = [
    { id: 'home', label: 'Главная' },
    { id: 'about', label: 'О системе и возможностях' },
    { id: 'downloads', label: 'Центр загрузки' },
    { id: 'support', label: 'База знаний и FAQ' },
    { id: 'legal', label: 'Лицензия и условия' },
  ];

  const getBreadcrumbTitle = () => {
    switch (activeTab) {
      case 'home':
        return 'Главная страница';
      case 'about':
        return 'Продукты > KeshOS 0.8.2 Brownie > Возможности и архитектура';
      case 'downloads':
        return 'Центр загрузки > KeshOS Universal BootCD';
      case 'support':
        return 'Служба технической поддержки и База знаний (KB)';
      case 'legal':
        return 'Юридические положения и лицензионные условия';
    }
  };

  return (
    <>
      <nav className="ms-main-nav">
        {tabs.map((tab) => (
          <button
            key={tab.id}
            className={`ms-nav-item ${activeTab === tab.id ? 'active' : ''}`}
            onClick={() => setActiveTab(tab.id)}
            type="button"
          >
            <span>{tab.label}</span>
          </button>
        ))}
      </nav>

      <div className="ms-sub-bar">
        <div className="ms-breadcrumb">
          <span>kesh.sneakdeak.net</span>
          <span className="sep">&gt;</span>
          <strong>{getBreadcrumbTitle()}</strong>
        </div>
        <div style={{ fontSize: '10px', color: '#666' }}>
          Локализация: Русский (Россия) | Выпуск: 0.8.2 Beta
        </div>
      </div>
    </>
  );
};
