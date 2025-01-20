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

// Creation of the display object with static configuration
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);

// Definition of the I2C pins for the SCD30 sensor
#define SDA_SCD30_PIN 45
#define SCL_SCD30_PIN 46

// ADC pin for reading the battery voltage
#define ADC_BATTERY_PIN 1 // Use pin ADC1_CH0 to read the battery voltage
#define GPIO_BATTERY_CTRL 37 // GPIO37 for controlling the battery reading

// Definition of the touch pin
#define TOUCH_PIN 7
int touchThreshold; // Touch detection threshold

// Reference values for the battery (in volts)
const float batteryMaxVoltage = 4.1;
const float batteryMinVoltage = 3.2;
const float lowBatteryThreshold = 3.7;  // 3.7V threshold for deep sleep

bool shouldBlink = false;
unsigned long lastBlinkTime = 0;
bool blinkState = false;
unsigned long lastDataTime = 0;
unsigned long lastWifiAttemptTime = 0;  // Variable to store the last Wi-Fi connection attempt
unsigned long lowBatteryStartTime = 0;  // Variable to store the start of the low battery period
bool lowBatteryWarning = false;  // Flag to track if we are in "low battery" mode
bool wifiConnected = false;  // Global variable for the Wi-Fi connection status

// Initialization of the SCD30 sensor
SCD30 airSensor;
TwoWire I2C_SCD30 = TwoWire(1);

// Variables for the battery measurement average
const int numReadings = 5;
float batteryReadings[numReadings];
int readIndex = 0;
float total = 0;
float averageBatteryVoltage = 0;

// Function to attempt Wi-Fi connection
bool attemptWiFiConnection() {
    Serial.println("Tentativo di connessione al Wi-Fi...");
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    const unsigned long wifiTimeout = 10000;  // 10-second timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
        delay(1000);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connesso al Wi-Fi!");
        return true;  // Connection successful
    } else {
        Serial.println("Connessione Wi-Fi fallita.");
        return false;  // Connection failed
    }
}

// Function to update the battery visualization on the display
void updateBatteryDisplay(int bars) {
    // Draw the battery bars
    for (int i = 0; i < 5; i++) {
        if (i < bars) {
            // Draw a filled bar
            display.fillRect(2 + (i * 6), 0, 5, 10);  // Barre piene più sottili e vicine
        } else {
            // Draw an empty bar
            display.drawRect(2 + (i * 6), 0, 5, 10);  // Barre vuote più sottili e vicine
        }
    }

    // Add the positive terminal at the end of the fifth bar
    display.fillRect(2 + (5 * 6), 2, 2, 6);  // Terminale positivo della batteria sempre all'estremità destra
}

// Function to go into deep sleep and monitor changes on GPIO01
void enterDeepSleep() {
    // Turn off the SCD30 sensor to save energy
    airSensor.StopMeasurement();

    // Set GPIO37 to HIGH to disable battery reading
    digitalWrite(GPIO_BATTERY_CTRL, HIGH);

    display.clear();
    display.drawString(0, 0, "Low Bat");
    display.drawString(0, 10, "Go sleep...");
    display.display();
    delay(1000);  // Show the message for 1 second

    // Configure GPIO01 as a wake-up source on any level change
    esp_sleep_enable_ext1_wakeup(1ULL << GPIO_NUM_1, ESP_EXT1_WAKEUP_ANY_HIGH);

    // Enter deep sleep
    esp_deep_sleep_start();
}

void calibrateTouch() {
    int numSamples = 10;
    int totalValue = 0;

    // Take multiple samples to establish an average "untouched" touch value
    for (int i = 0; i < numSamples; i++) {
        totalValue += touchRead(TOUCH_PIN);
        delay(50); // Small pause between readings to stabilize
    }

    int averageValue = totalValue / numSamples;
    touchThreshold = averageValue + 2000; // Add a tolerance to establish the threshold

    Serial.print("Valore medio del touch non toccato: ");
    Serial.println(averageValue);
    Serial.print("Soglia di rilevamento del touch: ");
    Serial.println(touchThreshold);
}

