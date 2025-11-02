import logoUrl from '/logo.png?url';

function Header({ lastUpdate }) {
  return (
    <header className="app__header">
      <div className="header-minimal">
        <div className="header-minimal__brand">
          <img src={logoUrl} alt="Promesa" />
          <div>
            <h1>Promesa Telemetría</h1>
            <p>Misión experimental · objetivo 1 km</p>
          </div>
        </div>
        <div className="header-minimal__status" role="status">
          <span className="status-indicator" aria-hidden />
          <span className="status-label">Última señal</span>
          <span className="status-value">{lastUpdate}</span>
        </div>
      </div>
    </header>
  );
}

export default Header;
