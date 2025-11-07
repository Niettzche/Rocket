const bridge = window.telemetryBridge;

const elements = {
  select: document.getElementById('port-select'),
  status: document.getElementById('status'),
  refresh: document.getElementById('refresh-btn'),
  auto: document.getElementById('auto-btn'),
  connect: document.getElementById('connect-btn'),
};

let cachedPorts = [];
let busy = false;

const setStatus = (message, variant = 'info') => {
  if (!elements.status) {
    return;
  }
  elements.status.textContent = message;
  elements.status.classList.toggle('status--error', variant === 'error');
  elements.status.classList.toggle('status--success', variant === 'success');
};

const setBusy = (value, label) => {
  busy = value;
  [elements.select, elements.refresh, elements.auto, elements.connect].forEach((el) => {
    if (!el) {
      return;
    }
    const noPorts = cachedPorts.length === 0;
    const shouldLock =
      value ||
      (noPorts &&
        (el === elements.select || el === elements.auto || el === elements.connect));
    el.disabled = shouldLock;
  });

  if (label) {
    setStatus(label);
  }
};

const formatPortLabel = (port) => {
  const { path, manufacturer, friendlyName, pnpId, preferred } = port;
  const meta = manufacturer || friendlyName || pnpId || '';
  const suffix = preferred ? ' (recomendado)' : '';
  return meta ? `${path} · ${meta}${suffix}` : `${path}${suffix}`;
};

const renderPorts = (ports, autoPort) => {
  cachedPorts = ports;
  elements.select.innerHTML = '';

  ports.forEach((port) => {
    const option = document.createElement('option');
    option.value = port.path || port.device || '';
    option.textContent = formatPortLabel(port);
    elements.select.append(option);
  });

  if (ports.length === 0) {
    elements.select.disabled = true;
    elements.auto.disabled = true;
    elements.connect.disabled = true;
    return;
  }

  elements.select.disabled = false;
  elements.auto.disabled = false;
  elements.connect.disabled = false;

  if (autoPort && ports.some((port) => (port.path || port.device) === autoPort)) {
    elements.select.value = autoPort;
  } else {
    elements.select.selectedIndex = 0;
  }
};

const ensureBridgeAvailable = () => {
  if (!bridge) {
    setStatus('La API de Electron no está disponible.', 'error');
    elements.connect.disabled = true;
    elements.auto.disabled = true;
    elements.refresh.disabled = true;
    return false;
  }
  return true;
};

const refreshPorts = async () => {
  if (!ensureBridgeAvailable() || busy) {
    return;
  }

  setBusy(true, 'Buscando puertos disponibles…');

  try {
    const result = await bridge.listPorts();
    const { ports = [], auto } = result ?? {};
    renderPorts(ports, auto);
    if (ports.length === 0) {
      setStatus('Conecta tu Arduino por USB y vuelve a intentarlo.', 'error');
    } else if (auto) {
      setStatus(`Se detectó ${auto}. Puedes continuar o elegir otra interfaz.`);
    } else {
      setStatus('Selecciona el puerto manualmente y presiona "Iniciar telemetría".');
    }
  } catch (error) {
    setStatus(error?.message || 'No se pudo listar los puertos.', 'error');
  } finally {
    setBusy(false);
  }
};

const connectWithPort = async (port) => {
  if (!ensureBridgeAvailable() || busy) {
    return;
  }
  if (!port) {
    setStatus('Selecciona un puerto antes de continuar.', 'error');
    return;
  }

  setBusy(true, `Conectando a ${port}…`);
  try {
    await bridge.connect(port);
    setStatus('Iniciando interfaz de telemetría…', 'success');
  } catch (error) {
    setStatus(error?.message || 'No se pudo iniciar el bridge serial.', 'error');
    setBusy(false);
  }
};

const handleAutoConnect = async () => {
  if (!ensureBridgeAvailable() || busy) {
    return;
  }

  setBusy(true, 'Detectando automáticamente…');
  try {
    const result = await bridge.autoConnect();
    if (result?.ports) {
      renderPorts(result.ports, result.port);
    }
    if (result?.port) {
      setStatus(`Se seleccionó ${result.port}. Cargando la interfaz…`, 'success');
    } else {
      setStatus('No se encontró un puerto recomendado automáticamente.', 'error');
      setBusy(false);
    }
  } catch (error) {
    setStatus(error?.message || 'No se pudo conectar automáticamente.', 'error');
    setBusy(false);
  }
};

elements.refresh?.addEventListener('click', (event) => {
  event.preventDefault();
  refreshPorts();
});

elements.connect?.addEventListener('click', (event) => {
  event.preventDefault();
  const port = elements.select?.value;
  connectWithPort(port);
});

elements.auto?.addEventListener('click', (event) => {
  event.preventDefault();
  handleAutoConnect();
});

bridge?.onBridgeError?.((payload) => {
  const message = payload?.message || 'Error inesperado en el puerto serial.';
  setStatus(message, 'error');
  setBusy(false);
});

bridge?.onBridgeStopped?.(() => {
  setBusy(false, 'La conexión serial se detuvo.');
  refreshPorts();
});

refreshPorts();
