import { useEffect, useState } from 'react';
import SensorCard from './components/SensorCard.jsx';
import AxisTrackChart from './components/AxisTrackChart.jsx';
import ValueBarChart from './components/ValueBarChart.jsx';
import OrientationVisualizer from './components/OrientationVisualizer.jsx';
import UwUCard from './components/UwUCard.jsx';
import CommandTerminal from './components/CommandTerminal.jsx';
import payload from '../lora_payload_sample.json';
import logo from './assets/logo.png';

const ensureTrailingSlash = (value) => (value.endsWith('/') ? value : `${value}/`);

const resolvePublicAsset = (fileName) => {
  const base = ensureTrailingSlash(import.meta.env.BASE_URL ?? './');
  const fallback = `${base}${fileName}`;

  if (typeof window === 'undefined' || !window.location?.href || window.location.href === 'about:blank') {
    return fallback;
  }

  try {
    return new URL(fileName, window.location.href).href;
  } catch (error) {
    console.warn(`No se pudo resolver la ruta de ${fileName}:`, error);
    return fallback;
  }
};

const backgroundUrl = resolvePublicAsset('background.jpg');
const logoUrl = logo;

const formatTimestamp = (iso) => {
  if (!iso) return 'Sin dato';
  const date = new Date(iso);
  return Number.isNaN(date.getTime()) ? 'Formato inválido' : date.toLocaleString();
};

