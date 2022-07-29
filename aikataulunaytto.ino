#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.h>     // Vanhempi Waveshare-näyttö
//#include <GxGDEH0213B72/GxGDEH0213B72.h> // LOLIN (WEMOS) -näyttö, pidempi piirikortti
//#include <GxGDEH0213B73/GxGDEH0213B73.h> // Waveshare V2, ongelmia D1 Minin kanssa
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeMono9pt7b.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ezTime.h>

// E-paperinäytön alustaminen
GxIO_Class io(SPI, SS, D3, D4);
GxEPD_Class display(io);

// Määritä nukkumisen kesto, oletuksena 900 s eli 15 min
#define SLEEP_SECONDS 900

// Kerroin sekunneista mikrosekunneiksi. Tässä voi myös ottaa huomioon
// mikrokontrollerin kellon systemaattisen virheen, esimerkiksi
// kertomalla tämän vakion luvulla 1.0194, jos tarvitaan keskimäärin
// 1.94 % pidempi nukkumisaika. Sopivan korjauskertoimen saa selville
// esimerkiksi oman NTP-palvelimen lokista tarkkailemalla
// toteutuneiden aikapyyntöjen ajankohtia.
#define SCALE_MICROSEC 1000000

// Suurin sallittu nukkumisaika hiljaisena
// aikataulujaksona. Laitteiston rajoihin perustuva, tasatuntiin
// pyöristetty. Arvo on sekunteja (3 h).
// Lähde: Marcel Stör - Max deep sleep for ESP8266. Viitattu 22.9.2019.
// https://thingpulse.com/max-deep-sleep-for-esp8266/
#define MAX_SLEEP 10800

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

// Bussilähtöjä ennen karsintaa enimmillään: 16 + 26 = 42
#define MAX_DEPART 42

// Näytölle mahtuvien aikataulurivien määrä
#define MAX_DISP 16

// HSL:n pysäkki-id:t voi hakea menemällä osoitteeseen
// https://www.hsl.fi/reitit-ja-aikataulut,
// kirjoittamalla pysäkkihakuun pysäkin nimen, ja
// kopioimalla osoitepalkista pysäkin tunnisteen,
// joka on muotoa HSL:<numerosarja>.

// Koko Suomen kattavia pysäkkitunnisteita voi hakea
// samasta rajapinnasta käyttämällä linkistä
// https://goo.gl/cwAC1H löytyvää kyselyä.

// GraphQL-pyyntö Digitransitin rajapintaan. Kokeile rajapintaa
// täällä: goo.gl/cwAC1H. Tällä haulla saa 16 seuraavaa lähtöä
// pysäkiltä E2069 sekä 26 seuraavaa lähtöä pysäkiltä E2053.
static const char digitransitQuery[] PROGMEM = "{\"query\":\"{"
  "E2069:stops(ids:[\\\"HSL:2215255\\\"]){stoptimesWithoutPatterns("
  "numberOfDepartures:16){"
  "realtimeDeparture,realtime,serviceDay,trip{route{shortName}}}},"
  "E2053:stops(ids:[\\\"HSL:2215222\\\"]){stoptimesWithoutPatterns("
  "numberOfDepartures:26){"
  "realtimeDeparture,realtime,serviceDay,trip{route{shortName}}}}}\"}";

// ArduinoJSON-puskurin koko. Ks. https://arduinojson.org/assistant/
// Puskurin on oltava suurempi kuin oletettu JSON-vastaus
// rajapinnasta. Alla olevalla laskutoimituksella saadaan 7926 tavua
// (7928 neljällä jaollisena). Tilaa on varattu reilusti.
// Linjanumeroiden vaatima tila on arvioitu yläkanttiin: viisi merkkiä
// jokaiselle. Merkkijonojen kopiointiin varattu tila on assistentin
// (ArduinoJson 6) ilmoittamista luvuista suurempi, esimerkkikoodista
// "Parsing program" otettu 3110; pienempi luku on 2797.
const size_t bufferSize = 2 * JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(16) +
    JSON_ARRAY_SIZE(26) + 87 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) +
    42 * JSON_OBJECT_SIZE(4) + 3110;

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

