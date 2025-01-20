/*
 * Project: CO2 Monitoring with Heltec Wireless Stick V3 and SCD30
 * Author: Fabio Bridarolli
 * License: MIT License
 * 
 * MIT License
 * 
 * Copyright (c) 2025 Fabio Bridarolli
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Wire.h>
#include <WiFi.h>
#include <HT_SSD1306Wire.h>  // Library for the OLED display
#include <SparkFun_SCD30_Arduino_Library.h>  // Includes the SparkFun library for SCD30
#include "wifi_credentials.h"

// Creating the display object with static configuration
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);

// Definition of I2C pins for the SCD30 sensor
#define SDA_SCD30_PIN 45
#define SCL_SCD30_PIN 46

// ADC pin for reading battery voltage
#define ADC_BATTERY_PIN 1 // Uses ADC1_CH0 pin to read battery voltage
#define GPIO_BATTERY_CTRL 37 // GPIO37 for controlling battery reading

// Definition of the touch pin
#define TOUCH_PIN 7
int touchThreshold; // Touch detection threshold

// Reference values for the battery (in volts)
const float batteryMaxVoltage = 4.1;
const float batteryMinVoltage = 3.2;
const float lowBatteryThreshold = 3.7;  // 3.7V threshold for deep sleep

// Variable to store the last Wi-Fi connection attempt
unsigned long lastWifiAttemptTime = 0;

// Variable to store the start of the low battery period
unsigned long lowBatteryStartTime = 0;  

// Flag to track if we are in "low battery" mode
bool lowBatteryWarning = false;  

// Global variable for Wi-Fi connection status
bool wifiConnected = false;  

// Initializing the SCD30 sensor
SCD30 airSensor;
TwoWire I2C_SCD30 = TwoWire(1);

// Variables for battery reading averaging
const int numReadings = 5;
float batteryReadings[numReadings];
int readIndex = 0;
float total = 0;
float averageBatteryVoltage = 0;

// Function to attempt Wi-Fi connection
bool attemptWiFiConnection() {
    Serial.println("Attempting Wi-Fi connection...");
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    const unsigned long wifiTimeout = 10000;  // 10-second timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
        delay(1000);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to Wi-Fi!");
        return true;  // Successful connection
    } else {
        Serial.println("Wi-Fi connection failed.");
        return false;  // Connection failed
    }
}

// Function to update battery display bars
void updateBatteryDisplay(int bars) {
    // Draw battery bars
    for (int i = 0; i < 5; i++) {
        if (i < bars) {
            // Draw a filled bar
            display.fillRect(2 + (i * 6), 0, 5, 10);
        } else {
            // Draw an empty bar
            display.drawRect(2 + (i * 6), 0, 5, 10);
        }
    }

    // Add the positive terminal to the end of the fifth bar
    display.fillRect(2 + (5 * 6), 2, 2, 6);
}

// Function to enter deep sleep and monitor changes on GPIO01
void enterDeepSleep() {
    // Turn off the SCD30 sensor to save power
    airSensor.StopMeasurement();

    // Set GPIO37 to HIGH to disable battery reading
    digitalWrite(GPIO_BATTERY_CTRL, HIGH);

    display.clear();
    display.drawString(0, 0, "Low Bat");
    display.drawString(0, 10, "Go sleep...");
    display.display();
    delay(1000);  // Display message for 1 second

    // Configure GPIO01 as a wake-up source on any level change
    esp_sleep_enable_ext1_wakeup(1ULL << GPIO_NUM_1, ESP_EXT1_WAKEUP_ANY_HIGH);

    // Enter deep sleep
    esp_deep_sleep_start();
}