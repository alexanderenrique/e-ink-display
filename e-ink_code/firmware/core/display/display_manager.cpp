#include "display_manager.h"
#include "hardware_config.h"

// Display object instance
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));

DisplayManager::DisplayManager() : _initialized(false) {
}

void DisplayManager::initSPI() {
    // ESP32-C3 has only one SPI peripheral, so we use the default SPI instance
    // Set CS pin as OUTPUT before initializing SPI
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    
    // Initialize SPI with custom pins for ESP32-C3
    // Parameter order: SCK, MISO, MOSI, CS
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, CS_PIN);
    
    // Connect the SPI instance to the display with appropriate settings
    // 4MHz clock, MSB first, SPI mode 0
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
}

void DisplayManager::disableSPI() {
    SPI.end();
    Serial.println("SPI disabled");
    
    // Set SPI pins to high impedance/low power state to reduce leakage
    pinMode(SPI_SCK, INPUT);
    pinMode(SPI_MOSI, INPUT);
    pinMode(CS_PIN, INPUT);
    pinMode(DC_PIN, INPUT);
    pinMode(RST_PIN, INPUT);
    pinMode(BUSY_PIN, INPUT);
}

bool DisplayManager::begin() {
    initSPI();
    display.init(115200, true, 2, false);
    _initialized = true;
    return true;
}

void DisplayManager::hibernate() {
    display.hibernate();
}

// Helper function to render text with word wrapping
// Returns the final Y position after rendering
int DisplayManager::renderTextWithWrap(String text, int startX, int startY, int maxWidth, int lineHeight, uint16_t textColor) {
    int yPos = startY;
    int xPos = startX;
    String word = "";
    
    // First, collect all words into an array for better look-ahead
    String words[100]; // Max 100 words
    bool isNewline[100]; // Track which entries are newlines
    int wordCount = 0;
    int wordStart = 0;
    
    for (int i = 0; i <= text.length(); i++) {
        char c = (i < text.length()) ? text.charAt(i) : ' ';
        if (c == '\n' || c == ' ') {
            if (i > wordStart && wordCount < 100) {
                words[wordCount] = text.substring(wordStart, i);
                isNewline[wordCount] = false;
                wordCount++;
            }
            if (c == '\n' && wordCount < 100) {
                words[wordCount] = ""; // Empty string marks newline
                isNewline[wordCount] = true;
                wordCount++;
            }
            wordStart = i + 1;
        }
    }
    
    // Now render words with smart wrapping
    for (int i = 0; i < wordCount; i++) {
        if (isNewline[i]) {
            // Handle explicit newline
            yPos += lineHeight;
            xPos = startX;
            continue;
        }
        
        String currentWord = words[i];
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(currentWord, xPos, yPos, &x1, &y1, &w, &h);
        
        // Check if current word fits on current line
        bool fitsOnCurrentLine = (xPos + w <= maxWidth);
        
        // Look ahead to next word to avoid orphaned short words
        bool shouldWrap = false;
        if (!fitsOnCurrentLine && xPos > startX) {
            // Word doesn't fit, need to wrap
            shouldWrap = true;
        } else if (fitsOnCurrentLine && xPos > startX && i + 1 < wordCount && !isNewline[i + 1]) {
            // Word fits, but check if next word would also fit
            String nextWord = words[i + 1];
            int16_t nx1, ny1;
            uint16_t nw, nh;
            display.getTextBounds(nextWord, xPos + w + 5, yPos, &nx1, &ny1, &nw, &nh);
            
            // If next word wouldn't fit, and current word is short (<= 4 chars), wrap both
            if (xPos + w + 5 + nw > maxWidth && currentWord.length() <= 4) {
                shouldWrap = true;
            }
        }
        
        if (shouldWrap) {
            yPos += lineHeight;
            xPos = startX;
            // Recalculate bounds at new position
            display.getTextBounds(currentWord, xPos, yPos, &x1, &y1, &w, &h);
        }
        
        display.setCursor(xPos, yPos);
        display.print(currentWord);
        xPos += w + 5; // Space between words
    }
    
    // Return final Y position (add lineHeight for next line)
    return yPos + lineHeight;
}

// Helper function to display battery percentage in upper right corner in red
void DisplayManager::displayBatteryPercentage(int batteryPercent) {
    if (batteryPercent < 0) return; // Skip if invalid
    
    String batteryText = String(batteryPercent) + "%";
    
    // Get text bounds to position in upper right corner
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(batteryText, 0, 0, &x1, &y1, &w, &h);
    
    // Position in upper right corner with padding (10 pixels from right edge, aligned with header text)
    int displayWidth = display.width();
    int xPos = displayWidth - w - 10;
    int yPos = 20;
    
    // Display in red
    display.setTextColor(GxEPD_RED);
    display.setCursor(xPos, yPos);
    display.print(batteryText);
}

