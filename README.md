# 2-18-aikataulunaytto

Oheisesta repositoriosta löytyy muokattu mallikoodi Mikrobitin numerossa 2/18 ilmestyneeseen tee-se-itse -aikataulunäyttöön. Alkuperäinen koodi on [täällä](https://github.com/Mikrobitti/bussiaikataulunaytto-MB1802). Automaattisesti päivittyvä bussiaikataulunäyttö pelastaa kiireisen työn sankarin aamun kertomalla tarkalleen, koska seuraava bussi kotipysäkin ohi pyyhältää.

Bussinäyttö koostuu e-paperinäytöstä, Wemos D1 Mini -mikrokontrollerista, litiumakusta latauspiireineen sekä 3D-tulostettavasta kotelosta. Wemos-mikrokontrollerin voi ohjelmoida helposti tuttuja Arduino-komentoja apuna käyttäen.

## Tiedostot

```
.
├── aikataulunaytto.ino                     # koko bussiaikataulunäytön Arduino-koodi
├── bussiaikataulunaytto_kotelo_bottom.stl  # kotelon pohjan 3d-malli
├── bussiaikataulunaytto_kotelo_top.stl     # kotelon kannen 3d-malli
├── bussiaikataulunaytto_taustalevy.stl     # seinäkiinnikkeen 3d-malli
└── nayttotesti.ino                         # koodi näytön testaamiseen
```

## 3D-mallit

Kotelon 3D-malleja voit tarkastella (ja muokata ottamalla itsellesi kopion) [OnShapessa](https://cad.onshape.com/documents/f1fb8a455b82d920070b3ebc/w/c682447b5a39c870d298b90b/e/590ca089253f38d85d7e398c).


## Asennus ja käyttöönotto

1. Asenna Arduino IDE (versio 1.8 tai uudempi)
2. Avaa **Preferences (Asetukset)** ja lisää osoite *https://arduino.esp8266.com/stable/package_esp8266com_index.json* kenttään *Additional Board Manager URLs*
3. Avaa **Tools > Board** -valikosta *Boards Manager* ja asenna paketti *esp8266*
4. Ota Wemos D1 Mini kohdealustaksi valikosta **Tools > Board**
5. Asenna tarvittavat kirjastot (ArduinoJson, ezTime, Adafruit GFX ja GxEPD) *Library Managerilla*. Sen löydät valikosta **Sketch > Include Library > Manage Libraries...**
6. Muokkaa koodia (ainakin Wifi-tunnuksesi!) ja ohjelmoi se Wemokseen upload-nappia painamalla!

## Esimerkkinäkymä e-paperinäytöllä

![Aikataulu näyttää tältä](https://github.com/mvkorpel/bussiaikataulunaytto-MB1802/raw/master/epaperi.png)

## Latausvirran rajoitus sopivaksi

Rakennusohjeessa kehotetaan käyttämään noin 500 mAh:n akkua. Esimerkkituotteena mainitun Hubsan-akun (520 mAh) ohjeissa kerrotaan, ettei akkua saa ladata yli 1 C:n<sup id="a1">[1](#f1),</sup><sup id="a2"> [2](#f2)</sup> eli 520 mA:n virralla. Ohjeessa käytetty latauspiirikortti kuitenkin antaa vakiona 1000 mA virtaa. Laturia olisi siis syytä rajoittaa. Ei hätää: TP4056-latauspiiri on konfiguroitavissa yhtä piirikortin vastusta vaihtamalla<sup id="a3">[3](#f3)</sup>. Seuraavan kuvan etualalla, merkinnällä R3, näkyy alkuperäinen 1,2 kΩ:n vastus. Vastus on mitoiltaan<sup id="a4">[4](#f4)</sup> tuumakokona ehkä 0603, mutta 0805 (noin 2,0 x 1,25 mm) sopii sen paikalle myös hyvin.

![Latauspiirikortti](https://github.com/mvkorpel/bussiaikataulunaytto-MB1802/raw/master/hw107_hd.jpg)

Seuraava kuva kertoo konfigurointivastuksen (Rprog, yllä kuvassa R3) vaikutuksen latausvirtaan (Ibat). Piirin valmistajan (katso [TC4056A](#tc4056a)) ilmoittamiin säätöarvoihin (mustat pisteet) on sovitettu malli, jossa on kaksi komponenttia: vakio ja virran käänteinen riippuvuus vastuksesta. Mallin sopivuus näyttää hyvältä. Esimerkkitapauksena (punainen tähti) on noin 440 mA:n virta vastuksen arvolla 2,7 kΩ.

![Vastuksen vaikutus latausvirtaan](https://github.com/mvkorpel/bussiaikataulunaytto-MB1802/raw/master/tp4056.png)

## TC4056A

Latauspiirikortin kuvasta huomataan, että pääpiirin malli onkin TC4056A eikä TP4056, jollaisena sitä myydään kiinalaisessa nettikaupassa. TC4056A-piirin valmistaja saattaa googlailun perusteella olla AMS, Fuman Electronics, tai ihan joku muu. Piirin mallinumeroa hakukoneessa käyttämällä löytyy erinäisiä dokumentteja, jotka tukevat tulkintaa, että kyseessä olisi TP4056:n lailla toimiva klooni.

<strong id="f1">1.</strong> [↑](#a1) [<em>A Guide to Understanding Battery Specifications</em>](https://web.mit.edu/evt/summary_battery_specifications.pdf). MIT Electric Vehicle Team. Haettu 3.12.2019.  
<strong id="f2">2.</strong> [↑](#a2) Battery charger: C-rate. (23.09 UTC, 19.11.2019). <em>Wikipedia</em>. Haettu 3.12.2019 osoitteesta <https://en.wikipedia.org/wiki/Battery_charger#C-rate>.  
<strong id="f3">3.</strong> [↑](#a3) [<em>TP4056 1A Standalone Linear Li-lon (sic) Battery Charger with Thermal Regulation in SOP-8</em>](http://www.tp4056.com/d/tp4056.html). NanJing Top Power ASIC Corp. Haettu 4.12.2019.  
<strong id="f4">4.</strong> [↑](#a4) Surface-mount technology: Two-terminal packages. (02.30 UTC, 6.12.2019). <em>Wikipedia</em>. Haettu 6.12.2019 osoitteesta <https://en.wikipedia.org/wiki/Surface-mount_technology#Two-terminal_packages>.
