#include "board_driver.h"
#include "chess_engine.h"
#include "sensor_test.h"
#include "crash_logger.h"
#include "unified_chess_game.h"

// Uncomment the next line to enable WiFi features (requires compatible board)
#define ENABLE_WIFI  // Currently disabled - RP2040 boards use local mode only
#ifdef ENABLE_WIFI
  // Use different WiFi manager based on board type
  #if defined(ESP32) || defined(ESP8266)
    #include "wifi_manager_esp32.h"  // Full WiFi implementation for ESP32/ESP8266
    #define WiFiManager WiFiManagerESP32
  #elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
    #include "wifi_manager.h"  // Full WiFi implementation for boards with WiFiNINA
  #else
    #include "wifi_manager_rp2040.h"  // Placeholder for RP2040 and other boards
    #define WiFiManager WiFiManagerRP2040
  #endif
#endif

// ---------------------------
// Game State and Configuration
// ---------------------------

// Game Mode Definitions
enum GameMode {
  MODE_SELECTION = 0,
  MODE_SENSOR_TEST = 4,
  MODE_UNIFIED_GAME = 6    // Unified game mode (supports any combination)
};

// Global instances
BoardDriver boardDriver;
ChessEngine chessEngine;
SensorTest sensorTest(&boardDriver);
UnifiedChessGame unifiedGame(&boardDriver, &chessEngine);

#ifdef ENABLE_WIFI
WiFiManager wifiManager;
#endif

// Crash logger
CrashLogger crashLogger;

// Current game state
GameMode currentMode = MODE_SELECTION;
bool modeInitialized = false;

// Game selection state
PlayerType selectedWhitePlayer = PLAYER_HUMAN;
PlayerType selectedBlackPlayer = PLAYER_HUMAN;
int selectedWhiteCol = -1;  // Track which column was selected for white
int selectedBlackCol = -1;  // Track which column was selected for black
bool whiteSelected = false;
bool blackSelected = false;

// ---------------------------
// Function Prototypes
// ---------------------------
void showGameSelection();
void handleGameSelection();
void initializeSelectedMode(GameMode mode);
void displayPauseModeComparison();
PlayerType getPlayerTypeFromPosition(int row, int col);

// ---------------------------
// SETUP
// ---------------------------
void setup() {
  // Initialize Serial with extended timeout
  Serial.begin(9600);
  
  // Wait for Serial to be ready (critical for RP2040)
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 10000)) {
    // Wait up to 10 seconds for Serial connection
    delay(100);
  }
  
  // Force a delay to ensure Serial is stable
  delay(2000);
  
  Serial.println();
  Serial.println("================================================");
  Serial.println("         OpenChess Starting Up");
  Serial.println("================================================");
  Serial.println("DEBUG: Serial communication established");
  Serial.print("DEBUG: Millis since boot: ");
  Serial.println(millis());
  
  // Initialize crash logger (checks for previous crashes)
  crashLogger.begin();
  
  // Set global instance for WiFi managers to access
  extern void setCrashLogger(CrashLogger* logger);
  setCrashLogger(&crashLogger);
  
  // Enable watchdog timer (8 second timeout)
  crashLogger.enableWatchdog(8);
  
  // Debug board type detection
  Serial.println("DEBUG: Board type detection:");
  #if defined(ESP32)
  Serial.println("  - Detected: ESP32");
  #elif defined(ESP8266)
  Serial.println("  - Detected: ESP8266");
  #elif defined(ARDUINO_SAMD_MKRWIFI1010)
  Serial.println("  - Detected: ARDUINO_SAMD_MKRWIFI1010");
  #elif defined(ARDUINO_SAMD_NANO_33_IOT)
  Serial.println("  - Detected: ARDUINO_SAMD_NANO_33_IOT");
  #elif defined(ARDUINO_NANO_RP2040_CONNECT)
  Serial.println("  - Detected: ARDUINO_NANO_RP2040_CONNECT");
  #else
  Serial.println("  - Detected: Unknown/Other board type");
  #endif
  
  // Check which mode is compiled
#ifdef ENABLE_WIFI
  Serial.println("DEBUG: Compiled with ENABLE_WIFI defined");
#else
  Serial.println("DEBUG: Compiled without ENABLE_WIFI (local mode only)");
#endif

  Serial.println("DEBUG: About to initialize board driver...");
  // Initialize board driver
  boardDriver.begin();
  Serial.println("DEBUG: Board driver initialized successfully");

