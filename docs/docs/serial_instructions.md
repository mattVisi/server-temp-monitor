# Windows
Dopo aver installato PuTTY, si può collegare il sistema al PC tramite un cavo USB.

### Identificare il dispositivo
Aprire Gestione Dispositivi:  
Clicca con il tasto destro del mouse sul pulsante START nella parte bassa dello schermo e soleziona "Gestione dispositivi".

Nella sezione "Porte (COM e LPT)" trova il dispositivo "Silicon Labs CP210x USB to UART Bridge"  e segnati il numero di porta (COMx).

### Avviare il teminale
Ora apri PuTTY.  
Sotto "Connection type" seleziona "Serial".  
Nel campo "Serial line" inserisci il numero di porta che hai ottenuto nel passaggio precedente.  
Nel campo "speed" inserisci il numero `115200`.  
Cliccando su "Open" si aprirà l'interfaccia.

### Fatto!
Hai aperto con successo l'interfaccia!  Clicca [qui](manual.md#configurazione) per tornare alle istruzioni da dove le hai lasciate. 