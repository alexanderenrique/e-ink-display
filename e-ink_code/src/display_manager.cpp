#include "display_manager.h"
#include "main.h" // For initSPI and getVoltage
#include <SPI.h>

// Helper function to render text with word wrapping
// Returns the final Y position after rendering
int renderTextWithWrap(String text, int startX, int startY, int maxWidth, int lineHeight, uint16_t textColor) {
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
void displayBatteryPercentage() {
  int batteryPercent = getVoltage();
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
// Format: "Latest Earthquake\nM 4.6 - Location\nDate Time PST/PDT"
void displayEarthquakeFact(String earthquakeData) {
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
    displayBatteryPercentage();
    
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
void displayISSData(String issData) {
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
    displayBatteryPercentage();
    
    // Parse the ISS data (format: "Where is the ISS?\nLat, Long\nAlt KM / Miles\nVel KPH / MPH")
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

// Default display function for general text
// First line in red, rest in black
void displayDefault(String fact) {
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
    displayBatteryPercentage();
    
    // Split by first newline to separate first line from rest
    int newlinePos = fact.indexOf('\n');
    String firstLine = "";
    String restOfText = "";
    
    if (newlinePos > 0) {
      firstLine = fact.substring(0, newlinePos);
      restOfText = fact.substring(newlinePos + 1);
    } else {
      firstLine = fact;
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

// Generic display function (fallback)
void displayFact(String fact) {
  displayDefault(fact);
}
