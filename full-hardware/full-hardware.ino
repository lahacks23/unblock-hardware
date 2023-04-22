#include "ble_server_utils.h"
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


static NimBLEServer *pServer;
volatile bool dataChanged= false;

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

//Firebase stream setups

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
// Define Firebase Data object


void lockDoor(){
  Serial.println("Door in locked state!");
}

void unlockDoor(){
  Serial.println("Door in unlocked state!");
}





void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Server");

    /** sets device name */
    NimBLEDevice::init("unblock-lock");

    /** Optional: set the transmit power, default is 3db */
#ifdef ESP_PLATFORM
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
    NimBLEDevice::setPower(9); /** +9db */
#endif



    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    // NimBLEDevice::setSecurityAuth(false, false, true);
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService *pDeadService = pServer->createService("DEAD");
    NimBLECharacteristic *pBeefCharacteristic = pDeadService->createCharacteristic(
        "BEEF",
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            /** Require a secure connection for read and write access */
            NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
            NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
    );

    pBeefCharacteristic->setValue("Burger");
    pBeefCharacteristic->setCallbacks(&chrCallbacks);

    /** 2904 descriptors are a special case, when createDescriptor is called with
     *  0x2904 a NimBLE2904 class is created with the correct properties and sizes.
     *  However we must cast the returned reference to the correct type as the method
     *  only returns a pointer to the base NimBLEDescriptor class.
     */
    NimBLE2904 *pBeef2904 = (NimBLE2904 *)pBeefCharacteristic->createDescriptor("2904");
    pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
    pBeef2904->setCallbacks(&dscCallbacks);

    NimBLEService *pBaadService = pServer->createService("BAAD");
    NimBLECharacteristic *pFoodCharacteristic = pBaadService->createCharacteristic(
        "F00D",
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::NOTIFY);

    pFoodCharacteristic->setValue("Fries");
    pFoodCharacteristic->setCallbacks(&chrCallbacks);

    /** Note a 0x2902 descriptor MUST NOT be created as NimBLE will create one automatically
     *  if notification or indication properties are assigned to a characteristic.
     */

    /** Custom descriptor: Arguments are UUID, Properties, max length in bytes of the value */
    NimBLEDescriptor *pC01Ddsc = pFoodCharacteristic->createDescriptor(
        "C01D",
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
        20);
    pC01Ddsc->setValue("Send it back!");
    pC01Ddsc->setCallbacks(&dscCallbacks);

    /** Start the services when finished creating all Characteristics and Descriptors */
    pDeadService->start();
    pBaadService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    /** Add the services to the advertisment data **/
    pAdvertising->addServiceUUID(pDeadService->getUUID());
    pAdvertising->addServiceUUID(pBaadService->getUUID());
    /** If your device is battery powered you may consider setting scan response
     *  to false as it will extend battery life at the expense of less data sent.
     */
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("Advertising Started");




//WIFI SETUP
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

unsigned long ble_timestamp = 0;
void loop() {
    // BLE UPDATES
    uint64_t now = millis();
    //send BLE updates every 5 seconds
    if (pServer->getConnectedCount() && (now - ble_timestamp > 5000) )
    {
      ble_timestamp = now;
      Serial.println("Sending info to BLE clients");
        NimBLEService *pSvc = pServer->getServiceByUUID("BAAD");
        if (pSvc)
        {
            NimBLECharacteristic *pChr = pSvc->getCharacteristic("F00D");
            if (pChr)
            {
                pChr->notify(true);
            }
        }
    }

    //firebase shit
    if (Firebase.ready())
    {
    }

    if (dataChanged){
      dataChanged= false;
    }

    //delay(2000);
}
