#SE SUPONE QUE ESTO DEBE DE ESTAR DE HOME DE LA CARCACHITA.
#LoRa-RaspberryPi es la libreria que sirve con el Lora

#=============PYTHON ENV PARA LA LIB==============================
cd LoRa-RaspberryPi/
make all
export PYTHONPATH="$(pwd):$PYTHONPATH"

cd ..
cd lectura_sensores_paralelo/
python3 read_sensors.py




