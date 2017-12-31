#include <dummy.h>

#include <SPI.h>
#include <ESP8266WiFi.h>
#include "osapi.h"
extern "C" {
#include "user_interface.h"
}
#include <MFRC522.h>
#include <limits.h>
/* credentials.h must define three constants:
 *  SERVER - the Raumfeld speaker to use
 *  WLAN - the name of the WiFi network
 *  PSK - the WIFi WPA2 key
 */
#include "credentials.h"
#include "raumfeld.h"

extern MFRC522 mfrc522;
MFRC522::MIFARE_Key rf_key;
uint8_t card_present;
void setup_rfid();

#define CARD_DEBOUNCE 10
#define BUFSIZE 1024
char buffer[BUFSIZE];
uint16_t current_track, current_volume, target_volume, last_set_volume;

/* Pin reading */
uint8_t pins[4];
#define VOLUME_UP 0
#define VOLUME_DOWN 2
#define PREV_TRACK 4
#define NEXT_TRACK 5

#define MAX_VOLUME 100 /* Maximum allowed volume level */

#define BLOCKSIZE 256
const uint8_t ID[] = {0x46, 0xc3, 0xbc, 0x72, 0x20, 0x48, 0x65, 0x6c, 0x65, 0x6e, 0x61, 0x20, 0x32, 0x30, 0x31, 0x37};
uint8_t rfid_block[BLOCKSIZE];

/* LED pattern
 * Setup - 0x5555
 * WiFi connected - 0x5050
 * Raumfeld setup - 0x5000
 * Card Removed - 0xF010
 * Card inserted - 0x1111
 * No server found - 0xFFF0
 * Server found - 0x0F0F
 * Loop w/o card interaction - 0xFFFF
 */
uint16_t led_pattern;


/* Cyclic actions:
    - Check availability of server every PING_INTERVAL seconds
    - Check current track number every TRACK_INTERVAL seconds
    - Check card presence every CARD_INTERVAL seconds
    - Increase/decrease volume every VOLUME_INTERVAL milliseconds (if button is pressed)
   Whenever a timer has finished, it sets the corresponding flag,
   which in turn is handled in the main loop.
*/
#define CARD_INTERVAL 2ul
#define PING_INTERVAL 60ul
#define TRACK_INTERVAL 20ul
#define VOLUME_INTERVAL 500ul
os_timer_t timer_ping, timer_card, timer_track, timer_volume, timer_led;

bool flag_track = false;
void cbTimerTrack(void *arg) {
  flag_track = true;
}

bool flag_ping = false;
void cbTimerPing(void *arg) {
  flag_ping = true;
}

bool flag_card = false;
void cbTimerCard(void *arg) {
  flag_card = true;
}

uint32_t volume_press_time = ULONG_MAX;
bool flag_volume = false;
void cbTimerVolume(void *arg) {
  flag_volume = true;
}
enum {
  volume_none,
  volume_up,
  volume_down,
  volume_up_finish,
  volume_down_finish
} volume_dir;

enum {
  pn_none,
  pn_prev,
  pn_next
} flag_prevnext;

// the setup function runs once when you press reset or power the board
void setup() {
  // init LED (pin 2). The bits in led_pattern are written cyclically to the LED.
  pinMode(2, OUTPUT);
  
  Serial.begin(38400);

  WiFi.begin(WLAN, PSK);

  while (WiFi.status() != WL_CONNECTED) {
    for (uint8_t n=0; n < 5; n++) {
      digitalWrite(2,HIGH);
      delay(50);
      digitalWrite(2,LOW);
      delay(50);
    }
    
    Serial.print(".");
  }

  setup_raumfeld();
  
  setup_rfid();

  card_present = false;
  for (uint8_t n = 0; n < 6; n++) {
    rf_key.keyByte[n] = 0xFF;
  }

  /* Enable Touch */
  memset(pins, 0, 4);
  pinMode(15, OUTPUT); /* Touch Enable pin */
  digitalWrite(15, LOW); /* Touch off */

  /* Setup timer structure */
  flag_prevnext = pn_none;
  volume_dir = volume_none;
  flag_track = flag_ping = flag_card = flag_volume = false;

  Serial.println("Arming timers");
  os_timer_setfn(&timer_card, cbTimerCard, NULL);
  os_timer_arm(&timer_card, 1000 * CARD_INTERVAL, true);
  os_timer_setfn(&timer_ping, cbTimerPing, NULL);
  os_timer_arm(&timer_ping, 1000 * PING_INTERVAL, true);
  os_timer_setfn(&timer_track, cbTimerTrack, NULL);
  os_timer_setfn(&timer_volume, cbTimerVolume, NULL);

}

