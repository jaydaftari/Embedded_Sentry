// Team members
// Jay Daftari(jd5829)
// Akshat Mishra(am15111)
// Chirranjeavi Murthi(cm6855)
// Header files
#include <mbed.h>
#include "math.h"

// Custom header files interfacing with gyroscope, LCD on the STM32F429I Discovery board
#include "configuration.h"
#include "gyroscope.h"
#include "./drivers/LCD_DISCO_F429ZI.h"

// LCD object
LCD_DISCO_F429ZI display;
static BufferedSerial serialComm(USBTX, USBRX);

// Define onboard LEDs
DigitalOut greenLED(LED1);
DigitalOut redLED(LED2);

// Define User Button with PA_0
DigitalIn userBtn(PA_0);

// Timer for button press duration

Timer visualTimer;
Timer pressTimer;
bool timerActive = false;

// Variables for button state tracking
int btnStatus = 0;
int prevBtnStatus = 0;

// Constants for gesture recording
#define RECORD_TIME 8.0f // 8 seconds
#define SAMPLE_DELAY 0.08f  // 80 ms interval
#define MAX_SAMPLES (int)(RECORD_TIME / SAMPLE_DELAY) // Should be 100 samples
#define LCD_WIDTH 240
#define LCD_HEIGHT 320
// Arrays to store gyroscope data
float keyData[MAX_SAMPLES][3]; // Recorded key sequence
float attemptData[MAX_SAMPLES][3];   // Unlock attempt sequence

// Flag to check if key has been recorded
bool keyStored = false;

// Function prototypes
void setupGyro();
void setupDisplay();
void captureGesture(float data[][3], const char* message);
bool matchGestures(float data1[][3], float data2[][3]);
void drawGreenTick();
void drawRedCross();
void showTimerScreen();
void displayCounterScreen();
// Main function
int main() {
    // Initialize the Gyroscope
    setupGyro();
    
    // Initialize the LCD
    setupDisplay();

    int systemMode = 0; // 0 - Idle, 1 - Record Key, 2 - Unlock

    while (true) {
        btnStatus = userBtn.read();

        if (btnStatus && !prevBtnStatus) {
            // Button was just pressed
            pressTimer.reset();
            pressTimer.start();
            timerActive = true;
        } else if (!btnStatus && prevBtnStatus) {
            // Button was just released
            pressTimer.stop();
            float pressLength = pressTimer.read();
            timerActive = false;

            if (pressLength >= 2.0f) {
                // Long press - enter Record Key mode
                if (systemMode == 0) {
                    systemMode = 1; // Record Key mode

                    // Indicate to the user that recording is starting
                    display.Clear(LCD_COLOR_DARKYELLOW);
                    BSP_LCD_SetFont(&Font16);
                    display.DisplayStringAt(0, LINE(3), (uint8_t *)"Recording Key Gesture", CENTER_MODE);

                    // Record the key gesture with counter
                    captureGesture(keyData, "Recording Key...");

                    // After recording, display "Key Recorded"
                    display.Clear(LCD_COLOR_BLACK);
                    display.DisplayStringAt(0, LINE(5), (uint8_t *)"Key Saved", CENTER_MODE);
                    ThisThread::sleep_for(2s);

                    keyStored = true;
                    systemMode = 0; // Return to Idle mode
                    setupDisplay(); // Go back to main menu
                }
            } else {
                // Short press - enter Unlock mode
                if (systemMode == 0) {
                    if (keyStored) {
                        systemMode = 2; // Unlock mode
                        display.Clear(LCD_COLOR_BLACK);
                        BSP_LCD_SetFont(&Font16);
                        display.DisplayStringAt(0, LINE(3), (uint8_t *)"Perform Unlock", CENTER_MODE);
                        display.DisplayStringAt(0, LINE(4), (uint8_t *)"Gesture Please...", CENTER_MODE);

                        // Record the unlock gesture with counter
                        captureGesture(attemptData, "Rec Unlock...");

                        // Compare unlockData to recordedData
                        bool unlockSuccess = matchGestures(keyData, attemptData);
                        if (unlockSuccess) {
                            // Indicate success
                            greenLED = 1;
                            redLED = 0;
                            drawGreenTick(); // Draw the green tick symbol
                            display.DisplayStringAt(0, LINE(2), (uint8_t *)"Congrats, Unlocked!", CENTER_MODE);
                            // '''display.Clear(LCD_COLOR_GREEN);
                            // display.SetTextColor(LCD_COLOR_WHITE);
                            // display.SetBackColor(LCD_COLOR_BLACK);'''
                            //display.DisplayStringAt(0, LINE(5), (uint8_t *)"Congrats, Unlocked!", CENTER_MODE);
                        } else {
                            // Indicate failure
                            greenLED = 0;
                            redLED = 1;
                            drawRedCross(); // Draw the red cross symbol
                            display.DisplayStringAt(0, LINE(2), (uint8_t *)"Try Again", CENTER_MODE);
                            // '''display.Clear(LCD_COLOR_RED);
                            // display.SetTextColor(LCD_COLOR_WHITE);
                            // display.SetBackColor(LCD_COLOR_BLACK);
                            // display.DisplayStringAt(0, LINE(5), (uint8_t *)"Try Again", CENTER_MODE);'''
                        }
                        ThisThread::sleep_for(2s);
                        systemMode = 0; // Return to Idle mode
                        setupDisplay(); // Go back to main menu
                    } else {
                        // No key recorded yet
                        display.Clear(LCD_COLOR_BLACK);
                        BSP_LCD_SetFont(&Font16);
                        display.DisplayStringAt(0, LINE(5), (uint8_t *)"No Key saved", CENTER_MODE);
                        ThisThread::sleep_for(2s);
                        systemMode = 0;
                        setupDisplay(); // Go back to main menu
                    }
                }
            }
        } else {
            if (timerActive && pressTimer.read() >= 5.0f) {
                // If button is held for too long,      
                pressTimer.stop();
                timerActive = false;
            }
        }

        prevBtnStatus = btnStatus;

        ThisThread::sleep_for(10ms); // Small delay to prevent tight looping
    }

    return 0;
}


