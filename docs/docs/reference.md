# Introduzione al progetto

---

  ATTENZIONE: Questa parte del sito potrebbe risultare incompleta in quanto ci lavoro solo nel tempo lbero. Ho deciso comunque di pubblicarla man mano che la scrivo.
  Per informazioni riguardo le parti di codice che non ho ancora coperto in questa pagina, ti invito a guardare i commenti nel [codice sorgente](https://github.com/mattVisi/server-temp-monitor).

---

L'obiettivo del progetto era quello di realizzare un sistema a basso costo e con poche necessità di manutenzione che possa montitorare la temperatura di un ambiente.

# Descrizione del progetto

### Il microcontrollore
Il cuore del sistema è un microcontrollore ESP32, che permette la connessione alla rete WiFi



## Tutto il codice passo per passo

### Il setup

All'avvio il sistema inizializza la porta seriale e stampa alcune informazioni generali sul progetto:

``` 
void setup()
{
  Serial.begin(115200);
  delay(2000);
  // while(!Serial) {;}    //Waits for the serial port to open. uSE ONLY when debugging via serial port
  
  Serial.println();
  Serial.println("### Temperature monitor with email alarm notification ###");
  Serial.println("                       Version 1.2");
  Serial.println();
  Serial.println("Copyright 2022 Matteo Visintini");
  Serial.println();
  Serial.println("This project is proudly open source, the software is distributed under the GNU General Public License");
  Serial.println();
  Serial.println("Source code, license and documentation are availble at: https://github.com/mattVisi/server-temp-monitor");
  delay(4000);
```
