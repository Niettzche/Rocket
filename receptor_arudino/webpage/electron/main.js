import { app, BrowserWindow, Menu, ipcMain } from 'electron';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { SerialManager } from './serialManager.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const isDev = process.env.NODE_ENV === 'development';

const preloadPath = path.join(__dirname, 'preload.cjs');
const portPickerHtml = path.join(__dirname, 'port-picker.html');
const distIndex = path.join(__dirname, '../dist/index.html');

const serialManager = new SerialManager();

let mainWindow;
let loadingTelemetry = false;

const createWindow = () => {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    minWidth: 960,
    minHeight: 640,
    backgroundColor: '#020617',
    show: false,
    webPreferences: {
      contextIsolation: true,
      preload: preloadPath,
    },
  });

  mainWindow.once('ready-to-show', () => {
    mainWindow?.show();
  });

  loadPortPicker();

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
};

const loadPortPicker = () => {
  if (!mainWindow || loadingTelemetry) {
    return;
  }
  mainWindow.loadFile(portPickerHtml);
};

const loadTelemetryUi = async () => {
  if (!mainWindow) {
    return;
  }

  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    await mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL);
    return;
  }

  await mainWindow.loadFile(distIndex);
};

const connectAndLaunch = async (port) => {
  loadingTelemetry = true;
  try {
    await serialManager.connect(port);
    await loadTelemetryUi();
  } finally {
    loadingTelemetry = false;
  }
};

ipcMain.handle('bridge:list-ports', async () => {
  return serialManager.listPorts();
});

ipcMain.handle('bridge:connect', async (_event, payload) => {
  const { port } = payload ?? {};
  if (!port) {
    throw new Error('Selecciona un puerto para continuar.');
  }
  await connectAndLaunch(port);
  return { ok: true, port };
});

ipcMain.handle('bridge:auto-connect', async () => {
  const listing = await serialManager.listPorts();
  if (!listing.auto) {
    throw new Error('No se detectó ningún puerto compatible.');
  }
  await connectAndLaunch(listing.auto);
  return { ok: true, ...listing };
});

serialManager.on('stopped', () => {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('bridge:stopped', { reason: 'disconnected' });
    loadPortPicker();
  }
});

serialManager.on('line', (payload) => {
  if (!payload) {
    return;
  }
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('bridge:serial-line', payload);
  }
});

serialManager.on('payload', (payload) => {
  if (!payload) {
    return;
  }
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('bridge:payload', payload);
  }
});

serialManager.on('error', (error) => {
  console.error('[serial-manager]', error);
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('bridge:error', { message: error.message });
  }
});

app.whenReady().then(() => {
  Menu.setApplicationMenu(null);
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('before-quit', () => {
  serialManager.disconnect().catch((error) => {
    console.error('Error al cerrar el puerto serial:', error);
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
