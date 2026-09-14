import React from 'react';

export const LegalPage: React.FC = () => {
  return (
    <div className="ms-page-legal">
      <div className="ms-section-header">
        <div className="ms-section-title">
          <span>Юридические положения, Условия и Конфиденциальность</span>
        </div>
        <span style={{ fontSize: '10px', color: '#666' }}>Редакция от сентября 2026 г.</span>
      </div>

      {/* Privacy Statement Box */}
      <div className="ms-card" style={{ marginBottom: '16px' }}>
        <div className="ms-card-header">
          <span>Положение о конфиденциальности KeshOS (Privacy Statement)</span>
        </div>
        <div style={{ fontSize: '11px', lineHeight: '1.6', color: '#333' }}>
          <p style={{ marginBottom: '8px' }}>
            Проект KeshOS придерживается строгих стандартов цифровой конфиденциальности. 
            Политика работы с пользовательскими данными основана на следующих принципах:
          </p>
          <ol style={{ paddingLeft: '20px', marginBottom: '8px' }}>
            <li>
              <strong>Локальность вычислений:</strong> Операционная система не отправляет фоновые телеметрические пакеты на внешние серверы и не требует обязательного подключения к глобальной сети.
            </li>
            <li>
              <strong>Отсутствие обязательных онлайн-профилей:</strong> Все учётные записи создаются и функционируют исключительно локально на вашем компьютере.
            </li>
            <li>
              <strong>Диагностические журналы ядра:</strong> Файлы трассировки (например, <code>debugkesh.log</code>) сохраняются только локально и используются исключительно для отладки аппаратной совместимости.
            </li>
          </ol>
        </div>
      </div>

      {/* Terms of Use Box */}
      <div className="ms-card" style={{ marginBottom: '16px' }}>
        <div className="ms-card-header">
          <span>Условия использования и Лицензия (Terms of Use & License)</span>
        </div>
        <div style={{ fontSize: '11px', lineHeight: '1.6', color: '#333' }}>
          <p style={{ marginBottom: '8px' }}>
            KeshOS распространяется на условиях лицензии <strong>GNU General Public License (GPL)</strong> и лицензий сопутствующих свободных компонентов открытого программного обеспечения.
          </p>
          <ul style={{ paddingLeft: '20px', marginBottom: '8px' }}>
            <li>
              <strong>Свобода использования:</strong> Допускается использование в любых личных, образовательных и исследовательских целях.
            </li>
            <li>
              <strong>Открытость исходного кода:</strong> Исходный код модификаций открыт и доступен для аудита и сборки.
            </li>
            <li>
              <strong>Отказ от гарантий (AS IS):</strong> Программное обеспечение предоставляется по принципу «КАК ЕСТЬ» (AS IS), без каких-либо явных или подразумеваемых гарантий.
            </li>
          </ul>
        </div>
      </div>

      {/* Trademarks Disclaimer */}
      <div className="ms-card">
        <div className="ms-card-header">
          <span>Уведомление о товарных знаках</span>
        </div>
        <p style={{ fontSize: '10px', lineHeight: '1.5', color: '#555' }}>
          Microsoft, MS-DOS, Windows, Windows NT, Windows 2000, Windows XP являются товарными знаками Microsoft Corporation в США и других юрисдикциях. 
          Проект KeshOS является независимой открытой разработкой, не аффилирован с корпорацией Microsoft и использует торговые наименования исключительно в целях технического описания интерфейсов совместимости.
        </p>
      </div>
    </div>
  );
};
