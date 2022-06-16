# [server-temp-monitor](https://mattVisi.github.io/server-temp-monitor)
Un sistema di monitoraggio della temperatura con allarme via email.


## Il repository
Questo repository viene utilizzato per lo sviluppo del progetto.
<br>Il branch <code>main</code> contiene l'ultima release *più o meno* stabile.
<br>Il branch <code>development</code> viene utilizzato per sviluppare il progetto aggiungendo nuove feature (e nuovi bug 😜).

### Librerie e dipendenze
A causa di un aggiornamento della libreria OneWire, la libreria [DallasTemperature by Miles Burton](https://github.com/milesburton/Arduino-Temperature-Control-Library) non era più funzionante. 
<br>
Attualmente si utilizza la libreria [DallasTemperature di Piotr Stolarz](https://github.com/pstolarz/Arduino-Temperature-Control-Library) che per la comunicazione si basa sulla libreria [OneWireNG](https://github.com/pstolarz/OneWireNg).
Per l'invio delle email si utilizzza la libreria [ESP Mail Client](https://github.com/mobizt/ESP-Mail-Client).

## Contribuire
Per lo sviluppo del firmware viene utilizzato l'ambiente di sviluppo PlatformIO.
<br>
Il cuore del progetto è un microcontrollore ESP32, la temperatura viene misurata da un sensore DS18B20.
<br>
