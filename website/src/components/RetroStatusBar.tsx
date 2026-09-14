import React from 'react';

interface RetroStatusBarProps {
  isLoading: boolean;
}

export const RetroStatusBar: React.FC<RetroStatusBarProps> = ({ isLoading }) => {
  return (
    <div className="retro-status-bar">
      <div className="retro-status-left">
        <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor" style={{ opacity: 0.7 }}>
          <path d="M4 1h6l4 4v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1zm5 1H4v12h9V6h-4V2z" />
        </svg>
        <span>{isLoading ? 'Загрузка данных KeshOS.com...' : 'Готово'}</span>
      </div>

      {isLoading && (
        <div className="retro-status-progress">
          <div className="retro-status-progress-fill" style={{ width: '80%' }}></div>
        </div>
      )}

      <div className="retro-status-right">
        <div className="retro-status-zone" title="Защищенный узел SSL">
          <svg width="10" height="10" viewBox="0 0 16 16" fill="currentColor">
            <path d="M8 1a4 4 0 0 0-4 4v3H3a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h10a1 1 0 0 0 1-1V9a1 1 0 0 0-1-1h-1V5a4 4 0 0 0-4-4zm2 7H6V5a2 2 0 1 1 4 0v3z"/>
          </svg>
          <span>SSL</span>
        </div>
        <div className="retro-status-zone" title="Зона безопасности Интернет">
          <svg width="10" height="10" viewBox="0 0 16 16" fill="currentColor">
            <path d="M8 0a8 8 0 1 0 0 16A8 8 0 0 0 8 0zm5.9 7H10.8A12.7 12.7 0 0 0 9.7 2.1 6.5 6.5 0 0 1 13.9 7zM8 1.6c.7 1.2 1.4 3.2 1.6 5.4H6.4C6.6 4.8 7.3 2.8 8 1.6zM2.1 7A6.5 6.5 0 0 1 6.3 2.1 12.7 12.7 0 0 0 5.2 7H2.1zm0 2h3.1c.2 2 .8 4 1.1 5.3A6.5 6.5 0 0 1 2.1 9zm4.3 0h3.2c-.2 2.2-.9 4.2-1.6 5.4C7.3 13.2 6.6 11.2 6.4 9zm4.4 0h3.1a6.5 6.5 0 0 1-4.2 5.3c.3-1.3.9-3.3 1.1-5.3z"/>
          </svg>
          <span>Интернет</span>
        </div>
      </div>
    </div>
  );
};
