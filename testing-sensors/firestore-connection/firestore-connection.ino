
/**
 * Created by K. Suwatchai (Mobizt)
 *
 * Email: k_suwatchai@hotmail.com
 *
 * Github: https://github.com/mobizt/Firebase-ESP-Client
 *
 * Copyright (c) 2023 mobizt
 *
 */

// This example shows how to get a document from a document collection. This operation required Email/password, custom or OAUth2.0 authentication.

#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include "secrets.h"
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>


/* 1. Define the WiFi credentials */


volatile bool dataChanged= false;

// Define Firebase Data object


void lockDoor(){
  Serial.println("Door in locked state!");
}

void unlockDoor(){
  Serial.println("Door in unlocked state!");
}


String locksPath = "/locks";
String lockStatePath = "/lock1/locked";
bool lockState = false;

FirebaseData fbdo, locks;

FirebaseAuth auth;
FirebaseConfig config;

bool taskCompleted = false;

unsigned long dataMillis = 0;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif




void unblockStreamCallback(FirebaseStream stream){
  Serial.println("Handling stream");
  //Serial.println(numLockrs);
  //if (stream.get(lockStatePath)){
    if(dataChanged){
        Serial.println("Data already changed");
        return;
    }
    bool currLockState = stream.to<bool>();
      Serial.println("Stream Lock State:");
      Serial.println(currLockState);
      Serial.println("Past Lock State: ");
      bool pastLockState = lockState;
      Serial.println(pastLockState);

      if(currLockState != pastLockState){
        Serial.println("Changing door state!");
        //handle changing
        //true -> false
        if(pastLockState == true){
          unlockDoor();
        }
        //false -> true
        else if(pastLockState == false) {
          lockDoor();
        }

        else {
          Serial.println("Huh");
        }

        lockState = currLockState;
        dataChanged = true;
  }
}

void unblockStreamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!locks.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", locks.httpCode(), locks.errorReason().c_str());
}


void setup()
{

    Serial.begin(115200);

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    multi.addAP(WIFI_SSID, WIFI_PASSWORD);
    multi.run();
#else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif

    Serial.print("Connecting to Wi-Fi");
    unsigned long ms = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
        if (millis() - ms > 10000)
            break;
#endif
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    /* Assign the api key (required) */
    config.api_key = API_KEY;


  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

    /* Assign the user sign in credentials */
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    // The WiFi credentials are required for Pico W
    // due to it does not have reconnect feature.
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    config.wifi.clearAP();
    config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
#endif

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

#if defined(ESP8266)
    // In ESP8266 required for BearSSL rx/tx buffer for large data handle, increase Rx size as needed.
    fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 2048 /* Tx buffer size in bytes from 512 - 16384 */);
#endif

    // Limit the size of response payload to be collected in FirebaseData
    fbdo.setResponseSize(2048);

    Firebase.begin(&config, &auth);


    Firebase.reconnectWiFi(true);

    //handle streaming
    if(!Firebase.RTDB.beginStream(&locks, locksPath))
      Serial.printf("stream begin error, %s\n\n", locks.errorReason().c_str());

    //set stream callback
    Firebase.RTDB.setStreamCallback(&locks, unblockStreamCallback, unblockStreamTimeoutCallback);

    //set initial states
    bool initialLockState = Firebase.RTDB.getBool(&fbdo, locksPath + lockStatePath);
    lockState = initialLockState;
    Serial.println("Lock initially set to");
    Serial.println(lockState);
    //if locked
    if(lockState){
      lockDoor();
    }
    else unlockDoor();
    


}

void loop()
{

    // Firebase.ready() should be called repeatedly to handle authentication tasks.

    if (Firebase.ready())
    {

    }

      if (dataChanged){
    dataChanged= false;
  }
}
