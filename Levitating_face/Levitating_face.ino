#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define I2C Pins
#define I2C_SDA 5
#define I2C_SCL 6

// Animation Variables
float angle = 0.0;
unsigned long lastBlinkTime = 0;
unsigned long lastSmileTime = 0;
unsigned long smileDurationEnd = 0;

bool isBlinking = false;
bool isSmiling = false;

// Timing intervals (in milliseconds)
unsigned long nextBlinkInterval = 3000; 
unsigned long nextSmileInterval = 6000;

void setup() {
  Serial.begin(115200);

  // Initialize I2C with specific pins
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  randomSeed(analogRead(0)); // Seed random numbers using floating analog pin
}

void loop() {
  unsigned long currentTime = millis();
  display.clearDisplay();

  // --- 1. HANDLE TIMERS & STATES ---
  
  // Blink timing logic
  if (!isBlinking && (currentTime - lastBlinkTime > nextBlinkInterval)) {
    isBlinking = true;
    lastBlinkTime = currentTime;
  }
  if (isBlinking && (currentTime - lastBlinkTime > 150)) { // Blink lasts 150ms
    isBlinking = false;
    nextBlinkInterval = random(2000, 6000); // Set next random interval
  }

  // Smile timing logic
  if (!isSmiling && (currentTime - lastSmileTime > nextSmileInterval)) {
    isSmiling = true;
    lastSmileTime = currentTime;
    smileDurationEnd = currentTime + random(1500, 3000); // Smile for 1.5 to 3 seconds
  }
  if (isSmiling && (currentTime > smileDurationEnd)) {
    isSmiling = false;
    lastSmileTime = currentTime;
    nextSmileInterval = random(5000, 10000); // Next smile in 5-10 seconds
  }

  // --- 2. CALCULATE FLOATING POSITION ---
  
  // Increment angle to drive the float math (adjust 0.03 to change speed)
  angle += 0.03; 
  if (angle > 2 * PI) angle = 0;

  // X moves on a smooth cosine path, Y on a sine path
  // We offset the center (64, 32) by up to 12 pixels horizontally and 6 vertically
  int offsetX = cos(angle) * 12;
  int offsetY = sin(angle * 2) * 6; // Multiplying angle creates a figure-8 float pattern

  int leftEyeX = 36 + offsetX;
  int rightEyeX = 92 + offsetX;
  int eyesY = 28 + offsetY;

  // --- 3. RENDER FACE ---

  if (isBlinking) {
    // Draw flat lines for closed eyes
    display.drawFastHLine(leftEyeX - 10, eyesY, 20, SSD1306_WHITE);
    display.drawFastHLine(rightEyeX - 10, eyesY, 20, SSD1306_WHITE);
  } else {
    // Draw expressive pupils: big outer circle, dark center, tiny white shine
    display.fillCircle(leftEyeX, eyesY, 12, SSD1306_WHITE);
    display.fillCircle(leftEyeX, eyesY, 6, SSD1306_BLACK);
    display.fillCircle(leftEyeX + 3, eyesY - 3, 2, SSD1306_WHITE); // Catchlight reflection

    display.fillCircle(rightEyeX, eyesY, 12, SSD1306_WHITE);
    display.fillCircle(rightEyeX, eyesY, 6, SSD1306_BLACK);
    display.fillCircle(rightEyeX + 3, eyesY - 3, 2, SSD1306_WHITE); // Catchlight reflection
  }

  // Draw the mouth
  int mouthX = 64 + offsetX;
  int mouthY = 46 + offsetY;
  
  if (isSmiling) {
    // Draw a happy curved smile using an overlapping circles trick
    display.fillCircle(mouthX, mouthY, 12, SSD1306_WHITE);
    display.fillCircle(mouthX, mouthY - 4, 12, SSD1306_BLACK); // Mask out top half
    display.fillRect(mouthX - 12, mouthY - 12, 24, 12, SSD1306_BLACK); // Clean up upper edges
  } else {
    // Default neutral/slightly curious little mouth line
    display.drawFastHLine(mouthX - 6, mouthY + 2, 12, SSD1306_WHITE);
  }

  // Send buffer to hardware
  display.display();
  
  // Small frame delay to keep things running at roughly ~40-50 FPS smoothly
  delay(20); 
}