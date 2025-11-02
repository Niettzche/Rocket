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
  const width = 240;
  const rowHeight = 32;
  const padding = 18;
  const height = data.length * rowHeight + padding * 2;
  const zeroX = ((0 - min) / (max - min)) * width;

  return (
    <div className="chart chart--axis">
      {title && <h5 className="chart__title">{title}</h5>}
      <svg
        className="chart__surface"
        viewBox={`0 0 ${width} ${height}`}
        role="img"
        aria-label={title || 'Gráfica de ejes'}
      >
        {data.map((item, index) => {
          const y = padding + index * rowHeight + rowHeight / 2;
          const value = clamp(item.value, min, max);
          const xValue = ((value - min) / (max - min)) * width;
          const display = formatValue(item.value, decimals);

          return (
            <g key={item.label ?? index} className="chart__row">
              <line
                x1={0}
                x2={width}
                y1={y}
                y2={y}
                className="chart__baseline"
              />
              {zeroX >= 0 && zeroX <= width && (
                <line
                  x1={zeroX}
                  x2={zeroX}
                  y1={y - rowHeight / 2 + 4}
                  y2={y + rowHeight / 2 - 4}
                  className="chart__zero"
                />
              )}
              <circle cx={xValue} cy={y} r={5} className="chart__point" />
              <text
                x={xValue}
                y={y - 10}
                className="chart__value"
                textAnchor={xValue < width * 0.8 ? 'start' : 'end'}
                dx={xValue < width * 0.8 ? 8 : -8}
              >
                {display}
                {unit ? ` ${unit}` : ''}
              </text>
              <text x={0} y={y + 14} className="chart__label">
                {item.label}
              </text>
            </g>
          );
        })}
        <text x={0} y={padding - 6} className="chart__min">
          {`${formatValue(min, decimals)}${unit ? ` ${unit}` : ''}`}
        </text>
        <text x={width} y={padding - 6} className="chart__max" textAnchor="end">
          {`${formatValue(max, decimals)}${unit ? ` ${unit}` : ''}`}
        </text>
      </svg>
    </div>
  );
}

export default AxisTrackChart;
