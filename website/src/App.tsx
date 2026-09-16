import { useState, useEffect } from 'react';
import { Header } from './components/Header';
import type { PageTab } from './components/Navigation';
import { Footer } from './components/Footer';

import { HomePage } from './pages/HomePage';
import { AboutPage } from './pages/AboutPage';
import { DownloadsPage } from './pages/DownloadsPage';
import { SupportPage } from './pages/SupportPage';
import { LegalPage } from './pages/LegalPage';

import './styles/cupcake.css';

export function App() {
  const [activeTab, setActiveTabState] = useState<PageTab>('home');
  const [searchQuery, setSearchQuery] = useState<string>('');
  const [theme, setTheme] = useState<'light' | 'dark'>(() => {
    const saved = localStorage.getItem('cc_theme');
    if (saved === 'light' || saved === 'dark') {
      return saved;
    }
    return window.matchMedia?.('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  });

  // Sync theme with HTML attribute
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('cc_theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme((prev) => (prev === 'light' ? 'dark' : 'light'));
  };

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
    window.location.hash = tab;
    setActiveTabState(tab);
    window.scrollTo({ top: 0, behavior: 'smooth' });
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
    <div className="cc-app">
      {/* Background ambient lighting */}
      <div className="cc-ambient-glow" aria-hidden="true" />

      {/* Sticky Header with 2 rows */}
      <Header
        onSearch={handleSearch}
        searchQuery={searchQuery}
        setSearchQuery={setSearchQuery}
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        theme={theme}
        toggleTheme={toggleTheme}
      />

      {/* Main Content Area */}
      <main className="cc-container">
        {renderActivePage()}
      </main>

      {/* Footer */}
      <Footer setActiveTab={setActiveTab} />
    </div>
  );
}

export default App;
