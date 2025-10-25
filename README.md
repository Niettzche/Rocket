# Rocket System

## Descripcion general
Este repositorio contiene el software que captura lecturas de sensores (IMU MPU6050, barometro BMP180 y GPS NEO6M), las agrega y las publica mediante un radio LoRa. Incluye:
- Implementacion en Python que coordina los hilos de cada sensor, arma el paquete y lo envia o recibe por LoRa.
- Un prototipo equivalente en C dentro de `rocket_c/` para entornos con recursos limitados.
- Archivos auxiliares para ejecutar el servicio en una Raspberry Pi (por ejemplo `start.sh` y `read_sensors.service`).

El punto de entrada principal es `read_sensors.py`, que puede operar como transmisor o receptor segun la configuracion de LoRa.

## Requisitos previos
- Python 3.9 o superior.
- Acceso a los sensores conectados y los modulos Python que los controlan (`acceleration.py`, `BMP180.py`, `neo3.py`).
- Libreria `loralib` compilada para la plataforma objetivo. El proyecto asume la estructura provista por el repositorio `LoRa-RaspberryPi` (ver seccion de preparacion).

## Preparacion del entorno
1. Compila la libreria de LoRa:
   ```bash
   git clone https://github.com/HelTecAutomation/LoRa-RaspberryPi.git
   cd LoRa-RaspberryPi
   make all
   ```
2. Expone la libreria al interprete de Python (ajusta la ruta segun tu entorno):
   ```bash
   export PYTHONPATH="/ruta/a/LoRa-RaspberryPi:$PYTHONPATH"
   ```
3. Regresa al directorio del proyecto `rocket_system`.
4. (Opcional) Crea un entorno virtual e instala dependencias adicionales si las necesitas para tus sensores:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -U pip
   ```

Puedes automatizar los pasos 1 y 2 con el script `start.sh`, pero revisa y actualiza las rutas antes de usarlo.

## Configuracion de LoRa
El archivo `lora_config.json` controla el modo de operacion y los parametros de la radio.

Campos principales:
- `mode`: `"tx"` para transmitir lecturas de sensores, `"rx"` para escuchar paquetes.
- `frequency_hz`: frecuencia de operacion (por defecto 433000000 Hz).
- `spread_factor`: factor de dispersion admitido por el modulo (7 a 12).
- `poll_interval`: retardo entre lecturas cuando se esta en modo receptor.
- `frame_timeout`: tiempo maximo para recomponer paquetes fragmentados.

El programa valida estos campos y recurre a valores por defecto si encuentra datos invalidos.

## Ejecucion
1. Ajusta `lora_config.json` al modo deseado.
2. Desde la raiz del repositorio ejecuta:
   ```bash
   python3 read_sensors.py
   ```

En modo `tx` el proceso:
- Inicializa LoRa en transmision.
- Inicia un hilo por sensor y uno para el agregador.
- Publica cada 0.5 s un resumen con los ultimos valores de cada sensor.
- Detecta una condicion de aceleracion cero con la IMU y registra cuando envia esa senal.

En modo `rx` el proceso:
- Inicializa LoRa en recepcion.
- Itera sobre `receive_loop`, imprime los paquetes recibidos y reporta el RSSI/SNR.

En ambos casos, las salidas se escriben en consola con colores para facilitar la lectura. El resumen final indica que sensores enviaron datos reales, cuales usaron valores simulados y si se detecto la condicion de aceleracion cero.

Para detener el servicio usa `Ctrl+C`. El programa captura SIGINT/SIGTERM y cierra los hilos de forma ordenada.

## Despliegue como servicio
El archivo `read_sensors.service` ofrece un ejemplo de unidad de systemd. Ajusta `User`, `Group`, `WorkingDirectory` y `ExecStart` a las rutas de tu sistema y copia el archivo a `/etc/systemd/system/`. Luego ejecuta:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now read_sensors.service
```

## Modulo en C (`rocket_c/`)
El directorio `rocket_c/` contiene una version en C del agregador. Para compilarla:
```bash
cd rocket_c
make
```
El ejecutable generado es `rocket_sensors`. Los objetos intermedios se guardan en `rocket_c/build/` y estan ignorados en Git.

## Comandos utiles
- `python3 read_sensors.py` — lanza el agregador principal.
- `make -C rocket_c clean` — limpia los binarios del proyecto en C.
- `journalctl -u read_sensors.service -f` — sigue los logs en despliegues con systemd.

## Estructura del repositorio
- `read_sensors.py`: orquestador principal y punto de entrada.
- `sensor_workers.py`: hilos de cada sensor y generacion de datos dummy cuando no hay hardware.
- `aggregator.py`: combinacion de mediciones, deteccion de aceleracion cero y envio por LoRa.
- `lora_transport.py`: adaptador sobre `loralib` para inicializar radio, enviar y recibir tramas.
- `logger.py`, `summaries.py`, `sensor_messages.py`: utilidades para logging y formateo de payloads.
- `rocket_c/`: implementacion equivalente en C.
- `start.sh`, `read_sensors.service`: ejemplos de scripts de despliegue.

Con lo anterior deberias tener todo lo necesario para ejecutar el sistema y adaptar la configuracion a tu entorno.