void cbTimerLED(void*) {
  return;
  if (led_pattern & 0x01) {
    digitalWrite(2,HIGH);
    led_pattern >>= 1;
    led_pattern |= 0x8000;
  } else {
    digitalWrite(2,LOW);
    led_pattern >>= 1;
    led_pattern &= 0x7FFF;
  }
}

void enablePinsAndTimers() {
  digitalWrite(15, HIGH); /* Touch on */

  pinMode(VOLUME_UP, INPUT);
  pinMode(VOLUME_DOWN, INPUT);
  pinMode(PREV_TRACK, INPUT);
  pinMode(5, INPUT);
  attachInterrupt(digitalPinToInterrupt(VOLUME_UP), cbPin, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_DOWN), cbPin, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PREV_TRACK), cbPin, CHANGE);
  attachInterrupt(digitalPinToInterrupt(NEXT_TRACK), cbPin, CHANGE);

  os_timer_arm(&timer_track, TRACK_INTERVAL * 1000, true);
}

void disablePinsAndTimers() {
  detachInterrupt(digitalPinToInterrupt(VOLUME_UP));
  detachInterrupt(digitalPinToInterrupt(VOLUME_DOWN));
  detachInterrupt(digitalPinToInterrupt(PREV_TRACK));
  detachInterrupt(digitalPinToInterrupt(NEXT_TRACK));
  digitalWrite(15, LOW); /* Touch off */
  pinMode(2, OUTPUT); /* LED */

  os_timer_disarm(&timer_track);
}

/* Converts a keypress time interval (in ms) to a new target volume.
    < 500 ms: 1 step
    0.5...3s: 4 steps/s
    > 3s: 10 steps/s
*/
uint16_t updateVolume(uint32_t interval) {
  uint16_t volumedelta = 3;
  uint32_t n;

  if (interval <= 0) {
    return current_volume;
  }

  for (n = 500; n < 3000; n += 250) {
    if (interval > n) {
      volumedelta++;
    } else {
      break;
    }
  }
  for (n = 3000; n < 20000; n += 100) {
    if (interval > n) {
      volumedelta++;
    } else {
      break;
    }
  }
  Serial.println("updateVolume(): timedelta " + String(interval) + " = delta " + String(volumedelta) + ", current volume " + String(current_volume) + ", dir " + String(volume_dir));

  switch (volume_dir) {
    case volume_down:
    case volume_down_finish:
      if (volumedelta < current_volume) {
        Serial.println("updateVolume() (dn): returning " + String(current_volume) + "-" + String(volumedelta));
        return current_volume - volumedelta;
      } else {
        return 0;
      }
      break;
    case volume_up:
    case volume_up_finish:
      if (volumedelta + current_volume < MAX_VOLUME) {
        Serial.println("updateVolume() (up): returning " + String(volumedelta) + "+" + String(current_volume));
        return current_volume + volumedelta;
      } else {
        Serial.println("updateVolume(): returning MAXVOLUME");
        return MAX_VOLUME;
      }
      break;
    default:
      return current_volume;
  }
}

void cardRemoved() {
  card_present = false;
  Serial.println("Card removed");

  stop();
  disablePinsAndTimers();
  digitalWrite(2,LOW);
}

/****************************************************************************
   Unknown card
   Find out if the speaker is playing. If so, fetch the URI and write it to the card.
 ***************************************************************************/