void setup() {
    // Inizializzazione della seriale
    Serial.begin(115200);

    // Initialize I2C for the OLED display
    Wire.begin(SDA_OLED, SCL_OLED);
    
    // Activation of the Vext Pin to power the OLED
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);

    // Configure GPIO37 as an output to control battery reading
    pinMode(GPIO_BATTERY_CTRL, OUTPUT);
    digitalWrite(GPIO_BATTERY_CTRL, HIGH); // Set initially HIGH (not reading)

    // Display reset
    pinMode(16, OUTPUT);
    digitalWrite(16, LOW);
    delay(50);
    digitalWrite(16, HIGH);

    // Display initialization
    display.init();
    display.setFont(ArialMT_Plain_10);
    display.clear();
    display.drawString(0, 0, "CO2 Sensor");
    display.drawString(10, 10, "SCD-30");
    display.drawString(0, 20, "Con WiFi...");
    display.display();

    // Touch calibration
    calibrateTouch();

    // Wi-Fi connection with timeout
    wifiConnected = attemptWiFiConnection();

    // Initialize a second I2C bus for the SCD30 sensor
    I2C_SCD30.begin(SDA_SCD30_PIN, SCL_SCD30_PIN);

    // Initialization of the SCD30 sensor
    if (airSensor.begin(I2C_SCD30) == false) {
        Serial.println("SCD30 non trovato. Controlla le connessioni.");
        display.drawString(0, 40, "SCD30 NOT");
        display.display();
        while (1);
    } else {
        Serial.println("SCD30 inizializzato con successo.");
        display.clear();

        // Calculation of the battery charge level and number of bars
        int adcValue = 0;
        float batteryVoltage = 0.0;

        digitalWrite(GPIO_BATTERY_CTRL, LOW); // Enable battery reading
        adcValue = analogRead(ADC_BATTERY_PIN);
        digitalWrite(GPIO_BATTERY_CTRL, HIGH); // Disable battery reading

        batteryVoltage = adcValue * (0.004090);
        
        int batteryPercentage = (int)(((batteryVoltage - batteryMinVoltage) / (batteryMaxVoltage - batteryMinVoltage)) * 100);
        if (batteryPercentage > 100) batteryPercentage = 100; // Limit to 100%
        if (batteryPercentage < 0) batteryPercentage = 0; // Limit to 0%
        int bars = batteryPercentage / 20;

        // Show the battery level
        updateBatteryDisplay(bars);

        // Check the Wi-Fi connection status and update the display
        if (wifiConnected) {
            display.drawString(0, 10, "WI-FI OK");
        } else {
            display.drawString(0, 10, "WI-FI NOT");
        }

        // Add a check for the SCD30 sensor
        if (!airSensor.dataAvailable()) {
            Serial.println("SCD30 non risponde. Tentativo di reset...");

            // Perform sensor reset
            airSensor.reset();
            delay(5000);  // Wait a few seconds for the reset

            // Try to reinitialize the sensor
            if (airSensor.begin(I2C_SCD30) == false) {
                Serial.println("SCD30 non trovato dopo il reset. Riprovo...");
                display.drawString(0, 20, "SCD30 NOT");
            } else {
                Serial.println("SCD30 inizializzato con successo dopo reset.");
                display.drawString(0, 20, "SCD30 OK");
            }
        } else {
            display.drawString(0, 20, "Wait!!");
        }

        display.display();
    }

    // Initialization of battery readings
    for (int thisReading = 0; thisReading < numReadings; thisReading++) {
        batteryReadings[thisReading] = 0;
    }

    // Set the reading interval to 8 seconds
    airSensor.setMeasurementInterval(8);

    lastDataTime = millis();

    delay(2000);  // Initial pause to display the message
}