#ifdef ENABLE_WIFI
  Serial.println();
  Serial.println("=== WiFi Mode Enabled ===");
  Serial.println("DEBUG: About to initialize WiFi Manager...");
  Serial.println("DEBUG: This will attempt to create Access Point");
  
  // Initialize WiFi Manager
  wifiManager.begin();
  
  Serial.println("DEBUG: WiFi Manager initialization completed");
  Serial.println("If WiFi AP was created successfully, you should see:");
  Serial.println("- Network name: OpenChessBoard");
  Serial.println("- Password: chess123");
  Serial.println("- Web interface: http://192.168.4.1");
  Serial.println("Or place a piece on the board for local selection");
#else
  Serial.println();
  Serial.println("=== Local Mode Only ===");
  Serial.println("WiFi features are disabled in this build");
  Serial.println("To enable WiFi: Uncomment #define ENABLE_WIFI and recompile");
#endif

  Serial.println();
  Serial.println("=== Game Selection Mode ===");
  Serial.println("DEBUG: About to show game selection LEDs...");

  // Show game selection interface
  showGameSelection();
  
  Serial.println("DEBUG: Game selection LEDs should now be visible");
  Serial.println("Four white LEDs should be lit in the center of the board:");
  Serial.println("Position 1 (3,3): Chess Moves (Human vs Human)");
  Serial.println("Position 2 (3,4): Chess Bot (Human vs AI)");
  Serial.println("Position 3 (4,3): Black AI Stockfish (Hard)");
  Serial.println("Position 4 (4,4): Sensor Test");
  Serial.println();
  Serial.println("Place any chess piece on a white LED to select that mode");
  Serial.println("================================================");
  Serial.println("         Setup Complete - Entering Main Loop");
  Serial.println("================================================");
}

