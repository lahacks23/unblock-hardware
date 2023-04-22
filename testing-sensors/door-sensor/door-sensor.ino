#include <WiFi.h>
#include <sstream>
#include <ESP32Servo.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

#include <SPI.h>
#include <MFRC522.h>

const int REED_PIN = 23; // Pin connected to reed switch// LED pin
const int BUZZER_PIN = 18;

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Working!");
    pinMode(REED_PIN, INPUT_PULLUP); // Enable internal pull-up for the reed switch
                                     // pinMode(LED_PIN, OUTPUT);
}

void loop()
{
    // put your main code here, to run repeatedly:

    int proximity = digitalRead(REED_PIN); // Read the state of the switch

    // If the pin reads low, the switch is closed.
    if (proximity == LOW)
    {
        Serial.println("Switch closed");
        tone(BUZZER_PIN, 400);
        // digitalWrite(LED_PIN, HIGH);	// Turn the LED on
    }
    else
    {
        Serial.println("Switch opened");
        noTone(BUZZER_PIN);
        // digitalWrite(LED_PIN, LOW);		// Turn the LED off
    }
}
