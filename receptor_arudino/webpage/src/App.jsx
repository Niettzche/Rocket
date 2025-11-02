import { useEffect, useMemo, useState } from 'react';
import Header from './components/Header.jsx';
import SensorCard from './components/SensorCard.jsx';
import AxisTrackChart from './components/AxisTrackChart.jsx';
import ValueBarChart from './components/ValueBarChart.jsx';
import OrientationVisualizer from './components/OrientationVisualizer.jsx';
import TelemetryTerminal from './components/TelemetryTerminal.jsx';
import payload from '../lora_payload_sample.json';
import backgroundUrl from '/background.jpg?url';

const formatTimestamp = (iso) => {
  if (!iso) return 'Sin dato';
  const date = new Date(iso);
  return Number.isNaN(date.getTime()) ? 'Formato inválido' : date.toLocaleString();
};

function App() {
  const [isReady, setIsReady] = useState(false);
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

  const terminalMessages = useMemo(() => {
    if (!mpu6050 || !neo6m) {
      return [];
    }

    const lines = [];
    lines.push(
      `[${formatTimeShort(reportedAt)}] paquete LoRa recibido; timestamp=${formatTimestamp(reportedAt)}`,
    );
    lines.push(
      `mpu.accel_g ax=${toFixed(mpu6050.accel_g?.ax, 3)}g ay=${toFixed(
        mpu6050.accel_g?.ay,
        3,
      )}g az=${toFixed(mpu6050.accel_g?.az, 3)}g`,
    );
    lines.push(
      `mpu.gyro_dps gx=${toFixed(mpu6050.gyro_dps?.gx, 2)}°/s gy=${toFixed(
        mpu6050.gyro_dps?.gy,
        2,
      )}°/s gz=${toFixed(mpu6050.gyro_dps?.gz, 2)}°/s`,
    );
    lines.push(
      `mpu.attitude pitch=${toFixed(mpu6050.attitude_deg?.pitch, 2)}° roll=${toFixed(
        mpu6050.attitude_deg?.roll,
        2,
      )}° yaw=${toFixed(mpu6050.attitude_deg?.yaw, 2)}°`,
    );
    lines.push(
      `gps.coords lat=${toFixed(neo6m.latitude, 5)} lon=${toFixed(neo6m.longitude, 5)} alt=${toFixed(
        neo6m.altitude,
        1,
      )}m`,
    );
    lines.push(`gps.fix ${formatTimestamp(neo6m.fix_time)}`);
    if (neo6m.raw) {
      lines.push(`gps.nmea ${neo6m.raw}`);
    }

    return lines;
  }, [mpu6050, neo6m, reportedAt]);

  useEffect(() => {
    let isMounted = true;
    let readyTimeout;
    const pendingListeners = [];

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

  return (
    <div className="app-shell">
      {!isReady && (
        <div className="splash-screen" role="status" aria-live="polite">
          <div className="splash__logo-wrap">
            <img src="/logo.png" alt="Logotipo del cohete Promesa" />
            <span className="splash__ring" aria-hidden />
          </div>
          <p className="splash__label">Inicializando telemetría…</p>
        </div>
      )}

      <div className={`app ${isReady ? 'app--ready' : 'app--loading'}`}>
        <Header lastUpdate={formatTimestamp(reportedAt)} />
        <main className="app__content">
          <div className="sensor-grid">
            <SensorCard
              id="mpu6050"
              title="MPU6050"
              subtitle={`Capturado el ${formatTimestamp(mpu6050.timestamp)}`}
            >
              <div className="charts-grid">
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

            <div className="terminal-panel" id="terminal">
              <TelemetryTerminal messages={terminalMessages} interval={2400} />
            </div>
          </div>
        </main>
      </div>
    </div>
  );
}

export default App;
