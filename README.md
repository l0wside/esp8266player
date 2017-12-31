esp8266player - Kindgerechte Steuerung für einen Raumfeld One S
===============================================================

Motivation
----------
Der Player ist inspiriert von der [Toniebox](https://tonies.de/). Wer´s nicht kennt: das Kind hat ein Gerät, darauf stellt es eine Figur (die eine RFID-Kennung enthält), das Gerät lädt sich vom Server die zughörige Audiodatei und spielt los. Kein Gefummel mit CDs, die sowieso immer zerkratzen.

Nachteile an diesem System: man ist auf die Firma angewiesen, ohne Tonies keine Toniebox. Ohne Login geht nichts. Und: alle vorhandenen CDs (und bei uns sind das eine Menge) taugen nur noch als Frisbee oder Briefbeschwerer, eigene Inhalte lassen sich nur schwer wiedergeben.

Umsetzung
---------
Mit der Soft- und Hardware in diesem Projekt lässt sich ein Raumfeld One S per RFID fernsteuern. Dazu wird die sog. URI, die einen Track eindeutig bezeichnet, auf der Mifare-Karte gespeichert. Wird diese Karte dann auf den Player gelegt, sendet diese die URI an den Raumfeld One S, der diese URi dann abspielt. 
Vorteil: alles, was man mit der Raumfeld-App abspielen kann, lässt sich auch auf der Karte speichern. Theoretisch jedenfalls, ich habe es nur mit gerippten CDs auf dem NAS getestet.

Die Bedienung ist maximal simpel: Karte auflegen, ein paar Sekunden warten, Raumfeld dudelt los.

Programmiert werden die Karten ganz einfach: mit der Raumfeld-App einen Track auswählen. Wenn er spielt, auf den Player eine leere Mifare 1k-Karte legen. Der Player erkennt diese und schreibt die URI des aktuellen Tracks darauf. 

Wird die Karte nun wieder aufgelegt, wird der Track als neue URI an den Raumfeld gesendet, und dieser beginnt zu spielen. Wird die Karte weggenommen, wird ein Stop-Befehl an den Raumfeld gesendet.
Alle 20s wird die aktuelle Tracknummer abgefragt und (bei Änderung) auf der Karte gespeichert, so dass, wenn die Karte erneut aufgelegt wird, die CD nicht wieder von vorne losläuft. Was passiert, wenn ein Radiosender oder Spotify eingestellt ist, ist nicht getestet.

Bei uns sind statt RFID-Karten Aufkleber im Einsatz, die auf Holzwürfel geklebt werden. Irgendwann beschrifte ich die noch mit dem Laser.

Sonos und Co
------------
Implementiert ist ganz normales UPnP, das vermutlich mit einer Änderung des initialen Ports (bei Raumfeld 47365) auch mit Sonos läuft. Mangels Sonos habe ich es aber nicht getestet, ich freue mich auf Pull Requests.

Software
--------
Ein erster Umsetzungsversuch mit MicroPython scheiterte am komplett unbenutzbaren Netwerkstack von uPy auf ESP8266. Jetzt basiert die Software auf Arduino. Für RFID nutze ich die Lib von Miguel Balboa, der Zugriff auf den Raumfeld (SOAP über HTTP) ist selbst implementiert.

Hardware
--------
Basis ist ein ESP8266 (ESP13) auf dem im Repository enthaltenen Board. Achtung: der Schaltregler auf dem Board funktioniert zwar prinzipiell, bräuchte aber eine Spule, die größer ist, als der Platz erlaubt. Eine Vorgängerversion hatte mal einen LP3874-3.3 (Linearregler) eingebaut, der funktionierte erheblich besser.
Für Lauter, Leiser, Vor, Zurück sind insgesamt vier Touchfelder vorhanden, die mit Azoteq IQS127D ausgewertet werden. Wer sich das nicht traut, kann auch normale Taster nehmen, muss aber dafür sorgen, dass die Bootkonfiguration weiterhin funktioniert.
Für RFID kommt ein Chinaboard mit RC522 zum Einsatz, wie sie für rund 5 EUR z.B. bei eBay zu bekommen sind.

To Do
-----
* Hardware
  * Nicht funktionierenden Schaltregler sanieren oder durch Linearregler ersetzen
  * Einspeisung über Micro-USB-Buchse statt fest verlöteten Anschluss
  * Die Pinreihenfolge beim Anschluss des RFID-Boards müsste umgedreht werden, dann lässt sich 1:1 verbinden
  * Alternativ: RC522 direkt auf dem Board bestücken und Antenne integrieren.
* Software
  * Ist der Raumfeld im Standby, wird er nicht geweckt. 
  * Gelegentlich funktioniert die Erkennung der RFID-Karten nach einer Weile nicht mehr.
  * Als Status-LED wäre eine WS2812 prima, die dann farblich anzeigen könnte, wie es dem Gerät geht (suche WLAN, suche Raumfeld, Raumfeld gefunden, Karte im Feld, Raumfeld spielt). Das aktuelle Geblinke der blauen LED ist eher nervig.
  * Lauter/Leiser funktioniert nur mittelprächtig (in der App ist das viel besser). Wahrscheinlich wäre es sinnvoll, dafür eine permanent offene HTTP-Verbindung zu halten (also nicht Connection: close, sondern Connection: keep-alive) und mit inkrementellen statt absoluten Lautstärkebefehlen zu arbeiten.