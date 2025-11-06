import { useEffect, useMemo, useRef, useState } from 'react';

const PROMPT = 'Promesa@Lora';
const INPUT_ENTRY = 'input';
const OUTPUT_ENTRY = 'output';

const isNumber = (value) => typeof value === 'number' && !Number.isNaN(value);

const formatNumber = (value, decimals = 2) => (isNumber(value) ? value.toFixed(decimals) : '—');

const formatTimestamp = (iso) => {
  if (!iso) {
    return 'Sin dato';
  }

  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) {
    return iso;
  }

  return date.toLocaleString('es-MX', {
    hour12: false,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
};

const buildAsciiTable = (mpu6050, neo6m) => {
  const rows = [
    ['Sensor', 'Métrica', 'Valor'],
    [
      'MPU6050',
      'Aceleración X (g)',
      formatNumber(mpu6050?.accel_g?.ax, 3),
    ],
    ['MPU6050', 'Aceleración Y (g)', formatNumber(mpu6050?.accel_g?.ay, 3)],
    ['MPU6050', 'Aceleración Z (g)', formatNumber(mpu6050?.accel_g?.az, 3)],
    ['MPU6050', 'Giro X (°/s)', formatNumber(mpu6050?.gyro_dps?.gx, 2)],
    ['MPU6050', 'Giro Y (°/s)', formatNumber(mpu6050?.gyro_dps?.gy, 2)],
    ['MPU6050', 'Giro Z (°/s)', formatNumber(mpu6050?.gyro_dps?.gz, 2)],
    ['MPU6050', 'Pitch (°)', formatNumber(mpu6050?.attitude_deg?.pitch, 2)],
    ['MPU6050', 'Roll (°)', formatNumber(mpu6050?.attitude_deg?.roll, 2)],
    ['MPU6050', 'Yaw (°)', formatNumber(mpu6050?.attitude_deg?.yaw, 2)],
    ['NEO-6M', 'Latitud', isNumber(neo6m?.latitude) ? neo6m.latitude.toFixed(6) : '—'],
    ['NEO-6M', 'Longitud', isNumber(neo6m?.longitude) ? neo6m.longitude.toFixed(6) : '—'],
    ['NEO-6M', 'Altitud (m)', formatNumber(neo6m?.altitude, 1)],
    ['NEO-6M', 'Fix Time', formatTimestamp(neo6m?.fix_time)],
  ];

  const widths = rows[0].map((_, columnIndex) =>
    Math.max(...rows.map((row) => String(row[columnIndex] ?? '').length)),
  );

  const separator = `+${widths.map((width) => ''.padEnd(width + 2, '-')).join('+')}+`;

  const renderRow = (row) =>
    `| ${row
      .map((cell, index) => String(cell ?? '').padEnd(widths[index], ' '))
      .join(' | ')} |`;

  return [
    separator,
    renderRow(rows[0]),
    separator,
    ...rows.slice(1).map(renderRow),
    separator,
  ].join('\n');
};

const initialMessage = [
  'Dynx Interactive Terminal lista.',
  'Escribe "help" u "ayuda" para ver los comandos disponibles.',
];

function CommandTerminal({ open, onClose, mpu6050, neo6m, reportedAt }) {
  const [entries, setEntries] = useState([]);
  const [currentInput, setCurrentInput] = useState('');
  const [history, setHistory] = useState([]);
  const [historyIndex, setHistoryIndex] = useState(-1);
  const inputRef = useRef(null);
  const logRef = useRef(null);

  useEffect(() => {
    if (open) {
      setEntries(initialMessage.map((line) => ({ type: OUTPUT_ENTRY, content: line })));
      setCurrentInput('');
      setHistoryIndex(-1);

      window.setTimeout(() => {
        inputRef.current?.focus();
      }, 0);
    }
  }, [open]);

  useEffect(() => {
    if (open && logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight;
    }
  }, [entries, open]);

  const addOutput = (lines) => {
    const payload = Array.isArray(lines) ? lines : [lines];
    setEntries((prev) => [...prev, ...payload.map((content) => ({ type: OUTPUT_ENTRY, content }))]);
  };

  const pushInputEntry = (content) => {
    setEntries((prev) => [...prev, { type: INPUT_ENTRY, content }]);
  };

  const latestSnapshot = useMemo(
    () => ({
      mpu6050,
      neo6m,
      reportedAt,
    }),
    [mpu6050, neo6m, reportedAt],
  );

  const handleClose = () => {
    onClose?.();
  };

  const handleCommand = (rawCommand) => {
    const command = rawCommand.trim();
    pushInputEntry(command);

    if (!command) {
      return;
    }

    const normalized = command.toLowerCase();
    const tokens = normalized.split(/\s+/).filter(Boolean);
    const primary = tokens[0] ?? '';

    if (primary === 'clear' || primary === 'limpiar') {
      window.setTimeout(() => {
        setEntries([]);
      }, 0);
      return;
    }

    if (['exit', 'salir', 'cerrar'].includes(primary)) {
      addOutput('Cerrando terminal…');
      window.setTimeout(handleClose, 120);
      return;
    }

    if (primary === 'help' || primary === 'ayuda') {
      addOutput([
        'Comandos disponibles:',
        '  help | ayuda          Muestra esta ayuda.',
        '  clear | limpiar       Limpia la terminal.',
        '  exit | cerrar         Cierra la terminal.',
        '  mpu [detallado]       Datos actuales del MPU6050.',
        '  neo | gps             Datos actuales del NEO-6M.',
        '  all | todos           Resumen combinado de sensores.',
        '  tabla | table         Tabla ASCII con todos los datos.',
      ]);
      return;
    }

    const snapshot = latestSnapshot;

    if (primary === 'mpu' || primary === 'mpu6050') {
      const detailed = tokens[1] === 'detallado';
      const { mpu6050: mpuData } = snapshot;

      if (!mpuData) {
        addOutput('No existen datos del MPU6050 disponibles.');
        return;
      }

      addOutput([
        `MPU6050 @ ${formatTimestamp(mpuData.timestamp)}:`,
        `  Aceleración (g): ax=${formatNumber(mpuData?.accel_g?.ax, 3)} ay=${formatNumber(mpuData?.accel_g?.ay, 3)} az=${formatNumber(mpuData?.accel_g?.az, 3)}`,
        `  Giroscopio (°/s): gx=${formatNumber(mpuData?.gyro_dps?.gx, 2)} gy=${formatNumber(mpuData?.gyro_dps?.gy, 2)} gz=${formatNumber(mpuData?.gyro_dps?.gz, 2)}`,
        `  Actitud (°): pitch=${formatNumber(mpuData?.attitude_deg?.pitch, 2)} roll=${formatNumber(mpuData?.attitude_deg?.roll, 2)} yaw=${formatNumber(mpuData?.attitude_deg?.yaw, 2)}`,
      ]);

      if (detailed) {
        addOutput(['  Datos capturados del payload bruto:']);
        addOutput(JSON.stringify(mpuData, null, 2));
      }
      return;
    }

    if (primary === 'neo' || primary === 'neo6m' || primary === 'gps') {
      const { neo6m: neoData } = snapshot;
      if (!neoData) {
        addOutput('No existen datos del NEO-6M disponibles.');
        return;
      }

      addOutput([
        `NEO-6M @ ${formatTimestamp(neoData.timestamp)}:`,
        `  Coordenadas: lat=${isNumber(neoData.latitude) ? neoData.latitude.toFixed(6) : '—'} lon=${
          isNumber(neoData.longitude) ? neoData.longitude.toFixed(6) : '—'
        }`,
        `  Altitud: ${formatNumber(neoData.altitude, 1)} m`,
        `  Fix: ${formatTimestamp(neoData.fix_time)}`,
        `  NMEA: ${neoData.raw ?? '—'}`,
      ]);
      return;
    }

    if (primary === 'all' || primary === 'todos') {
      const reportTime = formatTimestamp(snapshot.reportedAt);
      addOutput([
        `Reporte completo @ ${reportTime}`,
        `  MPU6050 -> pitch=${formatNumber(snapshot?.mpu6050?.attitude_deg?.pitch, 2)} roll=${formatNumber(
          snapshot?.mpu6050?.attitude_deg?.roll,
          2,
        )} yaw=${formatNumber(snapshot?.mpu6050?.attitude_deg?.yaw, 2)}`,
        `  NEO-6M  -> lat=${
          isNumber(snapshot?.neo6m?.latitude) ? snapshot.neo6m.latitude.toFixed(6) : '—'
        } lon=${
          isNumber(snapshot?.neo6m?.longitude) ? snapshot.neo6m.longitude.toFixed(6) : '—'
        } alt=${formatNumber(snapshot?.neo6m?.altitude, 1)} m`,
      ]);
      return;
    }

    if (primary === 'tabla' || primary === 'table') {
      addOutput(buildAsciiTable(snapshot.mpu6050, snapshot.neo6m));
      return;
    }

    addOutput(`Comando no reconocido: "${command}". Escribe "help" para ver las opciones disponibles.`);
  };

  const handleSubmit = (event) => {
    event.preventDefault();
    const command = currentInput;

    setHistory((prev) => (command.trim() ? [...prev, command] : prev));
    setHistoryIndex(-1);
    setCurrentInput('');
    handleCommand(command);
  };

  const handleKeyDown = (event) => {
    if (event.key === 'Escape') {
      event.preventDefault();
      handleClose();
      return;
    }

    if (event.key === 'ArrowUp') {
      event.preventDefault();
      if (history.length === 0) {
        return;
      }
      const nextIndex = historyIndex < 0 ? history.length - 1 : Math.max(historyIndex - 1, 0);
      const value = history[nextIndex] ?? '';
      setHistoryIndex(nextIndex);
      setCurrentInput(value);
      window.setTimeout(() => {
        const target = inputRef.current;
        if (target) {
          target.setSelectionRange(value.length, value.length);
        }
      }, 0);
    } else if (event.key === 'ArrowDown') {
      event.preventDefault();
      if (history.length === 0) {
        return;
      }
      const nextIndex = historyIndex < 0 ? -1 : Math.min(historyIndex + 1, history.length - 1);
      setHistoryIndex(nextIndex);
      setCurrentInput(nextIndex >= 0 ? history[nextIndex] ?? '' : '');
      window.setTimeout(() => {
        const value = nextIndex >= 0 ? history[nextIndex] ?? '' : '';
        inputRef.current?.setSelectionRange(value.length, value.length);
      }, 0);
    }
  };

  if (!open) {
    return null;
  }

  return (
    <div className="command-terminal" role="dialog" aria-modal="true" aria-label="Terminal interactiva Dynx">
      <div className="command-terminal__backdrop" onClick={handleClose} />
      <div className="command-terminal__window" role="document">
        <header className="command-terminal__header">
          <span className="command-terminal__title">Dynx · Terminal</span>
          <button type="button" className="command-terminal__close" onClick={handleClose}>
            ×
          </button>
        </header>
        <div className="command-terminal__log" ref={logRef}>
          {entries.map((entry, index) => {
            if (entry.type === INPUT_ENTRY) {
              return (
                <div key={`input-${index}-${entry.content}`} className="command-terminal__line">
                  <span className="command-terminal__prompt">{PROMPT}$</span>
                  <span className="command-terminal__input">{entry.content}</span>
                </div>
              );
            }

            return (
              <pre key={`output-${index}-${entry.content}`} className="command-terminal__output">
                {entry.content}
              </pre>
            );
          })}
        </div>
        <form className="command-terminal__form" onSubmit={handleSubmit}>
          <label className="command-terminal__label" htmlFor="command-terminal-input">
            <span className="command-terminal__prompt">{PROMPT}$</span>
          </label>
          <input
            id="command-terminal-input"
            ref={inputRef}
            className="command-terminal__input-field"
            value={currentInput}
            onChange={(event) => setCurrentInput(event.target.value)}
            onKeyDown={handleKeyDown}
            autoComplete="off"
            spellCheck={false}
          />
        </form>
      </div>
    </div>
  );
}

export default CommandTerminal;
