import { useEffect, useRef } from 'react';

const formatTime = (iso) => {
  if (!iso) {
    return '--:--:--';
  }
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) {
    return iso;
  }
  return date.toLocaleTimeString('es-MX', { hour12: false });
};

function SerialMonitor({ open, lines, onClose, onClear }) {
  const logRef = useRef(null);

  useEffect(() => {
    if (!open) {
      return undefined;
    }

    const handleKeyDown = (event) => {
      if (event.key === 'Escape') {
        event.preventDefault();
        onClose?.();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [open, onClose]);

  useEffect(() => {
    if (open && logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight;
    }
  }, [lines, open]);

  if (!open) {
    return null;
  }

  return (
    <div className="serial-monitor" role="dialog" aria-modal="true">
      <div className="serial-monitor__panel">
        <header className="serial-monitor__header">
          <div>
            <p className="serial-monitor__eyebrow">Monitor serial</p>
            <h2>Lecturas en vivo</h2>
          </div>
          <button type="button" className="serial-monitor__close" onClick={onClose}>
            Cerrar (Esc)
          </button>
        </header>

        <div className="serial-monitor__log" ref={logRef}>
          {lines.length === 0 ? (
            <p className="serial-monitor__placeholder">Aún no se reciben lecturas del puerto.</p>
          ) : (
            <ul>
              {lines.map((entry, index) => (
                <li key={`${entry.timestamp}-${index}`}>
                  <span className="serial-monitor__timestamp">{formatTime(entry.timestamp)}</span>
                  <code>{entry.text ?? entry.raw ?? ''}</code>
                </li>
              ))}
            </ul>
          )}
        </div>

        <footer className="serial-monitor__footer">
          <button type="button" onClick={onClear}>
            Limpiar
          </button>
          <span className="serial-monitor__hint">Ctrl+S para volver a abrir este monitor</span>
        </footer>
      </div>
    </div>
  );
}

export default SerialMonitor;

