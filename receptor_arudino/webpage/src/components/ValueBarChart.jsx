const formatValue = (value, decimals = 2) => {
  if (value == null || Number.isNaN(value)) {
    return '—';
  }
  return value.toFixed(decimals);
};

function ValueBarChart({
  title,
  value,
  min,
  max,
  unit,
  decimals = 2,
  target,
  targetLabel,
}) {
  const normalized = (() => {
    if (value == null || Number.isNaN(value)) {
      return 0;
    }
    if (max === min) {
      return 0;
    }
    return (value - min) / (max - min);
  })();

  const clamped = Math.min(Math.max(normalized, 0), 1);
  const targetNormalized = (() => {
    if (target == null || Number.isNaN(target) || max === min) {
      return null;
    }
    const normalizedTarget = (target - min) / (max - min);
    if (Number.isNaN(normalizedTarget)) {
      return null;
    }
    return Math.min(Math.max(normalizedTarget, 0), 1);
  })();

  return (
    <div className="chart chart--value">
      {title && <h5 className="chart__title">{title}</h5>}
      <div className="chart__bar" aria-hidden>
        <span
          className="chart__bar-progress"
          style={{ width: `${clamped * 100}%` }}
          role="presentation"
        />
        {targetNormalized != null && (
          <span
            className="chart__bar-target"
            style={{ left: `${targetNormalized * 100}%` }}
            aria-hidden
          />
        )}
      </div>
      <div className="chart__value-display">
        <span>
          {formatValue(value, decimals)}
          {unit ? ` ${unit}` : ''}
        </span>
        <span className="chart__range">
          {formatValue(min, decimals)} – {formatValue(max, decimals)}
          {unit ? ` ${unit}` : ''}
        </span>
      </div>
      {targetNormalized != null && (
        <p className="chart__target-label">
          Meta: {formatValue(target, decimals)}
          {unit ? ` ${unit}` : ''}
          {targetLabel ? ` (${targetLabel})` : ''}
        </p>
      )}
    </div>
  );
}

export default ValueBarChart;
