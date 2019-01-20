Instrucciones de uso
====================
El objetivo fundamental es intentar que la gestión del código sea remota, mediante el uso
de openHAB y que no sea necesario retocarlo. En un futuro, implementará funciones OTA para
simplificar aun más su uso.

Pasos
-----

* Obtener la dirección MAC del sensor ESP8266 a usar
* Crear un **item** en openHAB con nombre M + direcciónMAC
* El estado de ese objeto, será el nombre del item al que reportará el sensor
* Cuando se inicializa el sensor, construye el nombre del item e interroga a openHAB por el nombre público del sensor
* Toda la comunicación posterior empleará el nombre anterior
* El valor del item **PollIntervalSensors** indica el intervalo (en segundos) en el que se actualizarán los valores de los sensores