// ---------------------------
// MAIN LOOP
// ---------------------------
void loop() {
  static unsigned long lastDebugPrint = 0;
  static unsigned long lastWatchdogFeed = 0;
  static bool firstLoop = true;
  
  if (firstLoop) {
    Serial.println("DEBUG: Entered main loop - system is running");
    firstLoop = false;
  }
  
  // Feed watchdog timer regularly (every 2 seconds)
  if (millis() - lastWatchdogFeed > 2000) {
    crashLogger.feedWatchdog();
    lastWatchdogFeed = millis();
  }
  
  // Print periodic status every 10 seconds
  if (millis() - lastDebugPrint > 10000) {
    Serial.print("DEBUG: Loop running, uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    lastDebugPrint = millis();
  }

#ifdef ENABLE_WIFI
  // Handle WiFi clients
  wifiManager.handleClient();
  
  // Check for pending undo request from WiFi
  if (wifiManager.hasPendingUndoRequest()) {
    Serial.println("Processing undo request from WiFi interface...");
    
    bool undoSuccess = false;
    if (currentMode == MODE_UNIFIED_GAME && modeInitialized) {
      if (unifiedGame.canUndo()) {
        undoSuccess = unifiedGame.undoLastMove();
        if (undoSuccess) {
          Serial.println("Move undone successfully");
        } else {
          Serial.println("Failed to undo move");
        }
      } else {
        Serial.println("No moves to undo");
      }
    } else {
      Serial.println("Warning: Undo requested but not in a supported mode");
    }
    
    wifiManager.setUndoResult(undoSuccess);
    wifiManager.clearUndoRequest();
  }
  
  // Check for pending board edits from WiFi
  char editBoard[8][8];
  if (wifiManager.getPendingBoardEdit(editBoard)) {
    Serial.println("Applying board edit from WiFi interface...");
    
    // Debug: Print first few squares of received board
    Serial.println("DEBUG: Received board (first row, first 4 cols):");
    for (int c = 0; c < 4; c++) {
      Serial.print("  [0][");
      Serial.print(c);
      Serial.print("] = '");
      Serial.print(editBoard[0][c]);
      Serial.println("'");
    }
    
    if (currentMode == MODE_UNIFIED_GAME && modeInitialized) {
      unifiedGame.setBoardState(editBoard);
      Serial.println("Board edit applied to Unified Game mode");
    } else {
      Serial.println("Warning: Board edit received but no active game mode");
    }
    
    wifiManager.clearPendingEdit();
  }
  
  // Update board state for WiFi display
  static unsigned long lastBoardUpdate = 0;
  if (millis() - lastBoardUpdate > 500) { // Update every 500ms
    char currentBoard[8][8];
    bool boardUpdated = false;
    
    float evaluation = 0.0;
    String pgn = "";
    if (currentMode == MODE_UNIFIED_GAME && modeInitialized) {
      unifiedGame.getBoardState(currentBoard);
      evaluation = unifiedGame.getEvaluation();
      pgn = unifiedGame.getPGN();
      boardUpdated = true;
    }
    
    if (boardUpdated) {
      wifiManager.updateBoardState(currentBoard, evaluation, pgn);
    }
    
    lastBoardUpdate = millis();
  }
  
  // Check for WiFi game selection (unified game mode)
  if (wifiManager.hasPlayerSelection()) {
    PlayerType wifiWhitePlayer = wifiManager.getSelectedWhitePlayer();
    PlayerType wifiBlackPlayer = wifiManager.getSelectedBlackPlayer();
    Serial.print("DEBUG: WiFi game selection detected - White: ");
    Serial.print(wifiWhitePlayer);
    Serial.print(", Black: ");
    Serial.println(wifiBlackPlayer);
    
    selectedWhitePlayer = wifiWhitePlayer;
    selectedBlackPlayer = wifiBlackPlayer;
    currentMode = MODE_UNIFIED_GAME;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    wifiManager.resetGameSelection();
    
    // Brief confirmation animation
    for (int i = 0; i < 3; i++) {
      boardDriver.setSquareLED(3, 3, 0, 255, 0, 0); // Green flash
      boardDriver.setSquareLED(3, 4, 0, 255, 0, 0);
      boardDriver.setSquareLED(4, 3, 0, 255, 0, 0);
      boardDriver.setSquareLED(4, 4, 0, 255, 0, 0);
      boardDriver.showLEDs();
      delay(200);
      boardDriver.clearAllLEDs();
      delay(200);
    }
  }
#endif

  if (currentMode == MODE_SELECTION) {
    handleGameSelection();
  } else {
    static bool modeChangeLogged = false;
    if (!modeChangeLogged) {
      Serial.print("DEBUG: Mode changed to: ");
      Serial.println(currentMode);
      modeChangeLogged = true;
    }
    // Initialize the selected mode if not already done
    if (!modeInitialized) {
      initializeSelectedMode(currentMode);
      modeInitialized = true;
    }
    
    // Run the current game mode (only if move detection is not paused)
    bool isPaused = false;
#ifdef ENABLE_WIFI
    isPaused = wifiManager.isMoveDetectionPaused();
#endif
    if (!isPaused) {
      switch (currentMode) {
        case MODE_SENSOR_TEST:
          sensorTest.update();
          break;
        case MODE_UNIFIED_GAME:
          unifiedGame.update();
          // Check if we should return to game selection (e.g., both kings missing)
          if (unifiedGame.shouldReturnToGameSelection()) {
            Serial.println("Returning to game selection mode...");
            unifiedGame.clearReturnToSelectionFlag();
            unifiedGame.reset();
            currentMode = MODE_SELECTION;
            modeInitialized = false;
            showGameSelection();
          }
          break;
        default:
          currentMode = MODE_SELECTION;
          modeInitialized = false;
          showGameSelection();
          break;
      }
    } else {
      // When paused, display comparison between physical board and internal state
      if (modeInitialized) {
        displayPauseModeComparison();
      }
    }
  }
  
  delay(50); // Small delay to prevent overwhelming the system
}

// ---------------------------
// GAME SELECTION FUNCTIONS
// ---------------------------

void showGameSelection() {
  // Clear all LEDs first
  boardDriver.clearAllLEDs();
  
  // White side selection (rank 4, row 3 in array): a4, b4, c4, d4
  // a4 (col 0, row 3) = Human - White
  boardDriver.setSquareLED(3, 0, 0, 0, 0, 255);  // White LED
  // b4 (col 1, row 3) = Easy AI - Green
  boardDriver.setSquareLED(3, 1, 0, 255, 0);  // Green
  // c4 (col 2, row 3) = Medium AI - Yellow
  boardDriver.setSquareLED(3, 2, 255, 255, 0);  // Yellow
  // d4 (col 3, row 3) = Hard AI - Orange
  boardDriver.setSquareLED(3, 3, 255, 165, 0);  // Orange
  
  // Black side selection (rank 5, row 4 in array): a5, b5, c5, d5
  // a5 (col 0, row 4) = Human - White
  boardDriver.setSquareLED(4, 0, 0, 0, 0, 255);  // White LED
  // b5 (col 1, row 4) = Easy AI - Green
  boardDriver.setSquareLED(4, 1, 0, 255, 0);  // Green
  // c5 (col 2, row 4) = Medium AI - Yellow
  boardDriver.setSquareLED(4, 2, 255, 255, 0);  // Yellow
  // d5 (col 3, row 4) = Hard AI - Orange
  boardDriver.setSquareLED(4, 3, 255, 165, 0);  // Orange
  
  // Sensor Test (row 4, col 4) - Red
  boardDriver.setSquareLED(4, 4, 255, 0, 0);
  
  boardDriver.showLEDs();
}

PlayerType getPlayerTypeFromPosition(int row, int col) {
  // White selection: rank 4 (row 3 in array)
  if (row == 3) {
    switch(col) {
      case 0: return PLAYER_HUMAN;      // a4
      case 1: return PLAYER_BOT_EASY;   // b4
      case 2: return PLAYER_BOT_MEDIUM;// c4
      case 3: return PLAYER_BOT_HARD;   // d4
      default: return PLAYER_HUMAN;
    }
  }
  // Black selection: rank 5 (row 4 in array)
  else if (row == 4) {
    switch(col) {
      case 0: return PLAYER_HUMAN;      // a5
      case 1: return PLAYER_BOT_EASY;   // b5
      case 2: return PLAYER_BOT_MEDIUM;// c5
      case 3: return PLAYER_BOT_HARD;   // d5
      default: return PLAYER_HUMAN;
    }
  }
  return PLAYER_HUMAN;
}

void handleGameSelection() {
  boardDriver.readSensors();
  
  // Update display to show current selection state (only if not in the middle of a selection animation)
  static unsigned long lastSelectionTime = 0;
  static bool inSelectionAnimation = false;
  
  if (!inSelectionAnimation && millis() - lastSelectionTime > 600) { // Only update display if not recently selected
    if (whiteSelected && !blackSelected) {
      // White selected, show selected white + all black options
      boardDriver.clearAllLEDs();
      if (selectedWhiteCol >= 0) {
        // Use original color for selected white square
        switch(selectedWhiteCol) {
          case 0: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 0, 0, 255); break; // Human - White
          case 1: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 255, 0); break; // Easy AI - Green
          case 2: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 255, 0); break; // Medium AI - Yellow
          case 3: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 165, 0); break; // Hard AI - Orange
        }
      }
      // Show all black options
      boardDriver.setSquareLED(4, 0, 0, 0, 0, 255);  // Human
      boardDriver.setSquareLED(4, 1, 0, 255, 0);     // Easy AI
      boardDriver.setSquareLED(4, 2, 255, 255, 0);   // Medium AI
      boardDriver.setSquareLED(4, 3, 255, 165, 0);   // Hard AI
      boardDriver.showLEDs();
    } else if (!whiteSelected && blackSelected) {
      // Black selected first, show selected black + all white options
      boardDriver.clearAllLEDs();
      if (selectedBlackCol >= 0) {
        // Use original color for selected black square
        switch(selectedBlackCol) {
          case 0: boardDriver.setSquareLED(4, selectedBlackCol, 0, 0, 0, 255); break; // Human - White
          case 1: boardDriver.setSquareLED(4, selectedBlackCol, 0, 255, 0); break; // Easy AI - Green
          case 2: boardDriver.setSquareLED(4, selectedBlackCol, 255, 255, 0); break; // Medium AI - Yellow
          case 3: boardDriver.setSquareLED(4, selectedBlackCol, 255, 165, 0); break; // Hard AI - Orange
        }
      }
      // Show all white options
      boardDriver.setSquareLED(3, 0, 0, 0, 0, 255);  // Human
      boardDriver.setSquareLED(3, 1, 0, 255, 0);     // Easy AI
      boardDriver.setSquareLED(3, 2, 255, 255, 0);   // Medium AI
      boardDriver.setSquareLED(3, 3, 255, 165, 0);   // Hard AI
      boardDriver.showLEDs();
    } else if (whiteSelected && blackSelected) {
      // Both selected, show only selected squares with original colors
      boardDriver.clearAllLEDs();
      if (selectedWhiteCol >= 0) {
        switch(selectedWhiteCol) {
          case 0: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 0, 0, 255); break; // Human - White
          case 1: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 255, 0); break; // Easy AI - Green
          case 2: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 255, 0); break; // Medium AI - Yellow
          case 3: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 165, 0); break; // Hard AI - Orange
        }
      }
      if (selectedBlackCol >= 0) {
        switch(selectedBlackCol) {
          case 0: boardDriver.setSquareLED(4, selectedBlackCol, 0, 0, 0, 255); break; // Human - White
          case 1: boardDriver.setSquareLED(4, selectedBlackCol, 0, 255, 0); break; // Easy AI - Green
          case 2: boardDriver.setSquareLED(4, selectedBlackCol, 255, 255, 0); break; // Medium AI - Yellow
          case 3: boardDriver.setSquareLED(4, selectedBlackCol, 255, 165, 0); break; // Hard AI - Orange
        }
      }
      boardDriver.showLEDs();
    }
  }
  
  // Check for white side selection (rank 4, row 3)
  if (!whiteSelected) {
    for (int col = 0; col < 4; col++) {
      if (boardDriver.getSensorState(3, col)) {
        selectedWhitePlayer = getPlayerTypeFromPosition(3, col);
        selectedWhiteCol = col;
        whiteSelected = true;
        inSelectionAnimation = true;
        Serial.print("White player selected: ");
        switch(selectedWhitePlayer) {
          case PLAYER_HUMAN: Serial.println("Human"); break;
          case PLAYER_BOT_EASY: Serial.println("Easy AI"); break;
          case PLAYER_BOT_MEDIUM: Serial.println("Medium AI"); break;
          case PLAYER_BOT_HARD: Serial.println("Hard AI"); break;
        }
        // Flash the selected square
        for (int i = 0; i < 3; i++) {
          boardDriver.clearAllLEDs();
          boardDriver.setSquareLED(3, col, 0, 255, 0);
          // Keep all black options lit
          boardDriver.setSquareLED(4, 0, 0, 0, 0, 255);  // Human
          boardDriver.setSquareLED(4, 1, 0, 255, 0);     // Easy AI
          boardDriver.setSquareLED(4, 2, 255, 255, 0);   // Medium AI
          boardDriver.setSquareLED(4, 3, 255, 165, 0);   // Hard AI
          boardDriver.showLEDs();
          delay(200);
          boardDriver.clearAllLEDs();
          // Use original color for selected white square
          switch(col) {
            case 0: boardDriver.setSquareLED(3, col, 0, 0, 0, 255); break; // Human - White
            case 1: boardDriver.setSquareLED(3, col, 0, 255, 0); break; // Easy AI - Green
            case 2: boardDriver.setSquareLED(3, col, 255, 255, 0); break; // Medium AI - Yellow
            case 3: boardDriver.setSquareLED(3, col, 255, 165, 0); break; // Hard AI - Orange
          }
          // Keep all black options lit
          boardDriver.setSquareLED(4, 0, 0, 0, 0, 255);  // Human
          boardDriver.setSquareLED(4, 1, 0, 255, 0);     // Easy AI
          boardDriver.setSquareLED(4, 2, 255, 255, 0);   // Medium AI
          boardDriver.setSquareLED(4, 3, 255, 165, 0);   // Hard AI
          boardDriver.showLEDs();
          delay(200);
        }
        // Keep selected white square (with original color) and all black options lit
        boardDriver.clearAllLEDs();
        switch(col) {
          case 0: boardDriver.setSquareLED(3, col, 0, 0, 0, 255); break; // Human - White
          case 1: boardDriver.setSquareLED(3, col, 0, 255, 0); break; // Easy AI - Green
          case 2: boardDriver.setSquareLED(3, col, 255, 255, 0); break; // Medium AI - Yellow
          case 3: boardDriver.setSquareLED(3, col, 255, 165, 0); break; // Hard AI - Orange
        }
        // Keep all black options lit
        boardDriver.setSquareLED(4, 0, 0, 0, 0, 255);  // Human
        boardDriver.setSquareLED(4, 1, 0, 255, 0);     // Easy AI
        boardDriver.setSquareLED(4, 2, 255, 255, 0);   // Medium AI
        boardDriver.setSquareLED(4, 3, 255, 165, 0);   // Hard AI
        boardDriver.showLEDs();
        lastSelectionTime = millis();
        inSelectionAnimation = false;
        delay(500);
        break;
      }
    }
  }
  
  // Check for black side selection (rank 5, row 4)
  if (!blackSelected) {
    for (int col = 0; col < 4; col++) {
      if (boardDriver.getSensorState(4, col)) {
        selectedBlackPlayer = getPlayerTypeFromPosition(4, col);
        selectedBlackCol = col;
        blackSelected = true;
        inSelectionAnimation = true;
        Serial.print("Black player selected: ");
        switch(selectedBlackPlayer) {
          case PLAYER_HUMAN: Serial.println("Human"); break;
          case PLAYER_BOT_EASY: Serial.println("Easy AI"); break;
          case PLAYER_BOT_MEDIUM: Serial.println("Medium AI"); break;
          case PLAYER_BOT_HARD: Serial.println("Hard AI"); break;
        }
        // Flash the selected square
        for (int i = 0; i < 3; i++) {
          boardDriver.clearAllLEDs();
          boardDriver.setSquareLED(4, col, 0, 255, 0);
          // Also show white selection if it exists (with original color)
          if (whiteSelected && selectedWhiteCol >= 0) {
            switch(selectedWhiteCol) {
              case 0: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 0, 0, 255); break; // Human - White
              case 1: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 255, 0); break; // Easy AI - Green
              case 2: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 255, 0); break; // Medium AI - Yellow
              case 3: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 165, 0); break; // Hard AI - Orange
            }
          } else {
            // Show all white options if white not yet selected
            boardDriver.setSquareLED(3, 0, 0, 0, 0, 255);  // Human
            boardDriver.setSquareLED(3, 1, 0, 255, 0);     // Easy AI
            boardDriver.setSquareLED(3, 2, 255, 255, 0);   // Medium AI
            boardDriver.setSquareLED(3, 3, 255, 165, 0);   // Hard AI
          }
          boardDriver.showLEDs();
          delay(200);
          boardDriver.clearAllLEDs();
          // Use original color for selected black square
          switch(col) {
            case 0: boardDriver.setSquareLED(4, col, 0, 0, 0, 255); break; // Human - White
            case 1: boardDriver.setSquareLED(4, col, 0, 255, 0); break; // Easy AI - Green
            case 2: boardDriver.setSquareLED(4, col, 255, 255, 0); break; // Medium AI - Yellow
            case 3: boardDriver.setSquareLED(4, col, 255, 165, 0); break; // Hard AI - Orange
          }
          if (whiteSelected && selectedWhiteCol >= 0) {
            switch(selectedWhiteCol) {
              case 0: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 0, 0, 255); break; // Human - White
              case 1: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 255, 0); break; // Easy AI - Green
              case 2: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 255, 0); break; // Medium AI - Yellow
              case 3: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 165, 0); break; // Hard AI - Orange
            }
          } else {
            // Show all white options if white not yet selected
            boardDriver.setSquareLED(3, 0, 0, 0, 0, 255);  // Human
            boardDriver.setSquareLED(3, 1, 0, 255, 0);     // Easy AI
            boardDriver.setSquareLED(3, 2, 255, 255, 0);   // Medium AI
            boardDriver.setSquareLED(3, 3, 255, 165, 0);   // Hard AI
          }
          boardDriver.showLEDs();
          delay(200);
        }
        // Keep selected black square and all white options (or selected white if already selected)
        boardDriver.clearAllLEDs();
        switch(col) {
          case 0: boardDriver.setSquareLED(4, col, 0, 0, 0, 255); break; // Human - White
          case 1: boardDriver.setSquareLED(4, col, 0, 255, 0); break; // Easy AI - Green
          case 2: boardDriver.setSquareLED(4, col, 255, 255, 0); break; // Medium AI - Yellow
          case 3: boardDriver.setSquareLED(4, col, 255, 165, 0); break; // Hard AI - Orange
        }
        if (whiteSelected && selectedWhiteCol >= 0) {
          switch(selectedWhiteCol) {
            case 0: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 0, 0, 255); break; // Human - White
            case 1: boardDriver.setSquareLED(3, selectedWhiteCol, 0, 255, 0); break; // Easy AI - Green
            case 2: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 255, 0); break; // Medium AI - Yellow
            case 3: boardDriver.setSquareLED(3, selectedWhiteCol, 255, 165, 0); break; // Hard AI - Orange
          }
        } else {
          // Show all white options if white not yet selected
          boardDriver.setSquareLED(3, 0, 0, 0, 0, 255);  // Human
          boardDriver.setSquareLED(3, 1, 0, 255, 0);     // Easy AI
          boardDriver.setSquareLED(3, 2, 255, 255, 0);   // Medium AI
          boardDriver.setSquareLED(3, 3, 255, 165, 0);   // Hard AI
        }
        boardDriver.showLEDs();
        lastSelectionTime = millis();
        inSelectionAnimation = false;
        delay(500);
        break;
      }
    }
  }
  
  // Check for sensor test (row 4, col 4)
  if (boardDriver.getSensorState(4, 4)) {
    Serial.println("Sensor Test mode selected!");
    currentMode = MODE_SENSOR_TEST;
    modeInitialized = false;
    whiteSelected = false;
    blackSelected = false;
    selectedWhiteCol = -1;
    selectedBlackCol = -1;
    boardDriver.clearAllLEDs();
    delay(500);
  }
  
  // If both sides are selected, start the unified game
  if (whiteSelected && blackSelected) {
    Serial.println("Both players selected! Starting unified chess game...");
    Serial.print("White: ");
    switch(selectedWhitePlayer) {
      case PLAYER_HUMAN: Serial.println("Human"); break;
      case PLAYER_BOT_EASY: Serial.println("Easy AI"); break;
      case PLAYER_BOT_MEDIUM: Serial.println("Medium AI"); break;
      case PLAYER_BOT_HARD: Serial.println("Hard AI"); break;
    }
    Serial.print("Black: ");
    switch(selectedBlackPlayer) {
      case PLAYER_HUMAN: Serial.println("Human"); break;
      case PLAYER_BOT_EASY: Serial.println("Easy AI"); break;
      case PLAYER_BOT_MEDIUM: Serial.println("Medium AI"); break;
      case PLAYER_BOT_HARD: Serial.println("Hard AI"); break;
    }
    
    // Start unified game mode
    currentMode = MODE_UNIFIED_GAME;
    modeInitialized = false;
    whiteSelected = false;
    blackSelected = false;
    selectedWhiteCol = -1;
    selectedBlackCol = -1;
    boardDriver.clearAllLEDs();
    delay(500);
  }
  
  delay(100);
}

