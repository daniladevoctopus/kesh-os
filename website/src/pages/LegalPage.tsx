import React from 'react';

export const LegalPage: React.FC = () => {
  return (
    <div className="cc-page">
      <section className="cc-section" style={{ paddingTop: 32 }}>
        <div className="cc-section-header">
          <span className="cc-section-tag">Правовая информация</span>
          <h2>Лицензионные условия и конфиденциальность</h2>
          <p className="cc-lead" style={{ marginTop: 8 }}>
            Политика работы с пользовательскими данными, условия свободной лицензии и уведомления о товарных знаках.
          </p>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: 24 }}>
          {/* Privacy Statement */}
          <div className="cc-card">
            <div className="cc-card-title">Положение о конфиденциальности</div>
            <div className="cc-card-desc">
              <p style={{ marginBottom: 12 }}>
                KeshOS придерживается строгого принципа локальности данных:
              </p>
              <ul style={{ paddingLeft: 20, display: 'flex', flexDirection: 'column', gap: 8 }}>
                <li>
                  <strong>Локальность вычислений:</strong> Операционная система не передаёт скрытые телеметрические пакеты на внешние серверы и не требует обязательного подключения к сети.
                </li>
                <li>
                  <strong>Локальные профили:</strong> Все учётные записи создаются и хранятся исключительно на физическом устройстве пользователя.
                </li>
                <li>
                  <strong>Диагностические журналы:</strong> Логи ядра (например, <code>debugkesh.log</code>) формируются локально для отладки драйверов и отправляются разработчикам только по личному решению пользователя.
                </li>
              </ul>
            </div>
          </div>

          {/* Terms of Use */}
          <div className="cc-card">
            <div className="cc-card-title">Условия использования и лицензия</div>
            <div className="cc-card-desc">
              <p style={{ marginBottom: 12 }}>
                KeshOS распространяется на условиях лицензии <strong>GNU General Public License (GPL)</strong> и лицензий сопутствующих открытых компонентов:
              </p>
              <ul style={{ paddingLeft: 20, display: 'flex', flexDirection: 'column', gap: 8 }}>
                <li>
                  <strong>Свободное применение:</strong> Допускается использование в личных, образовательных и исследовательских целях.
                </li>
                <li>
                  <strong>Открытый исходный код:</strong> Модификации и код проекта открыты для аудита и сборки в публичном репозитории.
                </li>
                <li>
                  <strong>Предоставление «как есть»:</strong> Программное обеспечение предоставляется на условиях «AS IS», без гарантий пригодности для коммерческой эксплуатации в критических системах.
                </li>
              </ul>
            </div>
          </div>

          {/* Trademarks */}
          <div className="cc-card">
            <div className="cc-card-title">Уведомление о товарных знаках</div>
            <div className="cc-card-desc">
              Microsoft, MS-DOS, Windows, Windows NT, Windows 2000, Windows XP являются товарными знаками Microsoft Corporation. Проект KeshOS является независимой разработкой на базе ReactOS и ядра NT, не аффилирован с корпорацией Microsoft и использует наименования исключительно в целях технического описания совместимости.
            </div>
          </div>
        </div>
      </section>
    </div>
  );
};