void unknownCard() {
  Serial.println("Handling unknown card");
  digitalWrite(2,HIGH);
  if (!playing()) {
    Serial.println("Not playing, return");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }

  /* Find out URI... */
  String uri = geturi();

  if (uri.length() < 10) {
    Serial.println("No proper URI found");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }

  /* ...and write it to card */
  MFRC522::StatusCode status;
  int idx = 0;
  uint8_t block_buf[18];
  uint8_t block_size = sizeof(block_buf);
  uint8_t blockno = 5;
  bool last_block = false;
  for (blockno = 8; blockno < 64; blockno++) {
    if (blockno % 4 == 3) {
      continue;
    }

    memset(block_buf, 0, 17);
    if (uri.length() > idx + 16) {
      uri.substring(idx, idx + 16).toCharArray((char*)block_buf, 17);
      Serial.print("Block " + String(blockno) + ": " + uri.substring(idx, idx + 15) + "#");
    } else {
      uri.substring(idx).toCharArray((char*)block_buf, 17);
      Serial.println("Last block " + String(blockno) + ": " + uri.substring(idx));
      last_block = true;
    }
    idx += 16;

    if (blockno % 4 == 0) {
      status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockno + 3, &rf_key, &(mfrc522.uid));
      if (status != MFRC522::STATUS_OK) {
        Serial.println("Auth failed during write, block " + String(blockno) + " status " + String(status));
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        delay(100);
        mfrc522.PCD_Init();
        return;
      }
    }

    status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockno, block_buf, 16);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Write() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }
    if (last_block) {
      break;
    }
  }
  /* Write a block of zeroes for termination after last real block */
  blockno++;
  if (blockno % 4 == 0) {
    status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockno + 3, &rf_key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.println("Auth failed during write, block " + String(blockno) + " status " + String(status));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }
  }
  memset(block_buf, 0, 16);
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockno, block_buf, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }

  /* Write track #1 to card (first two bytes of block 5) */
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &rf_key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.println("Auth failed during write, block 7, status " + String(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }

  blockno = 5;
  memset(block_buf, 0, 16);
  block_buf[0] = 0;
  block_buf[1] = 1;
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockno, (uint8_t*)block_buf, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed at block 5: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }

  /* Looks like we were successful, so write ID to block 4.
    Same sector as before, no re-authentication necessary
  */
  blockno = 4;
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockno, (uint8_t*)ID, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed at block 4: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }
  for (uint8_t n=0; n < 10; n++) {
    digitalWrite(2,LOW);
    delay(200);
    digitalWrite(2,HIGH);
    delay(200);
  }
}

/****************************************************************************
 Card with matching ID
 Read the URI and send it to the speaker
***************************************************************************/
void knownCard() {
  String uri;
  uint8_t blockno;
  uint8_t block_buf[18];
  uint8_t block_size = sizeof(block_buf);
  MFRC522::StatusCode status;
  digitalWrite(2,HIGH);
  
  for (blockno = 8; blockno < 64; blockno++) {
    if (blockno % 4 == 3) {
      continue;
    }
    if (blockno % 4 == 0) {
      status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockno + 3, &rf_key, &(mfrc522.uid));
      if (status != MFRC522::STATUS_OK) {
        Serial.println("Auth failed during write, block " + String(blockno) + " status " + String(status));
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        delay(100);
        mfrc522.PCD_Init();
        return;
      }
    }
    block_size = sizeof(block_buf);
    memset(block_buf, 0, sizeof(block_buf));
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockno, block_buf, &block_size);
    if (status != MFRC522::STATUS_OK) {//|| (size < BLOCKSIZE)) {
      Serial.println("Read failed, status " + String(status) + " size " + String(block_size));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }

    block_buf[16] = 0;
    String block = String((char*)block_buf);
    uri = uri + block;
    if (block.length() < 16) {
      break;
    }
  }

  /* Read track number */
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &rf_key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.println("Auth failed during read, block 7, status " + String(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }
  blockno = 5;
  block_size = sizeof(block_buf);
  memset(block_buf, 0, sizeof(block_buf));
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockno, block_buf, &block_size);
  if (status != MFRC522::STATUS_OK) {//|| (size < BLOCKSIZE)) {
    Serial.println("Read failed, status " + String(status) + " size " + String(block_size));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }
  current_track = (int16_t)((((uint16_t)(block_buf[0])) << 8) + block_buf[1]);
  Serial.println("Track block data " + String(block_buf[0]) + " " + block_buf[1]);
  Serial.println("Track #" + String(current_track));
  if ((current_track <= 0) || (current_track >= 0x7FFE)) {
    current_track = 1;
  }

  /* Send URI and track to speaker */
  if (uri.length() < BUFSIZE) {
    current_volume = -1;
    int16_t read_volume = -1;
    uri.toCharArray(buffer, uri.length() + 1);
    Serial.println("seturi()");
    delay(100);
    seturi(buffer);
    Serial.println("play(), track is " + String(current_track));
    delay(100);
    settrack(current_track);
    delay(100);
    play();
    delay(100);
    read_volume = getVolume();
    if (read_volume >= 0) {
      current_volume = read_volume;
    }
  }
  enablePinsAndTimers();
}

