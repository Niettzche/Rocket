function SensorCard({ id, title, subtitle, children }) {
  return (
    <section className="sensor-card" id={id}>
      {(title || subtitle) && (
        <header className="sensor-card__header">
          {title && <h3>{title}</h3>}
          {subtitle && <span className="sensor-card__timestamp">{subtitle}</span>}
        </header>
      )}
      <div className="sensor-card__body">{children}</div>
    </section>
  );
}

export default SensorCard;
