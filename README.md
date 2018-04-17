# DASH7 modem for RIOT OS  
## Aan de slag
### Hardware
Verbindt de tweede UART van het bordje met de pinnen op de modem.
Op de modem:
 * PA9: TX
 * PA10: RX
 Op de Nucleo de USART-pinnen op CN9.
 
![CN9 pinout](https://imgur.com/download/zKggbXp)

Uiteraard moet TX verbonden worden met RX en omgekeerd. Vergeet ook geen GND aan te sluiten.
Sluit zowel de modem als het Nucleo-bordje aan op de PC.

Om de shell van de Nucleo-L496 te kunnen bekijken moet men een FTDI-kabel verbinden met UART1. Deze bevindt zich aan de rechterkant van CN10 op D0 en D1. Deze zijn duidelijk gelabeld.
### Software
Met de volgende instructies kan je het voorbeeld snel aan de praat krijgen.
1. Maak een werkmap aan waar je alle bestanden kan plaatsen.
2. Maak een kloon van deze repository: ``git clone git@github.com:iliessens/RIOT-oss7modem.git``
3. Maak eveneens een kloon van de *RIOT-OS* repository: ``git clone https://github.com/RIOT-OS/RIOT.git``
Wil je gebruikmaken van de Nucleo-L496, kloon dan de repository die hiervoor de nodige files bevat.
``` 
git clone git@github.com:iliessens/RIOT.git
cd RIOT
git checkout nucleo-l496zg
```

5. Je hebt nu normaal twee mappen: RIOT en RIOT-oss7modem
	* Indien je deze mappenstructuur niet gevolgd hebt: pas de variabele ``RIOTBASE`` in  ``RIOT-oss7modem/Makefile`` aan zodat deze verwijst naar de RIOT-map.
6. Open nu een terminal in de map RIOT-oss7modem. We kunnen dan de commando's uitvoeren om het programma te compileren en te flashen.
```sudo make BOARD=nucleo-l496zg flash term```


De terminal opent en je kan commando's uitvoeren. Typ ``help`` voor info over de mogelijkheden.