// the loop function runs over and over again forever
void loop() {
  if (!server_found) {
    digitalWrite(2,LOW);
    clearServers();
    findServers();
    if (n_servers > 0) {
      led_pattern = 0x0F0F;
      server_found = true;
    }
    default_server = 0;
    Serial.println("Finding servers, result " + String (n_servers) + ", default " + String(default_server));
    return;
  }
  digitalWrite(2,HIGH);
  delay(100);
  digitalWrite(2,LOW);
  delay(100);
  if (!flag_track && !flag_ping && !flag_card && /*!flag_volume */ (volume_dir == volume_none) && (flag_prevnext == pn_none)) {
    return;
  }

  /* Check server for availability */
  if (flag_ping) {
    flag_ping = false;
    if (!ping(0)) {
      server_found = false;
    }
  }

  /* Send prev/next commands */
  if (flag_prevnext == pn_prev) {
    flag_prevnext = pn_none;
    prev();
  }
  if (flag_prevnext == pn_next) {
    flag_prevnext = pn_none;
    next();
  }

  if ((volume_dir != volume_none)) { // flag_volume) {
    flag_volume = false;
    if (!card_present) {
      volume_dir = volume_none;
      return;
    }
    Serial.println("flag_volume handler, dir is " + String(volume_dir) + ", CV now " + String(current_volume) + " TV " + String(target_volume));
    flag_volume = false;
    Serial.println("old target volume " + String(target_volume));
    target_volume = updateVolume(millis() - volume_press_time);
    Serial.println("Setting volume to new target volume " + String(target_volume));
    setVolume(target_volume);
    if ((volume_dir == volume_down_finish) || (volume_dir == volume_up_finish)) {
      current_volume = target_volume;
      volume_dir = volume_none;
      Serial.println("CV now " + String(current_volume) + " TV " + String(target_volume));
    }
  }

  /* Read current track cyclically and write it to the card (if it has changed) */
  if (flag_track) {
    flag_track = false;
    if (!card_present) {
      return;
    }

    int16_t read_track = -1;
    if (!playing()) {
      read_track = 1;
    }
    if (read_track == 0) {
      read_track = 1;
    }
    if ((read_track < 0) || (read_track == current_track)) {
      return;
    }
    current_track = read_track;
    Serial.println("Updating track on card to #" + String(current_track));
    /* Write track # to card (first two bytes of block 5) */
    if (!mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Cannot read card serial number");
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }
    
    MFRC522::StatusCode status;
    status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &rf_key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.println("Auth failed during write, block 7, status " + String(status));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }

    uint8_t block_buf[18];
    uint8_t block_size = sizeof(block_buf);
    uint8_t blockno = 5;
    memset(block_buf, 0, 16);
    block_buf[0] = (uint8_t)((((uint16_t)current_track) >> 8) & 0xFF);
    block_buf[1] = (uint8_t)(((uint16_t)current_track) & 0xFF);
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockno, (uint8_t*)block_buf, 16);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Write() failed at block 5: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(100);
    mfrc522.PCD_Init();
    return;
  }

  /****** Card handling *********/
  if (flag_card) {
    flag_card = false;

    MFRC522::StatusCode status;
    uint8_t block_buf[18];
    uint8_t block_size = sizeof(block_buf);
    uint8_t blockno = 4;

    String uri = String();

    bool card_detected;
    card_detected = mfrc522.PICC_IsNewCardPresent();
    if (card_present && !card_detected) {
      card_detected = mfrc522.PICC_IsNewCardPresent();
    }

    if (card_detected == card_present) {
      Serial.print(String(card_present));
      return;
    }
    Serial.println("detected " + String(card_detected) + "present " + String(card_present));
    if (!card_detected) {
      for (uint8_t n = 0; n < CARD_DEBOUNCE; n++) {
        delay(100);
        Serial.print("?");
        if (mfrc522.PICC_IsNewCardPresent()) {
          Serial.println("+");
          card_present = true;
          return;
        }
      }
      Serial.println("-");
      /* Card removed */
      cardRemoved();
      return;
    }
    Serial.println("Card inserted");

    if (!mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Cannot read card serial number");
      return;
    }

    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));
    // Check for compatibility
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
      Serial.println(F("Not a MIFARE Classic card."));
      return;
    }


    /* ID is in sector 1 (block 4).
        Base data (URI of first track) starts in sector 2 (block 8)
        and is written continuously in the first three blocks of each sector (i.e. 8,9,10,12,13,14,...)
        The last track number is written in the first two bytes of block 5.
    */
    status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &rf_key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.println("Auth failed " + String(status));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }

    block_size = sizeof(block_buf);
    blockno = 4;
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockno, block_buf, &block_size);
    if (status != MFRC522::STATUS_OK) {//|| (size < BLOCKSIZE)) {
      Serial.println("Read failed, status " + String(status) + " size " + String(block_size));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(100);
      mfrc522.PCD_Init();
      return;
    }

    if (!memcmp(ID, block_buf, 16)) {
      Serial.println("Known card");
      knownCard();
    } else {
      Serial.println("Unknown card");
      unknownCard();
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    mfrc522.PCD_Init();
    card_present = true;
  } /* End of card insert/remove handling */
}