// Display function specifically for earthquake facts
void DisplayManager::displayEarthquakeFact(String earthquakeData, int batteryPercent) {
    // Reinitialize SPI if it was disabled
    initSPI();
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(-1); // Landscape orientation
    display.setFont(&FreeMonoBold9pt7b);
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Display battery percentage in upper right corner
        if (batteryPercent >= 0) {
            displayBatteryPercentage(batteryPercent);
        }
        
        // Parse the earthquake data (format: "Latest Earthquake\nM X.X - Location\nDate Time TZ")
        int yPos = 20;
        int lineHeight = 25;
        int startX = 10;
        int maxWidth = 280;
        
        // Split by newlines
        int pos = 0;
        int lineNum = 0;
        while (pos < earthquakeData.length()) {
            int newlinePos = earthquakeData.indexOf('\n', pos);
            String line = "";
            if (newlinePos == -1) {
                line = earthquakeData.substring(pos);
                pos = earthquakeData.length();
            } else {
                line = earthquakeData.substring(pos, newlinePos);
                pos = newlinePos + 1;
            }
            
            // First line (title) in red, rest in black
            if (lineNum == 0) {
                display.setTextColor(GxEPD_RED);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            int finalY = renderTextWithWrap(line, startX, yPos, maxWidth, lineHeight, 
                                             (lineNum == 0) ? GxEPD_RED : GxEPD_BLACK);
            yPos = finalY;
            lineNum++;
        }
    } while (display.nextPage());
    
    display.hibernate();
}

// Display function for ISS data
void DisplayManager::displayISSData(String issData, int batteryPercent) {
    // Reinitialize SPI if it was disabled
    initSPI();
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(1); // Landscape orientation
    display.setFont(&FreeMonoBold9pt7b);
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Display battery percentage in upper right corner
        if (batteryPercent >= 0) {
            displayBatteryPercentage(batteryPercent);
        }
        
        // Parse the ISS data
        int yPos = 20;
        int lineHeight = 25;
        int startX = 10;
        int maxWidth = 280;
        
        // Split by newlines
        int pos = 0;
        int lineNum = 0;
        while (pos < issData.length()) {
            int newlinePos = issData.indexOf('\n', pos);
            String line = "";
            if (newlinePos == -1) {
                line = issData.substring(pos);
                pos = issData.length();
            } else {
                line = issData.substring(pos, newlinePos);
                pos = newlinePos + 1;
            }
            
            // First line in red, rest in black
            if (lineNum == 0) {
                display.setTextColor(GxEPD_RED);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            
            display.setCursor(startX, yPos);
            display.print(line);
            yPos += lineHeight;
            lineNum++;
        }
    } while (display.nextPage());
    
    display.hibernate();
}

// Display shown on cold boot when in BLE configuration mode
void DisplayManager::displayBluetoothConfigMode() {
    initSPI();
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(1); // Landscape orientation
    display.setFont(&FreeMonoBold9pt7b);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        int yPos = 20;
        int lineHeight = 25;
        int startX = 10;
        int maxWidth = 280;

        // First line in red
        display.setTextColor(GxEPD_RED);
        int finalY = renderTextWithWrap("Bluetooth Config Mode", startX, yPos, maxWidth, lineHeight, GxEPD_RED);

        // Second line in black
        display.setTextColor(GxEPD_BLACK);
        renderTextWithWrap("Visit Denton.Works/e-ink to configure your display", startX, finalY, maxWidth, lineHeight, GxEPD_BLACK);
    } while (display.nextPage());

    display.hibernate();
}

// Display low battery warning message
void DisplayManager::displayLowBatteryMessage() {
    initSPI();
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(1); // Landscape orientation
    display.setFont(&FreeMonoBold9pt7b);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        int yPos = 20;
        int lineHeight = 25;
        int startX = 10;
        int maxWidth = 280;

        // First line in red
        display.setTextColor(GxEPD_RED);
        int finalY = renderTextWithWrap("Battery Low", startX, yPos, maxWidth, lineHeight, GxEPD_RED);

        // Second line in black
        display.setTextColor(GxEPD_BLACK);
        renderTextWithWrap("Please Charge", startX, finalY, maxWidth, lineHeight, GxEPD_BLACK);
    } while (display.nextPage());

    display.hibernate();
}

// Default display function for general text
// First line in red, rest in black
void DisplayManager::displayDefault(String text, int batteryPercent) {
    // Reinitialize SPI if it was disabled
    initSPI();
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(1); // Landscape orientation
    display.setFont(&FreeMonoBold9pt7b);
    
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Display battery percentage in upper right corner
        if (batteryPercent >= 0) {
            displayBatteryPercentage(batteryPercent);
        }
        
        // Split by first newline to separate first line from rest
        int newlinePos = text.indexOf('\n');
        String firstLine = "";
        String restOfText = "";
        
        if (newlinePos > 0) {
            firstLine = text.substring(0, newlinePos);
            restOfText = text.substring(newlinePos + 1);
        } else {
            firstLine = text;
        }
        
        // Display first line in red
        display.setTextColor(GxEPD_RED);
        int finalY = renderTextWithWrap(firstLine, 10, 20, 280, 25, GxEPD_RED);
        
        // Display rest of text in black
        if (restOfText.length() > 0) {
            display.setTextColor(GxEPD_BLACK);
            renderTextWithWrap(restOfText, 10, finalY, 280, 25, GxEPD_BLACK);
        }
    } while (display.nextPage());
    
    display.hibernate();
}