void initializeSelectedMode(GameMode mode) {
  switch (mode) {
    case MODE_SENSOR_TEST:
      Serial.println("Starting Sensor Test...");
      sensorTest.begin();
      break;
    case MODE_UNIFIED_GAME:
      Serial.println("Starting Unified Chess Game...");
      unifiedGame.begin(selectedWhitePlayer, selectedBlackPlayer);
      break;
    default:
      currentMode = MODE_SELECTION;
      modeInitialized = false;
      showGameSelection();
      break;
  }
}

// Display comparison between physical board sensors and internal board state when paused
void displayPauseModeComparison() {
  // Read current sensor state
  boardDriver.readSensors();
  
  // Get current board state from active game mode
  char boardState[8][8];
  bool hasBoardState = false;
  
  switch (currentMode) {
    case MODE_UNIFIED_GAME:
      unifiedGame.getBoardState(boardState);
      hasBoardState = true;
      break;
    default:
      // No board state available for this mode
      break;
  }
  
  if (!hasBoardState) {
    // If no board state, just clear LEDs
    boardDriver.clearAllLEDs();
    return;
  }
  
  // Compare sensors with board state and set LEDs
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      bool sensorHasPiece = boardDriver.getSensorState(row, col);
      bool boardHasPiece = (boardState[row][col] != ' ');
      
      if (sensorHasPiece && boardHasPiece) {
        // White light: piece present AND board state has piece (match)
        boardDriver.setSquareLED(row, col, 0, 0, 0, 255); // White using W channel
      } else if (sensorHasPiece && !boardHasPiece) {
        // Red light: piece present BUT board state is empty (extra piece)
        boardDriver.setSquareLED(row, col, 255, 0, 0); // Red
      } else if (!sensorHasPiece && boardHasPiece) {
        // Blue light: board state has piece BUT no piece present (missing piece)
        boardDriver.setSquareLED(row, col, 0, 0, 255); // Blue
      } else {
        // Both empty: turn off LED
        boardDriver.setSquareLED(row, col, 0, 0, 0, 0);
      }
    }
  }
  
  boardDriver.showLEDs();
}