void cbPin() {
  uint8_t pin_status[4];
  uint8_t sum = 0;
  pin_status[0] = digitalRead(VOLUME_UP);
  pin_status[1] = digitalRead(VOLUME_DOWN);
  pin_status[2] = digitalRead(PREV_TRACK);
  pin_status[3] = digitalRead(NEXT_TRACK);
  if (!memcmp(pin_status, pins, 4)) {
    return;
  }
  memcpy(pins, pin_status, 4);

  for (uint8_t n = 0; n < 4; n++) {
    sum += pin_status[n];
  }
  if (sum > 1) {
    /* More than one key pressed. This is not supported, treat them as all off */
    memset(pin_status,0,4);
  }

  if (pin_status[2]) {
    flag_prevnext = pn_prev;
    return;
  }
  if (pin_status[3]) {
    flag_prevnext = pn_next;
    return;
  }

  /* Volume key released */
  if (!pin_status[0] && !pin_status[1] && (volume_dir != volume_none)) {
    os_timer_disarm(&timer_volume);
    switch(volume_dir) {
    case volume_up:
      volume_dir = volume_up_finish;
      break;
    case volume_down:
      volume_dir = volume_down_finish;
      break;
    default:
      volume_dir = volume_none;
      break;
    }
    flag_volume = true;
    Serial.println("Volume key released, new target volume is " + String(target_volume));
    return;
  }
  if (pin_status[0]) {
    volume_dir = volume_down;
    os_timer_arm(&timer_volume, VOLUME_INTERVAL, true);
    volume_press_time = millis();
    last_set_volume = current_volume;
    Serial.println("Volume down key pressed, starting timer");
    return;
  }
  if (pin_status[1]) {
    volume_dir = volume_up;
    os_timer_arm(&timer_volume, VOLUME_INTERVAL, true);
    volume_press_time = millis();
    last_set_volume = current_volume;
    Serial.println("Volume up key pressed, starting timer");
    return;
  }


}


