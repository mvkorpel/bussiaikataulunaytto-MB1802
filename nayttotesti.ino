#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.h>     // Vanhempi Waveshare-näyttö
//#include <GxGDEH0213B72/GxGDEH0213B72.h> // LOLIN (WEMOS) -näyttö, pidempi piirikortti
//#include <GxGDEH0213B73/GxGDEH0213B73.h> // Waveshare V2, ongelmia D1 Minin kanssa
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

GxIO_Class io(SPI, SS, D3, D4); // Alusta SPI-väylä
GxEPD_Class display(io); // Adafruit GFX -yhteensopiva luokka

void setup() {
  display.init();  // Alusta näyttö
  display.setTextColor(GxEPD_BLACK); // Aseta musta teksti
  
  display.setCursor(30, 60); // Tekstin sijainti: X=30, Y=60
  display.print("Hello Mikrobitti!");
  display.update(); // Päivitä em. komennot näytölle
}

void loop() {}
