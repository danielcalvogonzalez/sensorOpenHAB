Instrucciones de uso
====================
El objetivo fundamental es intentar que la gestión del código sea remota, mediante el uso
de openHAB y que no sea necesario retocarlo. En un futuro, implementará funciones OTA para
simplificar aun más su uso.

Pasos
-----

* Obtener la dirección MAC del sensor ESP8266 a usar
* Crear un *item* en openHAB con nombre M + direcciónMAC
* El estado de ese objeto, será el nombre del item al que reportará el sensor