function App() {
  const [isReady, setIsReady] = useState(false);
  const [isTerminalOpen, setIsTerminalOpen] = useState(false);
  const {
    reported_at: reportedAt,
    sensors: { mpu6050, neo6m },
  } = payload;

  const formatTimeShort = (iso) => {
    if (!iso) return 'Sin hora';
    const date = new Date(iso);
    return Number.isNaN(date.getTime())
      ? 'Hora inválida'
      : date.toLocaleTimeString('es-MX', { hour12: false });
  };

  const toFixed = (value, decimals = 2) => {
    if (typeof value !== 'number' || Number.isNaN(value)) {
      return '—';
    }
    return value.toFixed(decimals);
  };

  useEffect(() => {
    let isMounted = true;
    let readyTimeout;
    const pendingListeners = [];

    if (typeof document !== 'undefined') {
      document.documentElement.style.setProperty('--body-bg-image', `url("${backgroundUrl}")`);
    }

    const waitForWindow = new Promise((resolve) => {
      if (document.readyState === 'complete') {
        resolve();
        return;
      }

      const handler = () => resolve();
      window.addEventListener('load', handler, { once: true });
      pendingListeners.push(() => window.removeEventListener('load', handler));
    });

    const waitForBackground = new Promise((resolve) => {
      const img = new Image();
      let settled = false;

      const settle = () => {
        if (settled) {
          return;
        }
        settled = true;
        if (img.decode) {
          img
            .decode()
            .catch(() => undefined)
            .finally(resolve);
        } else {
          resolve();
        }
      };

      const onLoad = () => settle();
      const onError = () => {
        if (!settled) {
          settled = true;
          resolve();
        }
      };

      img.addEventListener('load', onLoad, { once: true });
      img.addEventListener('error', onError, { once: true });
      pendingListeners.push(() => {
        img.removeEventListener('load', onLoad);
        img.removeEventListener('error', onError);
      });

      img.src = backgroundUrl;

      if (img.complete) {
        if (img.naturalWidth > 0) {
          settle();
        } else {
          onError();
        }
      }
    });

    Promise.all([waitForWindow, waitForBackground]).then(() => {
      if (!isMounted) {
        return;
      }

      const reveal = () => {
        readyTimeout = window.setTimeout(() => {
          if (isMounted) {
            setIsReady(true);
          }
        }, 120);
      };

      if (window.requestAnimationFrame) {
        window.requestAnimationFrame(() => {
          if (!isMounted) {
            return;
          }
          window.requestAnimationFrame(() => {
            if (!isMounted) {
              return;
            }
            reveal();
          });
        });
      } else {
        reveal();
      }
    });

    return () => {
      isMounted = false;
      pendingListeners.forEach((unsubscribe) => unsubscribe());
      if (readyTimeout) {
        window.clearTimeout(readyTimeout);
      }
    };
  }, []);

  useEffect(() => {
    const handleKeyDown = (event) => {
      const isTrigger = (event.ctrlKey || event.metaKey) && event.key?.toLowerCase() === 't';
      if (!isTrigger) {
        return;
      }

      const target = event.target;
      const isEditable =
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target?.isContentEditable;

      if (isEditable) {
        return;
      }

      event.preventDefault();
      setIsTerminalOpen(true);
    };

    window.addEventListener('keydown', handleKeyDown);

    return () => {
      window.removeEventListener('keydown', handleKeyDown);
    };
  }, []);

  return (
    <div className="app-shell">
      {!isReady && (
        <div className="splash-screen" role="status" aria-live="polite">
          <div className="splash__logo-wrap">
            <img src={logoUrl} alt="Logotipo del programa Dynx" />
            <span className="splash__ring" aria-hidden />
          </div>
          <p className="splash__label">Inicializando telemetría…</p>
        </div>
      )}

      <div className={`app ${isReady ? 'app--ready' : 'app--loading'}`}>
        <main className="app__content">
          <div className="sensor-grid">
            <SensorCard
              id="mpu6050"
              title="MPU6050"
              subtitle={`Capturado el ${formatTimestamp(mpu6050.timestamp)}`}
            >
              <div className="charts-grid charts-grid--mpu">
                <OrientationVisualizer
                  pitch={mpu6050.attitude_deg.pitch}
                  roll={mpu6050.attitude_deg.roll}
                  yaw={mpu6050.attitude_deg.yaw}
                />
                <AxisTrackChart
                  title="Acelerómetro (g)"
                  unit="g"
                  min={-4}
                  max={4}
                  data={[
                    { label: 'X', value: mpu6050.accel_g.ax },
                    { label: 'Y', value: mpu6050.accel_g.ay },
                    { label: 'Z', value: mpu6050.accel_g.az },
                  ]}
                  decimals={3}
                />
                <AxisTrackChart
                  title="Giroscopio (°/s)"
                  unit="°/s"
                  min={-500}
                  max={500}
                  data={[
                    { label: 'X', value: mpu6050.gyro_dps.gx },
                    { label: 'Y', value: mpu6050.gyro_dps.gy },
                    { label: 'Z', value: mpu6050.gyro_dps.gz },
                  ]}
                  decimals={2}
                />
                <AxisTrackChart
                  title="Actitud (°)"
                  unit="°"
                  min={-180}
                  max={180}
                  data={[
                    { label: 'Pitch', value: mpu6050.attitude_deg.pitch },
                    { label: 'Roll', value: mpu6050.attitude_deg.roll },
                    { label: 'Yaw', value: mpu6050.attitude_deg.yaw },
                  ]}
                  decimals={2}
                />
              </div>
            </SensorCard>

            <SensorCard
              id="neo6m"
              title="NEO-6M"
              subtitle={`Capturado el ${formatTimestamp(neo6m.timestamp)}`}
            >
              <div className="geo-panel">
                <div className="geo-panel__summary">
                  <div className="geo-panel__coordinates">
                    <div className="geo-panel__slot">
                      <span className="geo-panel__label">Latitud</span>
                      <span className="geo-panel__value">{neo6m.latitude}</span>
                    </div>
                    <div className="geo-panel__slot">
                      <span className="geo-panel__label">Longitud</span>
                      <span className="geo-panel__value">{neo6m.longitude}</span>
                    </div>
                  </div>
                  <div className="geo-panel__altitude">
                    <ValueBarChart
                      title="Altitud"
                      value={neo6m.altitude}
                      min={0}
                      max={2000}
                      unit="m"
                      decimals={1}
                      target={1000}
                      targetLabel="1 km"
                    />
                  </div>
                  <dl className="geo-panel__meta">
                    <div>
                      <dt>Fix</dt>
                      <dd>{formatTimestamp(neo6m.fix_time)}</dd>
                    </div>
                    <div>
                      <dt>Cadena NMEA</dt>
                      <dd>
                        <code className="nmea">{neo6m.raw}</code>
                      </dd>
                    </div>
                  </dl>
                </div>
              </div>
            </SensorCard>

            <UwUCard />
          </div>
        </main>
      </div>
      <CommandTerminal
        open={isTerminalOpen}
        onClose={() => setIsTerminalOpen(false)}
        mpu6050={mpu6050}
        neo6m={neo6m}
        reportedAt={reportedAt}
      />
    </div>
  );
}

export default App;