void printTimetableRow(const char *busName, const char *departure, bool isRealtime, int idx)
{
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

// Järjestää kaksi valmiiksi pienimmästä suurimpaan järjestettyä
// kellonaikataulukkoa yhdeksi kokonaan järjestetyksi taulukoksi.
// Syötteenä tulee olla taulukko 'x', jossa on 'n' alkiota.  Molemmat
// syötetaulukot ovat 'x':n osia: ensimmäinen on 'x':n alussa ja
// toinen alkaa kohdasta 'x[idxTwo]'. Tulos kirjoitetaan valmiiksi
// allokoituun taulukkoon 'order' siten, että x[order[k]] on (k+1):s
// aika aiemmasta myöhempään. 'nOrder' on luku <= 'n', joka ilmaisee,
// kuinka monta aikaa, varhaisimmasta alkaen, halutaan selvittää. Siis
// 'order' taulukkoon tulee 'nOrder' käyttökelpoista lukua.
void interleaveTwo(const time_t *x, int n, int idxTwo, int *order, int nOrder)
{
    int atOne = 0;
    int atTwo = idxTwo;
    int k;
    for (k = 0; k < nOrder; k++)
    {
        if (atOne >= idxTwo)
        {
            order[k] = atTwo++;
        }
        else if (atTwo >= n)
        {
            order[k] = atOne++; 
        }
        else if (x[atOne] <= x[atTwo])
        {
            order[k] = atOne++;
        }
        else
        {
            order[k] = atTwo++;
        }
    }
}

void wifiOff()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    yield();
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

    // Tarkistetaan kello. Kelloa käytetään tässä ohjelmassa
    // kolmella tavalla:
    // 1. aikavyöhykeasetusten riittävän tuoreuden toteamiseksi
    //    (välttämätöntä, kun aikavyöhyketietoa käytetään)
    // 2. rajapintahaun kellonajan näyttämiseksi (hyödyllistä: auttaa
    //    laitteen oikean toiminnan varmistamisessa, vaikkakin
    //    aikataulurivien määrä laskee 17:stä 16:een)
    // 3. laitteen pitkän nukkumisajan asettamiseksi hiljaisina
    //    aikoina (hyödyllistä: auttaa säästämään akkua)
    // Tiheäkään kyselyväli ei haittaa, kun käytetään omaa palvelinta,
    // mitä varten voit poistaa seuraavan rivin kommenttimerkit ja
    // asettaa NTP-palvelimelle oikean DNS-nimen.
    //setServer("oma_palvelin");
    // waitForSync hyväksyy argumenttina aikarajan (sekunteja)
    if (!waitForSync(30))
    {
        // Jos kellon tarkistus epäonnistuu, mennään syvään uneen
        Serial.println("NTP sync failed");
        wifiOff();
        ESP.deepSleep(60 * SCALE_MICROSEC);
    }

    // Aikavyöhykkeen ominaisuudet haetaan mikrokontrollerin pysyvästä
    // muistista. Jos tieto kuitenkin on vanhaa, aikavyöhykedata
    // ladataan uudestaan ezTime-kirjaston kehittäjän palvelimelta.
    Timezone Finland;
    if (!Finland.setCache(0))
    {
        Serial.println("getting TZ info");
        if (!Finland.setLocation("Europe/Helsinki"))
        {
            // Jos aikavyöhykkeen haku epäonnistuu, mennään syvään uneen
            Serial.println("TZ lookup failed");
            wifiOff();
            ESP.deepSleep(90 * SCALE_MICROSEC);
        }
    }
    Serial.print("time zone is ");
    Serial.println(Finland.getTimezoneName());

    // Seuraavilla riveillä luodaan ja lähetetään HTTP-pyyntö
    // Digitransitin rajapintaan

    WiFiClient client;
    HTTPClient http; // Alustetaan HTTP-Client -instanssi

    // Huomaa kaksi vaihtoehtoista osoitetta Digitransitin rajapintoihin,
    // koko Suomen haku, ja HSL:n haku.
    http.begin(client, "http://api.digitransit.fi/routing/v1/routers/hsl/index/graphql"); // <- HSL
    //http.begin(client, "http://api.digitransit.fi/routing/v1/routers/finland/index/graphql"); // <- koko Suomi

    http.addHeader("Content-Type", "application/json"); // Rajapinta vaatii pyynnön JSON-pakettina
    time_t queryTime = UTC.now();                       // Kyselyaika muistiin
    int httpCode = http.POST(digitransitQuery);         // POST-muotoinen pyyntö
    String payload = http.getString();                  // Otetaan Digitransitin lähettämä vastaus talteen
    http.end();

    // WiFi pois päältä
    wifiOff();

    // Parsitaan vastaus helpommin käsiteltävään muotoon
    DynamicJsonDocument jsonDoc(bufferSize);
    DeserializationError err = deserializeJson(jsonDoc, payload.c_str());
    JsonObject root = jsonDoc.as<JsonObject>();

    // Otetaan referenssit JSON-muotoisen vastauksen bussilähdöistä:
    JsonObject busData = root["data"];
    // 1: pysäkki E2069
    JsonArray departures1 = busData["E2069"][0]["stoptimesWithoutPatterns"];
    // 2: pysäkki E2053
    JsonArray departures2 = busData["E2053"][0]["stoptimesWithoutPatterns"];

    int departureTime;
    int departureOrder[MAX_DISP];
    time_t timeStamp[MAX_DEPART];
    bool realTime[MAX_DEPART];
    const char *busName[MAX_DEPART];
    int k, idx2, nRows;
    uint8_t localHours, localMinutes;
    char clockHhMm[6];
    clockHhMm[2] = ':';
    clockHhMm[5] = '\0';
    // Tallennetaan tiedot kaikista lähdöistä
    int idx = 0;
    for (JsonObject dep : departures1)
    {
        // Lähtöaika (sekunteja vuorokauden alusta)
        departureTime = dep["realtimeDeparture"];
        // Lasketaan lähtöaika Unix-aikaleimana
        timeStamp[idx] = (time_t) dep["serviceDay"] + departureTime;
        // Onko lähtö tarkka (käyttääkö HSL:n GPS-seurantaa?)
        realTime[idx] = dep["realtime"];
        // Bussin reittinumero
        busName[idx] = dep["trip"]["route"]["shortName"]; // Bussin reittinumero
        idx++;
    }
    // Tallennetaan tiedot mahdollisista linjan 111 lähdöistä
    // pysäkiltä E2053
    idx2 = idx;
    for (JsonObject dep : departures2)
    {
        const char *thisName = dep["trip"]["route"]["shortName"];
        if ((strlen(thisName) == 3 && strncmp(thisName, "111",  3) == 0))
        {
            departureTime = dep["realtimeDeparture"];
            timeStamp[idx] = ((time_t) dep["serviceDay"]) + departureTime;
            realTime[idx] = dep["realtime"];
            busName[idx] = thisName;
            idx++;
        }
    }
    // Lajitellaan lähdöt aikajärjestykseen
    nRows = min(MAX_DISP, idx);
    interleaveTwo(timeStamp, idx, idx2, departureOrder, nRows);

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
            idx2 = departureOrder[k];
            // Parsittu lähtöaika
            localHours = Finland.hour(timeStamp[idx2], UTC_TIME);
            localMinutes = Finland.minute(timeStamp[idx2], UTC_TIME);
            formatTime(clockHhMm, localHours, localMinutes);
            // Tulostetaan rivi näytölle oikeaan kohtaan
            printTimetableRow(busName[idx2], clockHhMm, realTime[idx2], k + 2);
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

    int sleepSec;
    if (nRows > 0)
    {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        int runSec = (int) (millis() / 1000);
        // Pyritään siihen, että näytön päivitysten väli on noin
        // SLEEP_SECONDS sekuntia.
        sleepSec = max(0, SLEEP_SECONDS - runSec);
        // Jos seuraavan bussin lähtöön on pitkä aika, viivytetään
        // seuraava ajo tapahtuvaksi keskimäärin noin 1.5 *
        // SLEEP_SECONDS sekuntia ennen kyseistä lähtöä, mutta
        // satunnaistetaan kerroin välille 1.3 -- 1.7
        time_t tNow = UTC.now();
        randomSeed((unsigned long) tNow +
                   mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5]);
        long wakeGap = random(1.3 * SLEEP_SECONDS, 1.7 * SLEEP_SECONDS + 1);
        time_t wakeUp = timeStamp[departureOrder[0]] - (time_t) wakeGap;
        if (wakeUp > tNow + sleepSec)
        {
            sleepSec = min(MAX_SLEEP, (int) (wakeUp - tNow));
            time_t trueWakeUp = tNow + (time_t) sleepSec;
            localHours = Finland.hour(trueWakeUp, UTC_TIME);
            localMinutes = Finland.minute(trueWakeUp, UTC_TIME);
            formatTime(clockHhMm, localHours, localMinutes);
            Serial.print("Sleeping until ");
            Serial.print(clockHhMm);
            Serial.print(", ");
            if (sleepSec == MAX_SLEEP)
            {
                Serial.print("max sleep ");
                Serial.print(MAX_SLEEP);
                Serial.println(" seconds");
            }
            else
            {
                Serial.print(wakeGap);
                Serial.println(" seconds before next departure");
            }
        }
    }
    else
    {
        // Jos ei busseja (virhe), kokeillaan pian uudestaan
        sleepSec = 90;
    }

    // Komennetaan ESP8266 syväunitilaan.
    // Herättyään koodi suoritetaan setup()-funktion alusta.
    ESP.deepSleep((uint64_t) sleepSec * SCALE_MICROSEC);
}

void loop()
{
    // loop() jätetään tyhjäksi, sillä deepsleepistä johtuen
    // koodin suoritus ei ikinä pääse tänne asti.
}