void loop() {
    // Periodic check of the Wi-Fi connection every 10 minutes
    if (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttemptTime > 600000) {  // 600000 ms = 10 minutes
        wifiConnected = attemptWiFiConnection();

        // Update the display based on the Wi-Fi connection status
        display.clear();
        if (wifiConnected) {
            display.drawString(0, 10, "WI-FI OK");
        } else {
            display.drawString(0, 10, "WI-FI NOT");
        }
        display.display();
    }

    // Touch detection on GPIO07 with inverted logic
    int touchValue = touchRead(TOUCH_PIN);
    if (touchValue > touchThreshold) { // Se rileva un tocco con la logica invertita
        display.clear();
        
        String message;
        if (WiFi.status() == WL_CONNECTED) {
            message = WiFi.localIP().toString();
            
        } else {
            message = "NO IP";
        }

        int textWidth = display.getStringWidth(message);
        int displayWidth = display.getWidth();
        
        // Scroll the text if it's longer than the display width
        if (textWidth > displayWidth) {
            for (int i = 0; i <= textWidth - displayWidth; i++) {
                display.clear();
                display.drawString(-i, 10, message); // Scorri il testo a sinistra
                display.drawString(0, 0, "IP:");
                display.display();
                delay(100); // Adjust the scrolling speed here
            }
        } else {
            // If the text fits on the screen, simply show it
            display.drawString(0, 0, message);
            display.drawString(0, 0, "IP:");
            display.display();
        }

        // Wait for the touch to be released
        while (touchRead(TOUCH_PIN) > touchThreshold) {
            delay(10); // Attendi il rilascio del tocco
        }
        
        display.clear(); // Clear the display after release
    }

    // Continue the normal cycle

    // Subtract the old reading from the total
    total = total - batteryReadings[readIndex];

    // Make a new reading
    int adcValue = 0;
    float batteryVoltage = 0.0;

    digitalWrite(GPIO_BATTERY_CTRL, LOW); // Enable battery reading
    adcValue = analogRead(ADC_BATTERY_PIN);
    digitalWrite(GPIO_BATTERY_CTRL, HIGH); // Disable battery reading

    batteryVoltage = adcValue * (0.004090);

    // Check if the battery is below the threshold level
    if (batteryVoltage < lowBatteryThreshold) {
        if (!lowBatteryWarning) {
            lowBatteryWarning = true;
            lowBatteryStartTime = millis();  // Start the 300-second timer
        } else if (millis() - lowBatteryStartTime > 300000) {  // 300000 ms = 300 seconds = 5 minutes
            enterDeepSleep();  // Go into deep sleep after 5 minutes
        }
    } else {
        lowBatteryWarning = false;  // Reset the timer if the voltage is sufficient
    }

    // Add the new reading to the total
    batteryReadings[readIndex] = batteryVoltage;
    total = total + batteryReadings[readIndex];

    // Advance the index, and wrap around if necessary
    readIndex = readIndex + 1;
    if (readIndex >= numReadings) {
        readIndex = 0;
    }

    // Calculate the average
    averageBatteryVoltage = total / numReadings;

    // Calculate the battery charge percentage
    int batteryPercentage = (int)(((averageBatteryVoltage - batteryMinVoltage) / (batteryMaxVoltage - batteryMinVoltage)) * 100);
    if (batteryPercentage > 100) batteryPercentage = 100; // Limit to 100%
    if (batteryPercentage < 0) batteryPercentage = 0; // Limit to 0%

    // Print the average voltage and percentage on the serial monitor
    Serial.print("Tensione media della batteria: ");
    Serial.print(averageBatteryVoltage, 2);  // Print the voltage with two decimals
    Serial.println(" V");

    Serial.print("Percentuale di carica della batteria: ");
    Serial.print(batteryPercentage);  // Print the percentage
    Serial.println(" %");

    // Determine how many bars to fill based on the percentage
    int bars = batteryPercentage / 20;

    // Update the display with the battery bars and the CO2 label
    display.clear();
    updateBatteryDisplay(bars);  // Adds the battery bars

    // Add the "CO2" text after the battery indicator
    display.drawString(40, 0, "CO2");

    // Reading data from the SCD30 sensor
    if (airSensor.dataAvailable()) {
        lastDataTime = millis(); // Reset the timer if data is available

        int co2 = static_cast<int>(airSensor.getCO2()); 
        float temperature = airSensor.getTemperature();
        float humidity = airSensor.getHumidity();

        float compensatedCO2 = co2 * (1 + (0.002 * (humidity - 50)));  // Applying a manual correction for humidity

        Serial.print("CO2 (compensato): ");
        Serial.print(compensatedCO2);
        Serial.println(" ppm");

        Serial.print("Temperatura: ");
        Serial.print(temperature, 1);  // Print the temperature with one decimal
        Serial.println(" °C");

        Serial.print("Umidità: ");
        Serial.print(humidity, 1); // Print the humidity with one decimal
        Serial.println(" %RH");

        display.drawString(0, 10, " " + String(static_cast<int>(compensatedCO2)) + " ppm");
        display.drawString(0, 20, String(temperature, 1) + "C" );
        display.drawString(40, 20, "u" + String(humidity, 0) + "%" );
        display.display();
    } else if (millis() - lastDataTime > 10000) { // If there is no data for more than 10 seconds
        blinkState = !blinkState;

        display.clear();

        // Show the battery voltage in volts instead of bars
        display.drawString(0, 0, String(averageBatteryVoltage, 2) + " V");

        
        display.drawString(0, 10, "NO DATA");
        display.drawString(0, 20, "RETRYING...");
        
        display.display();

        Serial.println("SCD30 non risponde. Tentativo di reset...");

        // Perform sensor reset
        airSensor.reset();
        delay(10000); // Wait 10 seconds to see if the sensor recovers

        // Try to reinitialize the sensor in a loop until it recovers
        while (airSensor.begin(I2C_SCD30) == false) {
            Serial.println("SCD30 non trovato. Riprovo...");
            display.clear();
            display.drawString(0, 0, String(averageBatteryVoltage, 2) + " V");

            display.drawString(0, 10, "NO DATA");
            display.drawString(0, 20, "RETRYING...");
            display.display();

            delay(10000); // Wait another 10 seconds before trying again
        }

        // Reset the reading interval to 8 seconds after the reset
        airSensor.setMeasurementInterval(8);

        Serial.println("SCD30 inizializzato con successo dopo reset.");
        display.clear();
        display.drawString(0, 0, String(averageBatteryVoltage, 2) + " V");
        
        // Check the Wi-Fi connection status and update the display
        if (wifiConnected) {
            display.drawString(0, 10, "WI-FI OK");
        } else {
            display.drawString(0, 10, "WI-FI NOT");
        }
        display.drawString(0, 20, "Wait!!");
        display.display();
        lastDataTime = millis(); // Reset the timer
    }

    delay(2000);  // Wait 5 seconds before the next cycle
}