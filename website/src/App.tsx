import { useState, useEffect } from 'react';
import { Header } from './components/Header';
import { Navigation } from './components/Navigation';
import type { PageTab } from './components/Navigation';
import { RetroSidebar } from './components/RetroSidebar';
import { Footer } from './components/Footer';
import { RetroStatusBar } from './components/RetroStatusBar';
import { ClippyAssistant } from './components/ClippyAssistant';

import { HomePage } from './pages/HomePage';
import { AboutPage } from './pages/AboutPage';
import { DownloadsPage } from './pages/DownloadsPage';
import { SupportPage } from './pages/SupportPage';
import { LegalPage } from './pages/LegalPage';

import './styles/retro-ms.css';

export function App() {
  const [activeTab, setActiveTabState] = useState<PageTab>('home');
  const [isLoading, setIsLoading] = useState<boolean>(false);
  const [searchQuery, setSearchQuery] = useState<string>('');

  // Sync with URL Hash on load
  useEffect(() => {
    const hash = window.location.hash.replace('#', '') as PageTab;
    if (['home', 'about', 'downloads', 'support', 'legal'].includes(hash)) {
      setActiveTabState(hash);
    }

    const handleHashChange = () => {
      const newHash = window.location.hash.replace('#', '') as PageTab;
      if (['home', 'about', 'downloads', 'support', 'legal'].includes(newHash)) {
        setActiveTabState(newHash);
      }
    };

    window.addEventListener('hashchange', handleHashChange);
    return () => window.removeEventListener('hashchange', handleHashChange);
  }, []);

  const setActiveTab = (tab: PageTab) => {
    if (tab === activeTab) return;
    setIsLoading(true);
    window.location.hash = tab;
    setActiveTabState(tab);
    window.scrollTo({ top: 0, behavior: 'smooth' });

    // Brief simulated loading state for authentic retro feel
    setTimeout(() => {
      setIsLoading(false);
    }, 200);
  };

  const handleSearch = (query: string) => {
    if (!query.trim()) return;
    setActiveTab('support');
  };

  const renderActivePage = () => {
    switch (activeTab) {
      case 'home':
        return <HomePage setActiveTab={setActiveTab} />;
      case 'about':
        return <AboutPage />;
      case 'downloads':
        return <DownloadsPage />;
      case 'support':
        return <SupportPage initialSearchQuery={searchQuery} />;
      case 'legal':
        return <LegalPage />;
      default:
        return <HomePage setActiveTab={setActiveTab} />;
    }
  };

  return (
    <div className="retro-app-wrapper">
      {/* Top Microsoft Utility & Search Bar */}
      <Header
        onSearch={handleSearch}
        searchQuery={searchQuery}
        setSearchQuery={setSearchQuery}
      />

      {/* Blue Ribbon Main Navigation */}
      <Navigation activeTab={activeTab} setActiveTab={setActiveTab} />

      {/* Main Body Area */}
      <div className="ms-body-layout">
        <RetroSidebar setActiveTab={setActiveTab} />
        <main className="ms-main-content">{renderActivePage()}</main>
      </div>

      {/* Legal & Navigation Footer */}
      <Footer setActiveTab={setActiveTab} />

      {/* Simulated IE Browser Status Bar */}
      <RetroStatusBar isLoading={isLoading} />

      {/* Interactive Clippy Assistant */}
      <ClippyAssistant setActiveTab={setActiveTab} />
    </div>
  );
}

export default App;
