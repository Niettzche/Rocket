const { contextBridge, ipcRenderer } = require('electron');

const invoke = (channel, payload) => ipcRenderer.invoke(channel, payload);

const on = (channel, handler) => {
  if (typeof handler !== 'function') {
    return () => {};
  }

  const wrapped = (_event, payload) => handler(payload);
  ipcRenderer.on(channel, wrapped);
  return () => {
    ipcRenderer.removeListener(channel, wrapped);
  };
};

contextBridge.exposeInMainWorld('telemetryBridge', {
  listPorts: () => invoke('bridge:list-ports'),
  connect: (port) => invoke('bridge:connect', { port }),
  autoConnect: () => invoke('bridge:auto-connect'),
  onBridgeStopped: (handler) => on('bridge:stopped', handler),
  onBridgeError: (handler) => on('bridge:error', handler),
  onSerialLine: (handler) => on('bridge:serial-line', handler),
  onPayload: (handler) => on('bridge:payload', handler),
});
