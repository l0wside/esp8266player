#include <SPI.h>
#include <MFRC522.h>

#define RFID_CS 16
#define RFID_RST 11 /* Dummy value */



MFRC522 mfrc522(RFID_CS,UINT8_MAX);
MFRC522::MIFARE_Key key;

void setup_rfid() {
    SPI.begin();        // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522 card

    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    Serial.println(F("Scan a MIFARE Classic PICC to demonstrate read and write."));
    Serial.print(F("Using key (for A and B):"));
    Serial.println();

    Serial.println(F("BEWARE: Data will be written to the PICC, in sector #1"));

}

