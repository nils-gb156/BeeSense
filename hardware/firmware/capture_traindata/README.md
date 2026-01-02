
# Capture Traindata Firmware

## Funktionsweise

Dieses Programm läuft auf einem ESP32-S3 und nimmt automatisch jede Sekunde ein Bild mit einer Auflösung von 224x224 Pixeln auf. Die Bilder werden als JPEGs auf einer SD-Karte gespeichert und dienen als Trainingsdaten für KI-Anwendungen (z.B. Objekterkennung).

## Deployment

1. ESP-IDF installieren: [https://dl.espressif.com/dl/esp-idf/](https://dl.espressif.com/dl/esp-idf/)
2. In das Projektverzeichnis wechseln (Pfad anpassen):
	```
	cd C:\Users\nilsg\repos\BeeSense\hardware\firmware\capture_traindata
	```
3. Target setzen (nur beim ersten build):
	```
	idf.py set-target esp32s3
	```
4. Firmware bauen:
	```
	idf.py build
	```
5. Flashen und Monitor starten (Port ggf. anpassen):
	```
	idf.py -p COM3 flash monitor
	```