//display counter on screen
void displayCounterScreen() {
    int lastSec = -1;
    while (timerActive) {
        btnStatus = userBtn.read();  // Check button state continuously

        if (!btnStatus) {  // Button released
            timerActive = false;  // Stop the timer
            break;
        }

        int elapsedSec = (int)visualTimer.read();
        if (elapsedSec != lastSec) {
            lastSec = elapsedSec;
            char counterMsg[30];
            sprintf(counterMsg, "%d seconds...", elapsedSec);
            display.ClearStringLine(LINE(6));
            display.DisplayStringAt(0, LINE(6), (uint8_t *)counterMsg, CENTER_MODE);
        }

        ThisThread::sleep_for(100ms);  // Reduce CPU load
    }
}

// Function to show timer visualization screen
void showTimerScreen() {
    display.Clear(LCD_COLOR_BLACK);
    int lastSec = -1;
    while (timerActive) {
        int seconds = (int)pressTimer.read();
        if (seconds != lastSec) {
            lastSec = seconds;
            char timerMsg[20];
            sprintf(timerMsg, "Time: %d s", seconds);
            display.ClearStringLine(LINE(6));
            display.DisplayStringAt(0, LINE(6), (uint8_t *)timerMsg, CENTER_MODE);
        }
        ThisThread::sleep_for(200ms);  // Reduce screen refresh rate
    }
}

//red cross
void drawRedCross() {
    // Clear the display and set the background to black
    display.Clear(LCD_COLOR_DARKRED);

    // Draw a black-filled square at the center
    int squareX = 80, squareY = 80, squareSize = 100;
    display.SetTextColor(LCD_COLOR_BLACK);
    display.FillRect(squareX, squareY, squareSize, squareSize);

    // Draw a bold red cross inside the square
    display.SetTextColor(LCD_COLOR_DARKRED);
    for (int i = -2; i <= 2; i++) { // Thicker lines
        display.DrawLine(squareX + 30, squareY + 30 + i, squareX + 90, squareY + 90 + i);  // First diagonal
        display.DrawLine(squareX + 90, squareY + 30 + i, squareX + 30, squareY + 90 + i);  // Second diagonal
    }
}

//green Tick
void drawGreenTick() {
    // Clear the display and set the background to black
    display.Clear(LCD_COLOR_DARKGREEN);

    // Draw a black-filled square at the center
    int squareX = 80, squareY = 80, squareSize = 100;
    display.SetTextColor(LCD_COLOR_BLACK);
    display.FillRect(squareX, squareY, squareSize, squareSize);

    // Draw a bold green tick inside the square
    display.SetTextColor(LCD_COLOR_GREEN);
    for (int i = 0; i < 4; i++) { // Thicker lines
        display.DrawLine(squareX + 30, squareY + 60 + i, squareX + 50, squareY + 80 + i);  // First part of the tick
        display.DrawLine(squareX + 50, squareY + 80 + i, squareX + 90, squareY + 40 + i);  // Second part of the tick
    }
}

// Function to initialize the Gyroscope
void setupGyro() {
    // SPI configuration
    spi.format(8, 3);
    spi.frequency(1'000'000);

    // Initializing the gyroscope
    int GyroID = Gyro_Init();
    printf("Gyro_ID: %d\n", GyroID);
}

