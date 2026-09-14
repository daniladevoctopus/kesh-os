import React, { useState, useEffect, useRef } from 'react';
import type { PageTab } from './Navigation';
import mapPng from '../assets/map.png';
import rawAnimations from '../assets/animations.json';

interface Frame {
  Duration: number;
  ImagesOffsets?: {
    Column: number;
    Row: number;
  } | null;
}

interface AnimationData {
  Name: string;
  Frames: Frame[];
}

const FRAME_WIDTH = 124;
const FRAME_HEIGHT = 93;

const animations = rawAnimations as unknown as AnimationData[];
const animMap = new Map<string, Frame[]>();
animations.forEach((a) => animMap.set(a.Name, a.Frames));

interface ClippyAssistantProps {
  setActiveTab: (tab: PageTab) => void;
}

export const ClippyAssistant: React.FC<ClippyAssistantProps> = ({ setActiveTab }) => {
  const [bubbleOpen, setBubbleOpen] = useState(true);
  const [isMinimized, setIsMinimized] = useState(false);
  const [messageIndex, setMessageIndex] = useState(0);

  const [currentAnimName, setCurrentAnimName] = useState<string>('Show');
  const [frameIndex, setFrameIndex] = useState<number>(0);
  const [currentOffset, setCurrentOffset] = useState<{ col: number; row: number }>({ col: 0, row: 0 });

  const animTimerRef = useRef<number | null>(null);
  const idleTimerRef = useRef<number | null>(null);

  const messages = [
    'Здравствуйте! Похоже, вы знакомитесь с KeshOS 0.8.2. Рекомендуем скачать универсальный гибридный образ BootCD: он содержит и Live-режим без установки, и мастер разметки!',
    'Нужна инструкция по созданию загрузочной флешки? В разделе "База знаний & FAQ" есть руководство по записи через Rufus и Ventoy.',
    'Запуск на реальном ПК: образ уже оптимизирован для работы на многоядерных процессорах без сбоев APIC и синих экранов.',
  ];

  // Play a specific animation sequence
  const playAnimation = (name: string) => {
    if (!animMap.has(name)) return;
    if (animTimerRef.current) {
      clearTimeout(animTimerRef.current);
    }
    setCurrentAnimName(name);
    setFrameIndex(0);
  };

  // Step through frames of the current animation
  useEffect(() => {
    const frames = animMap.get(currentAnimName);
    if (!frames || frames.length === 0) return;

    const frame = frames[frameIndex];
    if (frame?.ImagesOffsets) {
      setCurrentOffset({
        col: frame.ImagesOffsets.Column,
        row: frame.ImagesOffsets.Row,
      });
    }

    const duration = frame?.Duration && frame.Duration > 0 ? frame.Duration : 100;

    animTimerRef.current = window.setTimeout(() => {
      if (frameIndex + 1 < frames.length) {
        setFrameIndex(frameIndex + 1);
      } else {
        // Animation finished: chain to next logical state
        if (currentAnimName === 'Show') {
          playAnimation('Greeting');
        } else if (currentAnimName === 'Greeting') {
          playAnimation('RestPose');
        } else if (currentAnimName !== 'RestPose') {
          playAnimation('RestPose');
        }
      }
    }, duration);

    return () => {
      if (animTimerRef.current) clearTimeout(animTimerRef.current);
    };
  }, [currentAnimName, frameIndex]);

  // Idle animation trigger when resting
  useEffect(() => {
    if (currentAnimName !== 'RestPose') return;

    const idleAnims = [
      'Idle1_1',
      'LookLeft',
      'LookRight',
      'Explain',
      'Thinking',
      'CheckingSomething',
      'Wave',
    ];

    const randomDelay = Math.floor(7000 + Math.random() * 8000); // 7 to 15 seconds

    idleTimerRef.current = window.setTimeout(() => {
      const nextAnim = idleAnims[Math.floor(Math.random() * idleAnims.length)];
      playAnimation(nextAnim);
    }, randomDelay);

    return () => {
      if (idleTimerRef.current) clearTimeout(idleTimerRef.current);
    };
  }, [currentAnimName]);

  const handleClippyClick = () => {
    // Play attention/congratulate animation on click
    const clickAnims = ['GetAttention', 'Congratulate', 'Wave', 'Explain'];
    const chosen = clickAnims[Math.floor(Math.random() * clickAnims.length)];
    playAnimation(chosen);

    if (!bubbleOpen) {
      setBubbleOpen(true);
    } else {
      setMessageIndex((prev) => (prev + 1) % messages.length);
    }
  };

  const handleNextTip = () => {
    setMessageIndex((prev) => (prev + 1) % messages.length);
    playAnimation('Explain');
  };

  if (isMinimized) {
    return (
      <div style={{ position: 'fixed', bottom: '12px', right: '12px', zIndex: 1000 }}>
        <button
          className="win-btn"
          onClick={() => {
            setIsMinimized(false);
            setBubbleOpen(true);
            playAnimation('Greeting');
          }}
          title="Вызвать ассистента Clippy"
          style={{ fontWeight: 'bold' }}
        >
          📎 Помощник KeshOS
        </button>
      </div>
    );
  }

  return (
    <div className="clippy-widget">
      {bubbleOpen && (
        <div className="clippy-bubble">
          <span
            className="clippy-bubble-close"
            onClick={() => setBubbleOpen(false)}
            title="Скрыть подсказку"
          >
            ✕
          </span>
          <p style={{ marginBottom: '8px', lineHeight: '1.45', color: '#222' }}>
            {messages[messageIndex]}
          </p>
          <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
            <button
              className="win-btn win-btn-primary"
              style={{ fontSize: '10px', padding: '2px 8px' }}
              onClick={() => {
                setActiveTab('downloads');
                playAnimation('Save');
              }}
            >
              Перейти к загрузке
            </button>
            <button
              className="win-btn"
              style={{ fontSize: '10px', padding: '2px 8px' }}
              onClick={handleNextTip}
            >
              Следующий совет
            </button>
            <button
              className="win-btn"
              style={{ fontSize: '10px', padding: '2px 6px', color: '#666' }}
              onClick={() => setIsMinimized(true)}
              title="Свернуть ассистента"
            >
              Свернуть
            </button>
          </div>
        </div>
      )}

      {/* Animated Clippy Sprite from map.png */}
      <div
        className="clippy-sprite-container"
        onClick={handleClippyClick}
        title="Нажмите на Clippy, чтобы получить совет!"
        style={{
          width: `${FRAME_WIDTH}px`,
          height: `${FRAME_HEIGHT}px`,
          backgroundImage: `url(${mapPng})`,
          backgroundPosition: `-${currentOffset.col * FRAME_WIDTH}px -${currentOffset.row * FRAME_HEIGHT}px`,
          backgroundRepeat: 'no-repeat',
          cursor: 'pointer',
          imageRendering: 'pixelated',
          filter: 'drop-shadow(2px 3px 5px rgba(0,0,0,0.35))',
          userSelect: 'none',
          flexShrink: 0,
        }}
      />
    </div>
  );
};
