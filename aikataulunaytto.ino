#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.h>     // Vanhempi Waveshare-näyttö
//#include <GxGDEH0213B72/GxGDEH0213B72.h> // LOLIN (WEMOS) -näyttö, pidempi piirikortti
//#include <GxGDEH0213B73/GxGDEH0213B73.h> // Waveshare V2, ongelmia D1 Minin kanssa
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeMono9pt7b.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ezTime.h>

// E-paperinäytön alustaminen
GxIO_Class io(SPI, SS, D3, D4);
GxEPD_Class display(io);

// Määritä nukkumisen kesto, oletuksena 900 s eli 15 min
#define SLEEP_SECONDS 900

// Langattoman verkon lähetysteho (dBm), 0-20.5.
#define WIFI_POWER 7

// HSL:n pysäkki-id:t voi hakea menemällä osoitteeseen
// https://www.hsl.fi/reitit-ja-aikataulut,
// kirjoittamalla pysäkkihakuun pysäkin nimen, ja
// kopioimalla osoitepalkista pysäkin tunnisteen,
// joka on muotoa HSL:<numerosarja>.

// Koko Suomen kattavia pysäkkitunnisteita voi hakea
// samasta rajapinnasta käyttämällä linkistä
// https://goo.gl/cwAC1H löytyvää kyselyä.

// GraphQL-pyyntö Digitransitin rajapintaan. Kokeile rajapintaa täällä: goo.gl/cwAC1H
static const char digitransitQuery[] PROGMEM = "{\"query\":\"{stops(ids:[\\\"HSL:2215255\\\"]){stoptimesWithoutPatterns(numberOfDepartures:17){realtimeDeparture,realtime,serviceDay,trip{route{shortName}}}}}\"}";

// ArduinoJSON-puskurin koko. Ks. https://arduinojson.org/assistant/
// Puskurin on oltava suurempi kuin oletettu JSON-vastaus
// rajapinnasta. Alla olevalla laskutoimituksella saadaan 3238 tavua
// (3240 neljällä jaollisena). Tilaa on varattu reilusti.
// Linjanumeroiden vaatima tila on arvioitu yläkanttiin: viisi merkkiä
// jokaiselle. Merkkijonojen kopiointiin varattu tila on assistentin
// (ArduinoJson 6) ilmoittamista luvuista suurempi, esimerkkikoodista
// "Parsing program" otettu 1270; pienempi luku on 1141.
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(17) + 37 * JSON_OBJECT_SIZE(1) + 17 * JSON_OBJECT_SIZE(4) + 1270;

// Kirjoittaa "hh:mm"-muotoisen, annettua aikaa esittävän merkkijonon
// taulukkoon 'x'. Merkeistä ':' ja lopun '\0' ovat jo valmiiksi
// paikoillaan. Yhden käyttötarkoituksen s(n)printf-korvike. Tällä
// ainakin helposti varmistutaan, että muutetaan vain juuri tunneille
// ja minuuteille varattuja merkkejä.
void formatTime(char *x, uint8_t h, uint8_t m)
{
    uint8_t digit;

    digit = h / 10;
    x[0] = '0' + digit;
    digit = h - digit * 10;
    x[1] = '0' + digit;

    digit = m / 10;
    x[3] = '0' + digit;
    digit = m - digit * 10;
    x[4] = '0' + digit;
}

void printTimetableRow(const char *busName, const char *departure, bool isRealtime, int idx) {
    /* Funktio tulostaa näytön puskuriin bussiaikataulurivin. Esim.
       110T  21:34~
    */
    display.setCursor(2, 2 + idx * 14);
    display.print(busName);
    display.setCursor(54, 2 + idx * 14);
    display.print(departure);
    if (isRealtime)
    {
        display.setCursor(108, 2 + idx * 14);
        display.print("~");
    }
}

