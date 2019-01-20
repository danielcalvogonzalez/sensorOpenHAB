# openHAB sensor
[![Maintainer](http://img.shields.io/badge/maintainer-@dan_calvo-blue.svg?style=flat-square)](https://twitter.com/dan_calvo)

C�digo **Arduino** para implementar la lectura de sensores DHT22 y reportar su valor al servidor openHAB.

El principal prop�sito es did�ctico, por lo que el c�digo no est� optimizado al 100%.

## Notas
Para un corrector funcionamiento necesita de un fichero **wifi.h**. Este fichero debe definir el valor de:

```
#define mySSID "identificadorSSID"
#define mySSID_PASSWORD "Password acceso a la Wifi"
```