// Function to initialize the LCD
void setupDisplay() {
    // Clear the display and set the background color
    display.Clear(LCD_COLOR_BLACK);

    // Set the font and text color for the main UI
    BSP_LCD_SetFont(&Font16); 
    display.SetTextColor(LCD_COLOR_WHITE);
    display.SetBackColor(LCD_COLOR_BLACK);

    // Display the main title
    display.DisplayStringAt(0, 10, (uint8_t *)"Embedded Sentry", CENTER_MODE);
    display.DisplayStringAt(0, 40, (uint8_t *)"Gesture-Based System", CENTER_MODE);

    // Draw a horizontal line to separate sections
    display.DrawLine(0, 70, LCD_WIDTH, 70);

    // Draw "Record Key" section
    display.SetTextColor(LCD_COLOR_BLACK); 
    display.SetBackColor(LCD_COLOR_DARKGREEN); 
    display.FillRect(20, 80, LCD_WIDTH - 40, 40); 
    display.SetTextColor(LCD_COLOR_WHITE); 
    display.DisplayStringAt(0, 90, (uint8_t *)"Record Key:", CENTER_MODE);
    display.DisplayStringAt(0, 110, (uint8_t *)"Press Button for 2s", CENTER_MODE);

    // Add more space before "Unlock"
    display.SetTextColor(LCD_COLOR_BLACK); 
    display.SetBackColor(LCD_COLOR_BLUE); 
    display.FillRect(20, 140, LCD_WIDTH - 40, 40); 
    display.SetTextColor(LCD_COLOR_WHITE); 
    display.DisplayStringAt(0, 150, (uint8_t *)"Unlock:", CENTER_MODE);
    display.DisplayStringAt(0, 170, (uint8_t *)"Tap Button to Unlock", CENTER_MODE);

    // Draw another horizontal line to separate sections
    display.DrawLine(0, 200, LCD_WIDTH, 200);

    // Draw "Team Members" section
    BSP_LCD_SetFont(&Font16);
    display.SetTextColor(LCD_COLOR_BLACK); 
    display.SetBackColor(LCD_COLOR_DARKMAGENTA); 
    display.FillRect(20, 210, LCD_WIDTH - 40, 40); 
    display.SetTextColor(LCD_COLOR_WHITE); 
    display.DisplayStringAt(0, 220, (uint8_t *)"TEAM MEMBERS", CENTER_MODE);
    display.DisplayStringAt(0, 240, (uint8_t *)"jd5829, am15111", CENTER_MODE);
    display.DisplayStringAt(0, 260, (uint8_t *)"cm6855", CENTER_MODE);
    // Turn off LEDs
    greenLED = 0;
    redLED = 0;
}




// Function to record a gesture with counter display
void captureGesture(float data[][3], const char* message) {
    Timer captureTimer;
    captureTimer.start();

    int lastSec = -1;

    for (int i = 0; i < MAX_SAMPLES; i++) {
        float gyroXYZ[3];
        Gyro_Get_XYZ(gyroXYZ);
        data[i][0] = gyroXYZ[0];
        data[i][1] = gyroXYZ[1];
        data[i][2] = gyroXYZ[2];
        // printf("X coordinate: %d\n", gyroXYZ[0]);
        // printf("Y coordinate: %d\n", gyroXYZ[1]);
        // printf("Z coordinate: %d\n", gyroXYZ[2]);
        
        
        int elapsedSec = (int)captureTimer.read();
        if (elapsedSec != lastSec) {
            lastSec = elapsedSec;
            // Update LCD with counter
            char counterMsg[30];
            sprintf(counterMsg, "%s %d s", message, elapsedSec);
            display.ClearStringLine(LINE(6));
            display.DisplayStringAt(0, LINE(6), (uint8_t *)counterMsg, CENTER_MODE);
        }

        ThisThread::sleep_for(chrono::milliseconds(int(SAMPLE_DELAY * 1000)));
    }

    captureTimer.stop();
}

// Function to compare two gestures
bool matchGestures(float data1[][3], float data2[][3]) {
    float totalError = 0.0f;
    for (int i = 0; i < MAX_SAMPLES; i++) {
        float diff_x = data1[i][0] - data2[i][0];
        float diff_y = data1[i][1] - data2[i][1];
        float diff_z = data1[i][2] - data2[i][2];
        float error = sqrt(diff_x * diff_x + diff_y * diff_y + diff_z * diff_z);
        totalError += error;
    }
    float avgError = totalError / MAX_SAMPLES;
    const float ERROR_LIMIT = 16.0f; // Adjust
    if (avgError <= ERROR_LIMIT) {
        return true;
    } else {
        return false;
    }
}