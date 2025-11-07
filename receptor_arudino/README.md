# Receptor Arduino + GUI Electron

Este repo contiene dos piezas:

1. `receptor_rocket_system.ino`: firmware que arma los frames LoRa y escribe por `Serial`.
2. `webpage/`: interfaz en React/Electron que consume el archivo `lora_payload_sample.json`.

La app de Electron incluye un backend propio que:

- enumera los puertos seriales disponibles;
- auto-detecta Arduino Nano/CH340, o permite elegir manualmente la interfaz;
- abre la conexión serial, escucha los mensajes del sketch (`===== Payload recibido =====`) y actualiza `webpage/lora_payload_sample.json` para que la UI vea la telemetría real.

El script `serial_bridge.py` permanece disponible si quieres correr la captura desde una terminal sin abrir Electron.

## Requisitos

- Node 18+ y npm para la app Electron.
- (Opcional) Python 3.9+ con `pyserial` si deseas usar `serial_bridge.py` manualmente.

## Flujo recomendado (detector automático en Electron)

1. Compila y sube `receptor_rocket_system.ino` a tu Arduino Nano.
2. Conecta el Arduino por USB a la laptop.
3. Inicia la app de escritorio:

   ```bash
   cd webpage
   npm install
   npm run dev:electron   # para desarrollo (usa Vite)
   # o npm run start:electron después de npm run build
   ```

4. Antes de cargar la UI principal aparecerá un selector de puertos:
   - Detecta automáticamente interfaces tipo Arduino/CH340 (USB CDC, CH340, CP210, etc.).
   - Te permite elegir manualmente el puerto correcto si hay varios.
   - Al confirmar, Electron inicia la comunicación serial y, tras la conexión, carga la interfaz React/Vite con los datos que vaya recibiendo.
   - En la interfaz, presiona `Ctrl+S` para abrir el monitor serial en vivo y ver las líneas crudas que llegan desde el Arduino.

## Uso manual del bridge (opcional)

Si prefieres lanzar el bridge sin abrir Electron (por ejemplo para scripts rápidos), el archivo `serial_bridge.py` expone la misma heurística:

```bash
pip install pyserial  # una sola vez

# Listar puertos detectados en JSON
python3 serial_bridge.py --list-ports

# Autodetectar y arrancar
python3 serial_bridge.py --auto

# O especificar el puerto manualmente
python3 serial_bridge.py --port /dev/ttyUSB0 --mirror --log-level DEBUG
```

- `--mirror` imprime cada payload recibido.
- `--output` te deja persistir en otro JSON.
- `--auto` usa la misma heurística que el selector de Electron para encontrar un Arduino Nano/USB-Serial.

## Siguientes pasos opcionales

- El script actualmente persiste el último payload. Puedes extenderlo para emitir WebSockets/HTTP si prefieres actualizar la UI sin recargar.
- Para empaquetar todo, usa `npm run build` y luego `npm run start:electron` una vez que el bridge esté en ejecución (puede compilarse a binario con PyInstaller si quieres distribuirlo).
