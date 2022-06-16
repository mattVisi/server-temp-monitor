# Istruzioni per l'uso

## Avvio del sistema
Per avviare il sistema basta collegarlo tramite un cavo Micro USB ad un alimentatore che fornisca almeno 1A di corrente a una tensione di 5V.  
Il microcontrollore avvierà automaticamente il software caricato in memoria, inizializzerà il sensore di temperatura e proverà a connettersi alla rete WiFi con le impostazioni salvate precedentemente nella configurazione.


Durante il processo di avvio il LED di indicazione lampeggerà velocemente di colore BLU.

---
NOTA: Nel caso il sistema venisse disconnesso dalla rete WiFi, verrà riavviato continuamente fino a che non sarà stabilita una connessione.

---

#### Primo utilizzo
Per utilizzare il sistema è necessario collegarlo a una rete WiFi. Se è la prima volta che utilizzi il sistema, devi configurare la connessione di rete e qualunque altra impostazione desideri tramite l'[interfaccia di configurazione](#configurazione).

---

## LED di indicazione
Il led RGB integrato ti permette di sapere lo stato del sistema in base al colore e la velocità con cui lampeggia.

| **Colore** | **Descrizione del lampeggio** | **Stato**                      |
|:----------:|:-----------------------------:|:------------------------------:|
| Blu        | Molto veloce                  | Avvio del sistema              |
| Verde      | Breve, ogni secondo           | Sistema in funzione            |
| Giallo     | Breve, ogni secondo           | Soglia di pre allarme superata |
| Rosso      | Breve, ogni secondo           | Soglia di allarme superata     |
| Rosso      | Veloce                        | Guasto al sensore              |
| Blu        | Breve, ogni secondo           | Modalità di configurazione     |

---

## Configurazione
Per la configurazione del sistema è necessario collegarlo a un computer tramite un cavo USB e accedere all'interfaccia seriale (baud rate 115200) tramite un emulatore di terminale. (Consigliamo l'utilizzo del software [PuTTY](https://putty.org/)).  
-> [Istruzioni per utenti Windows](serial_instructions.md#windows)

Per avviare la modalità di configurazione tieni premuto il pulsante di configurazione per 3 secondi, il LED di indicazione inizierà a lampeggiare di colore blu e si avvierà il pannello di configurazione.

---
NOTA: Durante il processo di avvio del sistema è possibile tener premuto per 3 secondi il pulsante di configurazione. Anche se sul momento potrebbe sembrare che non accada nulla, appena il processo di avvio sarà completato, il sistema entrerà in modalità di configurazione.

---

### Il pannello di configurazione
Il pannello di configurazione si presenta come una serie di campi da compilare o lasciare vuoti (in tal caso il valore precedentemente impostato rimarrà in memoria), che vengono presentati all'utente divisi in tre sezioni:  

+ Network Configuration - Configurazioni che riguardano la connessione WiFi  
+ Temperature and alarm configuration - Impostazione delle soglie di temperatura e delle allarmi
+ Email configuration - Impostazioni che riguardano il servizio email

Per ogni campo il valore tra parentesi () è il valore attualmente presente in memoria. Per confermarlo premere < Invio > senza inserire nulla.

#### Network configuration
`SSID (your_ssid):`  Il nome della rete alla quale ci si vuole collegare  
  
`Are you using enterprise login? yes/no (no):`  Metodo di accesso alla rete WiFi ([Ulteriori informazioni](https://it.wikipedia.org/wiki/Wi-Fi_Protected_Access#Terminologia))  
  
+ Se avrai risposto `no` alla domanda precedente, ti verrà chiesto di inserire la password della rete alla quale vuoi accedere:

`Password (your_password):`

+ Se invece utilizzi un metodo di accesso enterprise, ti verranno richieste le credenziali di accesso:

`  User ID (your_ID):`  
`  Username (your_username):`  
`  Password (your_password):`  


#### Temperature and alarm configuration

`Pre alarm temperature (30.00°C ):`  La temperatura alla quale il sistema invierà le email di attenzione.

`Alarm temperature (35.00°C ):`  Temperatura sopra la quale il sistema invierà le email di allarme

`Alarm reset threshold (1.00°C ):`  Temperatura sottratta alla soglia sotto la quale rientra lo stato di allarme corrente.

`Intervall between mesurements (10 seconds):`  Intervallo di tempo, in secondi, tra le misurazioni della temperatura.

`Time intervall between alarm emails (2 minutes):`  Intervallo di tempo, in minuti, tra l'invio di email consecutive relative allo stesso stato di allarme.

#### Email configuration

`SMTP server address (smtp.gmail.com):`  Indirizzo del server SMTP utilizzato per inviare le email.

`SMTP port (587):`  Porta del server SMTP alla quale collegarsi.

`Sender email address (you@company.com):`  Indirizzo dal quale verranno inviate le email.

`Sender password (p******d):`  Password di accesso al server SMTP

`Sender name (ESP32 - Server temp monitor):`  Nome del mittente

`Recipient email address (someone@agency.net):`  Indirizzo email del destinatario

`Time intervall between I'm alive emails (1 hours):`  Il sistema invia un'email che ne assicura il corretto funzionamento a intervalli regolari. Qua puoi configurare l'intervallo di tempo (espresso in ore) tra l'invio di queste email.

###### Email di test

`Do you want to send a test email? yes/no:`

Se si risponde `yes` il sistema invia una email di prova con le impostazioni appena settate.

---
 NOTA: Se ci sono state delle modifiche alla configurazione di rete, il sistema non può inviare una email di prova, perchè la connessione alla rete WiFi avviene soltanto all'avvio.  
 In tal caso puoi concludere la configurazione, ritornare qua dopo che il sistema si sarà riavviato senza modificare ulteriormente la configurazione di rete e inviare quindi l'email di prova

---

###### Fine della configurazione

Alla fine della procedura il sistema stamperà un resoconto della configurazione impostata e chiederà:

`Confirm the current configuration? yes/no:`

Se si risponde `yes` il sistema si riavvierà con le impostazioni appena settate.  
Se si risponde `no` si ritornerà all'inizio del pannello di configurazione per poter modificare nuovamente i paramentri.