void setup()
{
    Serial.begin(115200);

    // Ei tallenneta WiFi-asetuksia pysyvään muistiin. Vähentää
    // muistin kulumista.
    WiFi.persistent(false);

    // Tehon säätö säästää akkua
    WiFi.setOutputPower(WIFI_POWER);

    // Asetetaan station-tila (asiakas)
    WiFi.mode(WIFI_STA);

    // Voit myös asettaa itsellesi staattisen IP:n
    // säästääksesi akkua. Tämä lyhentää Wifi-verkkoon yhdistämistä
    // usealla sekunnilla.
    //IPAddress ip(192,168,1,50);
    //IPAddress gateway(192,168,1,1);
    //IPAddress subnet(255,255,255,0);
    //IPAddress dns2(8,8,8,8);
    // Neljäs ja viides argumentti ovat DNS-palvelimia.
    // Tässä nimipalvelin on reitittimessä. dns2 on Googlen nimipalvelu.
    //WiFi.config(ip, gateway, subnet, gateway, dns2);
    // Asetetaan laitteelle selkonimi. Ei pakollinen säätö.
    WiFi.hostname("bussitaulu");
    WiFi.begin("Wifi-verkkoni", "salasana");

    // Yhdistetään langattomaan verkkoon
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("connecting...");
        delay(250);
    }

    // Tarkistetaan kello. Kelloa käytetään aikavyöhykeasetusten
    // riittävän tuoreuden toteamiseksi (välttämätöntä, kun
    // aikavyöhyketietoa käytetään). Tiheäkään kyselyväli ei haittaa,
    // jos käytetään omaa palvelinta, mitä varten voit poistaa
    // seuraavan rivin kommenttimerkit ja asettaa NTP-palvelimelle
    // oikean nimen.
    //setServer("oma_palvelin");
    waitForSync();

    // Aikavyöhykkeen ominaisuudet haetaan mikrokontrollerin pysyvästä
    // muistista. Jos tieto kuitenkin on vanhaa, aikavyöhykedata
    // ladataan uudestaan ezTime-kirjaston kehittäjän palvelimelta.
    Timezone Finland;
    if (!Finland.setCache(0))
    {
        Serial.println("getting TZ info");
        Finland.setLocation("Europe/Helsinki");
    }

    /* Seuraavilla riveillä luodaan ja lähetetään HTTP-pyyntö Digitransitin rajapintaan */

    HTTPClient http; // Alustetaan HTTP-Client -instanssi

    // Huomaa kaksi vaihtoehtoista osoitetta Digitransitin rajapintoihin,
    // koko Suomen haku, ja HSL:n haku.
    http.begin("http://api.digitransit.fi/routing/v1/routers/hsl/index/graphql"); // <- HSL
    //http.begin("http://api.digitransit.fi/routing/v1/routers/finland/index/graphql"); // <- koko Suomi

    http.addHeader("Content-Type", "application/json"); // Rajapinta vaatii pyynnön JSON-pakettina
    int httpCode = http.POST(digitransitQuery);         // POST-muotoinen pyyntö
    String payload = http.getString();                  // Otetaan Digitransitin lähettämä vastaus talteen muuttujaan 'payload'
    http.end();

    // Parsitaan vastaus helpomminkäsiteltävään muotoon
    DynamicJsonDocument jsonDoc(bufferSize);
    deserializeJson(jsonDoc, payload.c_str());
    JsonObject root = jsonDoc.as<JsonObject>();

    // otetaan referenssi JSON-muotoisen vastauksen bussilähdöistä 'departures'
    JsonArray departures = root["data"]["stops"][0]["stoptimesWithoutPatterns"];

    // Hyödylliset rivit debuggaukseen:
    // if (!root.success()) {
    //      Serial.println("Parsing failed");
    // }

    // Alustetaan E-paperinäyttö
    display.init();
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMono9pt7b);

    char clockHhMm[6];
    clockHhMm[2] = ':';
    clockHhMm[5] = '\0';
    // Käydään kaikki bussilähdöt yksitellen läpi.
    // Jokainen bussilähtö piirretään e-paperinäytön puskuriin.
    int idx = 0;
    for (auto dep : departures)
    {
        // Lähtöaika (sekunteja vuorokauden alusta). Tämä ei käänny
        // suoraan kellonajaksi päivinä, jolloin kelloa siirretään.
        int departureTime = dep["realtimeDeparture"];
        // Lasketaan lähtöaika Unix-aikaleimana
        time_t timeStamp = (time_t) dep["serviceDay"] + departureTime;
        // Parsittu lähtöaika
        uint8_t localHours = Finland.hour(timeStamp, UTC_TIME);
        uint8_t localMinutes = Finland.minute(timeStamp, UTC_TIME);
        formatTime(clockHhMm, localHours, localMinutes);
        // Onko lähtö tarkka (käyttääkö HSL:n GPS-seurantaa?)
        bool realTime = dep["realtime"];
        // Bussin reittinumero
        const char *busName = dep["trip"]["route"]["shortName"]; // Bussin reittinumero
        printTimetableRow(busName, clockHhMm, realTime, ++idx); // tulostetaan rivi näytölle oikeaan kohtaan
    }

    display.update(); // Piirrä näyttöpuskurin sisältö E-paperinäytölle

    // Komennetaan ESP8266 syväunitilaan.
    // Herättyään koodi suoritetaan setup()-funktion alusta
    ESP.deepSleep(SLEEP_SECONDS * 1000000);
}

void loop() {
    // loop() jätetään tyhjäksi, sillä deepsleepistä johtuen
    // koodin suoritus ei ikinä pääse tänne asti.
}
