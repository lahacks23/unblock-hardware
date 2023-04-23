#include "ble_server_utils.h"
#include <Arduino.h>
#include <WiFi.h>

#include "secrets.h"
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>
#include <sstream>
#include <ESP32Servo.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

#include <SPI.h>
#include <MFRC522.h>

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
// #include <AsyncHTTPRequest_Generic.h>             // https://github.com/khoih-prog/AsyncHTTPRequest_Generic
#include <Fetch.h>

// #include <ArduinoJson.h>
// #include <CustomJWT.h>

// CustomJWT jwt("key", 256);

void handleResponse(Response response)
{
    Serial.println("Response received:");
    // Printing response.
    Serial.println(response);
    // Printing response headers.
    //     Serial.printf("Content-Type Header: \"%s\"\n", response.headers["Content-Type"].c_str());
    //     Serial.printf("Connection Header: \"%s\"\n", response.headers["Connection"].c_str());
}

FetchClient httpclient;

String packetjwt = "";

String originalRequests[10];
int numRequests = 0;

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
    void onRead(NimBLECharacteristic *pCharacteristic)
    {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(F(": onRead(), value: "));
        Serial.println(pCharacteristic->getValue().c_str());
    };

    void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        Serial.println(F("Message Written To Server!"));
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(F(": onWrite(), value: "));

        Serial.println(pCharacteristic->getValue().c_str());
        String characteristicMsg = pCharacteristic->getValue();
        int firstSpaceInd = characteristicMsg.indexOf(" ");
        if (firstSpaceInd == -1)
        {
            Serial.println(F("INVALID request!"));
            return;
        }
        String msgType = characteristicMsg.substring(0, firstSpaceInd);
        String restOfMsg = characteristicMsg.substring(firstSpaceInd + 1);
        if (msgType == "UNLOCK" || msgType == "LOCK")
        {
            // UNLOCK uid requestID
            // send back
            // UNLOCK_ACK uid requestID lid nonce
            // nonce =random number
            unsigned long nonce = random(1, 1000000000);
            String res = msgType + "_ACK " + restOfMsg;
            res += " " + String(LOCK_ID);
            res += " " + String(nonce);
            // writing to client
            Serial.print("Writing ");
            Serial.print(res);
            Serial.println(" to client");

            pCharacteristic->setValue(res);
            return;
        }
        else if (msgType == "S")
        {
            // S NEXT 50 chars of JWT
            firstSpaceInd = characteristicMsg.indexOf(" ");
            if (firstSpaceInd == -1)
            {
                Serial.println(F("INVALID request!"));
                return;
            }
            String str_jwt = characteristicMsg.substring(0, firstSpaceInd + 1);
            packetjwt += str_jwt;
            return;
            // Serial.print("str_jwt is ");
            // Serial.println(str_jwt);

            // jwt.allocateJWTMemory();
            // Serial.println(jwt.decodeJWT((char *)str_jwt.c_str()));
            // Serial.printf("Header: %s\nHeader Length: %d\n", jwt.header, jwt.headerLength);
            // //int payload_len = sizeof(jwt_payload);
            // DynamicJsonDocument doc(1024);
            // deserializeJson(doc, jwt.payload);
            // int nonce = doc["nonce"];

            // //TODO: NONCING

            // Serial.printf("Payload: %s\nPayload Length: %d\n", jwt.payload, jwt.payloadLength);
            // Serial.printf("Signature: %s\nSignature Length: %d\n", jwt.signature, jwt.signatureLength);
            // jwt.clear();
        }
        else if (msgType == "F")
        {
            firstSpaceInd = characteristicMsg.indexOf(" ");
            if (firstSpaceInd == -1)
            {
                Serial.println(F("INVALID request!"));
                return;
            }
            String str_jwt = characteristicMsg.substring(0, firstSpaceInd + 1);
            packetjwt += str_jwt;

            RequestOptions options;
            options.method = "POST";
            options.headers["Content-Type"] = "application/json";
            options.body = packetjwt;

            // httpclient = fetch("http://ec2-54-177-39-50.us-west-1.compute.amazonaws.com:3000/verify", options, handleResponse);
            Serial.print("packetjwt is ");
            Serial.println(packetjwt);
            packetjwt = "";
        }

        else if (msgType == "SETUP")
        {
            return;
        }

        else
        {
            Serial.println(F("ERROR PARSING REQUEST TYPE!!"));
        }
        // Change characteristic!
    };
    /** Called before notification or indication is sent,
     *  the value can be changed here before sending if desired.
     */
    void onNotify(NimBLECharacteristic *pCharacteristic)
    {
        Serial.println(F("Sending notification to clients"));
    };

    /** The status returned in status is defined in NimBLECharacteristic.h.
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic *pCharacteristic, Status status, int code)
    {
        String str = ("Notification/Indication status code: ");
        str += status;
        str += ", return code: ";
        str += code;
        str += ", ";
        str += NimBLEUtils::returnCodeToString(code);
        Serial.println(str);
    };

    void onSubscribe(NimBLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc, uint16_t subValue)
    {
        String str = "Client ID: ";
        str += desc->conn_handle;
        str += " Address: ";
        str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
        if (subValue == 0)
        {
            str += " Unsubscribed to ";
        }
        else if (subValue == 1)
        {
            str += " Subscribed to notfications for ";
        }
        else if (subValue == 2)
        {
            str += " Subscribed to indications for ";
        }
        else if (subValue == 3)
        {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID()).c_str();

        Serial.println(str);
    };
};

// our bluetooth handlers
static CharacteristicCallbacks chrCallbacks;

Servo myservo;
int servo_pos = 0;

const int REED_PIN = 22; // Pin connected to reed switch// LED pin
const int BUZZER_PIN = 18;
const int SERVO_PIN = 21;
const int LOCKED_LED_PIN = 32;
const int UNLOCKED_LED_PIN = 23;

static NimBLEServer *pServer;
volatile bool dataChanged = false;

String locksPath = "/locks";
String lockStatePath = "/lock1/locked";
String lockState = "LOCK";

FirebaseData fbdo, locks;

FirebaseAuth auth;
FirebaseConfig config;

bool taskCompleted = false;
bool doorClosed = false;

unsigned long dataMillis = 0;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

// Firebase stream setups

void unblockStreamCallback(FirebaseStream stream)
{
    Serial.println(F("Handling stream"));
    // Serial.println(numLockrs);
    // if (stream.get(lockStatePath)){
    if (dataChanged)
    {
        Serial.println(F("Data already changed"));
        return;
    }
    String currLockState = stream.to<String>();
    // Serial.println("Stream Lock State:");
    // Serial.println(currLockState);
    // Serial.println("Past Lock State: ");
    String pastLockState = lockState;
    // Serial.println(pastLockState);

    if (currLockState != pastLockState)
    {
        Serial.println("Changing door state!");
        // handle changing
        // true -> false
        if (currLockState == "LOCK")
        {
            lockDoor();
        }
        // false -> true
        else if (currLockState == "UNLOCK")
        {
            unlockDoor();
        }
        else
        {
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

void lockDoor()
{
    Serial.println("Door in locked state!");
    myservo.write(180);
}

void unlockDoor()
{
    Serial.println("Door in unlocked state!");
    myservo.write(0);
}

void detectBrokenInto()
{
    // if locked
    if (lockState == "LOCK" && !doorClosed)
    {
        // tone(BUZZER_PIN,400);
    }
    else
        noTone(BUZZER_PIN);
}

void displayLockState()
{
    if (lockState == "UNLOCK")
    {
        digitalWrite(LOCKED_LED_PIN, LOW);
        digitalWrite(UNLOCKED_LED_PIN, HIGH);
    }
    else
    {
        digitalWrite(LOCKED_LED_PIN, HIGH);
        digitalWrite(UNLOCKED_LED_PIN, LOW);
    }
}

// void sendSignedRequest(String signedClientRes)
// {
//   static bool requestOpenResult;

//   if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone)
//   {
//     String bearerHeader = "Bearer " + String(LOCK_SECRET);
//     request.setReqHeader("Authorization", bearerHeader.c_str());
//     request.setReqHeader("Content-Type","application/json");
//     int firstSpaceInd = signedClientRes.indexOf(" ");
//     if (firstSpaceInd ==-1){
//       Serial.println("INVALID request!");
//       return;
//     }
//     //request.
//     // DynamicJsonDocument doc(1024);
//     // doc["uid"] =
//     // doc["rid"]: <request ID>,
//     // doc["lid"]: <lock id>,
//     // doc["type"]: "UNLOCK" | "LOCK",
//     // doc["nonce": <nonce>

//     //requestOpenResult = request.open("GET", "http://worldtimeapi.org/api/timezone/Europe/London.txt");
//     //request.setHeader
//     Serial.println(F("Sending request!"));
//     // requestOpenResult = request.open("POST", "http://ec2-54-177-39-50.us-west-1.compute.amazonaws.com:3000/verify");
//     // request.send();

//     if (requestOpenResult)
//     {
//       // Only send() if open() returns true, or crash
//       request.send();
//     }
//     else
//     {
//       Serial.println(F("Can't send bad request"));
//     }
//   }
//   else
//   {
//     Serial.println(F("Can't send request"));
//   }
// }

// void signedRequestCB(void *optParm, AsyncHTTPRequest *request, int readyState)
// {
//   (void) optParm;

//   if (readyState == readyStateDone)
//   {
//     AHTTP_LOGDEBUG(F("\n**************************************"));
//     AHTTP_LOGDEBUG1(F("Response Code = "), request->responseHTTPString());

//     if (request->responseHTTPcode() == 200)
//     {
//       Serial.println(F("\n**************************************"));
//       Serial.println(request->responseText());
//       Serial.println(F("**************************************"));
//     }
//   }
// }

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Starting NimBLE Server"));

    /** sets device name */
    NimBLEDevice::init("unblock-lock");

    /** Optional: set the transmit power, default is 3db */
