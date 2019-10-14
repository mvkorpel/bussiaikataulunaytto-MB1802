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

// Rivin korkeus (päällekkäisten rivien väli) E-paperinäytöllä
// (pikseleitä)
#define LINE_HEIGHT 14

// Näytön yläreunan marginaali
#define TOP_MARGIN 6

// Näytön yläreunan marginaali kellonajalle. Jos pienempi kuin
// TOP_MARGIN, kellon ja muiden rivien väliin jää tavallisia
// rivivälejä suurempi rako.
#define CLOCK_MARGIN 0

// HSL:n pysäkki-id:t voi hakea menemällä osoitteeseen
// https://www.hsl.fi/reitit-ja-aikataulut,
// kirjoittamalla pysäkkihakuun pysäkin nimen, ja
// kopioimalla osoitepalkista pysäkin tunnisteen,
// joka on muotoa HSL:<numerosarja>.

// Koko Suomen kattavia pysäkkitunnisteita voi hakea
// samasta rajapinnasta käyttämällä linkistä
// https://goo.gl/cwAC1H löytyvää kyselyä.

// GraphQL-pyyntö Digitransitin rajapintaan. Kokeile rajapintaa täällä: goo.gl/cwAC1H
static const char digitransitQuery[] PROGMEM = "{\"query\":\"{stops(ids:[\\\"HSL:2215255\\\"]){stoptimesWithoutPatterns(numberOfDepartures:16){realtimeDeparture,realtime,serviceDay,trip{route{shortName}}}}}\"}";

// ArduinoJSON-puskurin koko. Ks. https://arduinojson.org/assistant/
// Puskurin on oltava suurempi kuin oletettu JSON-vastaus
// rajapinnasta. Alla olevalla laskutoimituksella saadaan 3056 tavua.
// Tilaa on varattu reilusti.
// Linjanumeroiden vaatima tila on arvioitu yläkanttiin: viisi merkkiä
// jokaiselle. Merkkijonojen kopiointiin varattu tila on assistentin
// (ArduinoJson 6) ilmoittamista luvuista suurempi, esimerkkikoodista
// "Parsing program" otettu 1200; pienempi luku on 1076.
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(16) + 35 * JSON_OBJECT_SIZE(1) + 16 * JSON_OBJECT_SIZE(4) + 1200;

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

// Tulostaa kellonajan ('text') keskelle yläriviä.  Tekstin ympärille
// tulee pieni "kehys".
void printClockRow(const char *text)
{
    display.setCursor(11, CLOCK_MARGIN + LINE_HEIGHT);
    display.print("= ");
    display.print(text);
    display.print(" =");
}

void printTimetableRow(const char *busName, const char *departure, bool isRealtime, int idx) {
    // Funktio tulostaa näytön puskuriin bussiaikataulurivin. Esim.
    // 110T ~21:34
    // Tilde näytetään, jos aika on otettu aikataulusta eli ei ole
    // reaaliaikainen arvio.
    display.setCursor(0, TOP_MARGIN + idx * LINE_HEIGHT);
    display.print(busName);
    if (!isRealtime)
    {
        display.setCursor(55, TOP_MARGIN + idx * LINE_HEIGHT);
        display.print("~");
    }
    display.setCursor(66, TOP_MARGIN + idx * LINE_HEIGHT);
    display.print(departure);
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
    time_t queryTime = UTC.now();                       // Kyselyaika muistiin
    int httpCode = http.POST(digitransitQuery);         // POST-muotoinen pyyntö
    String payload = http.getString();                  // Otetaan Digitransitin lähettämä vastaus talteen muuttujaan 'payload'
    http.end();

    // WiFi pois päältä
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    yield();

    // Parsitaan vastaus helpomminkäsiteltävään muotoon
    DynamicJsonDocument jsonDoc(bufferSize);
    DeserializationError err = deserializeJson(jsonDoc, payload.c_str());
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

    int departureTime;
    time_t timeStamp[16];
    bool realTime[16];
    const char *busName[16];
    int k, nRows;
    uint8_t localHours, localMinutes;
    char clockHhMm[6];
    clockHhMm[2] = ':';
    clockHhMm[5] = '\0';
    // Tallennetaan tiedot kaikista lähdöistä
    int idx = 0;
    for (auto dep : departures)
    {
        // Lähtöaika (sekunteja vuorokauden alusta). Tämä ei käänny
        // suoraan kellonajaksi päivinä, jolloin kelloa siirretään.
        departureTime = dep["realtimeDeparture"];
        // Lasketaan lähtöaika Unix-aikaleimana
        timeStamp[idx] = (time_t) dep["serviceDay"] + departureTime;
        // Onko lähtö tarkka (käyttääkö HSL:n GPS-seurantaa?)
        realTime[idx] = dep["realtime"];
        // Bussin reittinumero
        busName[idx] = dep["trip"]["route"]["shortName"]; // Bussin reittinumero
        idx++;
    }
    nRows = idx;

    localHours = Finland.hour(queryTime, UTC_TIME);
    localMinutes = Finland.minute(queryTime, UTC_TIME);
    formatTime(clockHhMm, localHours, localMinutes);
    Serial.println(clockHhMm);
    if (nRows > 0)
    {
        // Alustetaan E-paperinäyttö
        display.init();
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeMono9pt7b);
        // Kirjoitetaan aikataulun päivitysaika keskelle
        // yläriviä. Esimerkki: = 12:34 =
        printClockRow(clockHhMm);
        // Käydään kaikki bussilähdöt yksitellen läpi. Jokainen bussilähtö
        // piirretään e-paperinäytön puskuriin.  Aloitetaan kakkosriviltä.
        for (k = 0; k < nRows; k++)
        {
            // Parsittu lähtöaika
            localHours = Finland.hour(timeStamp[k], UTC_TIME);
            localMinutes = Finland.minute(timeStamp[k], UTC_TIME);
            formatTime(clockHhMm, localHours, localMinutes);
            // Tulostetaan rivi näytölle oikeaan kohtaan
            printTimetableRow(busName[k], clockHhMm, realTime[k], k + 2);
        }
        display.update(); // Piirrä näyttöpuskurin sisältö E-paperinäytölle
    }
    else
    {
        // Jos virhe, jätetään mahdollinen aiempi aikataulu näytölle,
        // mutta lähetetään tietoa virheestä sarjaporttiin
        Serial.print("http code ");
        Serial.println(httpCode);
        if (err)
        {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(err.c_str());
        }
    }

    // Komennetaan ESP8266 syväunitilaan.
    // Herättyään koodi suoritetaan setup()-funktion alusta
    ESP.deepSleep(SLEEP_SECONDS * 1000000);
}

void loop() {
    // loop() jätetään tyhjäksi, sillä deepsleepistä johtuen
    // koodin suoritus ei ikinä pääse tänne asti.
}
