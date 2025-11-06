const clamp = (value, min, max) => {
  if (Number.isNaN(value)) {
    return min;
  }
  return Math.min(Math.max(value, min), max);
};

const formatValue = (value, decimals = 2) => {
  if (value == null || Number.isNaN(value)) {
    return '—';
  }
  return value.toFixed(decimals);
};

function AxisTrackChart({
  data,
  min,
  max,
  unit,
  decimals = 2,
  title,
}) {
  const range = max - min || 1;
  const zeroRatio = min < 0 && max > 0 ? (0 - min) / range : null;

  return (
    <div className="chart chart--axis">
      {title && <h5 className="chart__title">{title}</h5>}
      <div className="axis-chart" role="group" aria-label={title || 'Gráfica de ejes'}>
        {data.map((item, index) => {
          const value = clamp(item.value, min, max);
          const display = formatValue(item.value, decimals);
          const valueRatio = (value - min) / range;
          const valuePct = Math.max(0, Math.min(100, valueRatio * 100));
          const originPct = zeroRatio != null ? zeroRatio * 100 : min >= 0 ? 0 : 100;
          const left = Math.min(originPct, valuePct);
          const fillWidth = Math.max(Math.abs(valuePct - originPct), 1.2);

          return (
            <div key={item.label ?? index} className="axis-chart__row">
              <span className="axis-chart__label">{item.label}</span>
              <div className="axis-chart__bar">
                {zeroRatio != null && (
                  <span className="axis-chart__zero" style={{ left: `${zeroRatio * 100}%` }} />
                )}
                <span
                  className="axis-chart__fill"
                  style={{ left: `${left}%`, width: `${fillWidth}%` }}
                />
                <span className="axis-chart__marker" style={{ left: `${valuePct}%` }} />
              </div>
              <span className="axis-chart__value">
                {display}
                {unit ? ` ${unit}` : ''}
              </span>
            </div>
          );
        })}
      </div>
      <div className="axis-chart__scale">
        <span>{`${formatValue(min, decimals)}${unit ? ` ${unit}` : ''}`}</span>
        <span>{`${formatValue(max, decimals)}${unit ? ` ${unit}` : ''}`}</span>
      </div>
    </div>
  );
}

export default AxisTrackChart;