#ifdef ESP_PLATFORM
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
    NimBLEDevice::setPower(9); /** +9db */
#endif

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

// WIFI SETUP
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

    // handle streaming
    if (!Firebase.RTDB.beginStream(&locks, locksPath))
        Serial.printf("stream begin error, %s\n\n", locks.errorReason().c_str());

    // set stream callback
    Firebase.RTDB.setStreamCallback(&locks, unblockStreamCallback, unblockStreamTimeoutCallback);

    // set initial states
    bool initialLockState = Firebase.RTDB.getBool(&fbdo, locksPath + lockStatePath);
    lockState = initialLockState;
    Serial.println("Lock initially set to");
    Serial.println(lockState);
    // if locked
    if (lockState == "LOCK")
    {
        lockDoor();
    }
    else if (lockState == "UNLOCK")
    {
        unlockDoor();
    }

    // hardware setup
    pinMode(REED_PIN, INPUT_PULLUP); // Enable internal pull-up for the reed switch
                                     // pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // servo shit
    //  Allow allocation of all timers
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    myservo.setPeriodHertz(50);            // standard 50 hz servo
    myservo.attach(SERVO_PIN, 1000, 2000); // attaches the servo on pin 18 to the servo object
    pinMode(LOCKED_LED_PIN, OUTPUT);
    pinMode(UNLOCKED_LED_PIN, OUTPUT);
    // using default min/max of 1000us and 2000us
}

unsigned long ble_timestamp = 0;
void loop()
{
    // BLE UPDATES
    uint64_t now = millis();
    // send BLE updates every 5 seconds
    if (pServer->getConnectedCount() && (now - ble_timestamp > 5000))
    {
        ble_timestamp = now;
        // Serial.println("Sending info to BLE clients");
        NimBLEService *pSvc = pServer->getServiceByUUID("BAAD");
        if (pSvc)
        {
            NimBLECharacteristic *pChr = pSvc->getCharacteristic("F00D");
            {
                // pChr->notify(true);
            }
        }
    }

    // firebase shit
    if (Firebase.ready())
    {
    }

    if (dataChanged)
    {
        dataChanged = false;
    }

    // delay(2000);

    int proximity = digitalRead(REED_PIN); // Read the state of the switch

    // If the pin reads low, the switch is closed.
    if (proximity == LOW)
    {
        doorClosed = true;
    }
    else
    {
        doorClosed = false;
    }
    detectBrokenInto();
    displayLockState();
}
