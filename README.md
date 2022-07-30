# 2-18-aikataulunaytto

Oheisesta repositoriosta löytyy muokattu mallikoodi Mikrobitin numerossa 2/18 ilmestyneeseen tee-se-itse -aikataulunäyttöön. Alkuperäinen koodi on [täällä](https://github.com/Mikrobitti/bussiaikataulunaytto-MB1802). Automaattisesti päivittyvä bussiaikataulunäyttö pelastaa kiireisen työn sankarin aamun kertomalla tarkalleen, koska seuraava bussi kotipysäkin ohi pyyhältää.

Bussinäyttö koostuu e-paperinäytöstä, Wemos D1 Mini -mikrokontrollerista, litiumakusta latauspiireineen sekä 3D-tulostettavasta kotelosta. Wemos-mikrokontrollerin voi ohjelmoida helposti tuttuja Arduino-komentoja apuna käyttäen.

Tämä repositorion haara vain esittelee varsinaiset koodihaarat.

## Haarat (branches)

### Pääkehityslinja (master)
- Näytetään yhden pysäkin lähdöt
- Sisältää mm. kellonajan haun (NTP) ja siihen nojaavan vaihtelevan nukkumisajan

### pikkutunnit

- Näytetään yhden pysäkin lähdöt
- Kutakuinkin vähimmäismäärä muutoksia alkuperäiseen koodiin, jotta se toimii uudemmilla kirjastoversioilla ja näyttää kellonajat oikein

### runkolinja

- Näytetään yhden pysäkin lähdöt ja lisäksi yhden linjan lähdöt molempiin suuntiin erilliseltä pysäkkiparilta
- Pääkehityslinjan muutokset sisältyvät soveltuvin osin

### toinen_pysakki

- Näytetään yhden pysäkin lähdöt ja lisäksi yhden linjan lähdöt toiselta pysäkiltä
- Pääkehityslinjan muutokset sisältyvät soveltuvin osin
