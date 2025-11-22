#include "board_driver.h"
#include "chess_engine.h"
#include "chess_moves.h"
#include "sensor_test.h"
#include "chess_bot.h"
#include "chess_bot_vs_bot.h"
#include "crash_logger.h"

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
  MODE_CHESS_MOVES = 1,
  MODE_CHESS_BOT = 2,      // Chess vs Bot mode (Medium difficulty)
  MODE_GAME_3 = 3,         // Black AI Stockfish (Medium difficulty)
  MODE_SENSOR_TEST = 4,
  MODE_BOT_VS_BOT = 5      // AI vs AI mode
};

// Global instances
BoardDriver boardDriver;
ChessEngine chessEngine;
ChessMoves chessMoves(&boardDriver, &chessEngine);
SensorTest sensorTest(&boardDriver);
ChessBot chessBot(&boardDriver, &chessEngine, BOT_MEDIUM, true);   // Mode 2: Player White, AI Black, Medium
ChessBot chessBot3(&boardDriver, &chessEngine, BOT_MEDIUM, false);   // Mode 3: Player Black, AI White, Hard
ChessBotVsBot chessBotVsBot(&boardDriver, &chessEngine, BOT_MEDIUM, BOT_MEDIUM, 2000);  // Mode 5: AI vs AI, 2 second delay

#ifdef ENABLE_WIFI
WiFiManager wifiManager;
#endif

// Crash logger
CrashLogger crashLogger;

// Current game state
GameMode currentMode = MODE_SELECTION;
bool modeInitialized = false;

// ---------------------------
// Function Prototypes
// ---------------------------
void showGameSelection();
void handleGameSelection();
void initializeSelectedMode(GameMode mode);
void displayPauseModeComparison();

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
    if (currentMode == MODE_CHESS_MOVES && modeInitialized) {
      if (chessMoves.canUndo()) {
        undoSuccess = chessMoves.undoLastMove();
        if (undoSuccess) {
          Serial.println("Move undone successfully");
        } else {
          Serial.println("Failed to undo move");
        }
      } else {
        Serial.println("No moves to undo");
      }
    } else {
      Serial.println("Warning: Undo requested but not in Chess Moves mode");
    }
    
    wifiManager.setUndoResult(undoSuccess);
    wifiManager.clearUndoRequest();
  }
  
  // Check for pending board edits from WiFi
  char editBoard[8][8];
  if (wifiManager.getPendingBoardEdit(editBoard)) {
    Serial.println("Applying board edit from WiFi interface...");
    
    if (currentMode == MODE_CHESS_MOVES && modeInitialized) {
      chessMoves.setBoardState(editBoard);
      Serial.println("Board edit applied to Chess Moves mode");
    } else if (currentMode == MODE_CHESS_BOT && modeInitialized) {
      chessBot.setBoardState(editBoard);
      Serial.println("Board edit applied to Chess Bot mode");
    } else if (currentMode == MODE_GAME_3 && modeInitialized) {
      chessBot3.setBoardState(editBoard);
      Serial.println("Board edit applied to Black AI Stockfish mode");
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
    if (currentMode == MODE_CHESS_MOVES && modeInitialized) {
      chessMoves.getBoardState(currentBoard);
      pgn = chessMoves.getPGN();
      boardUpdated = true;
    } else if (currentMode == MODE_CHESS_BOT && modeInitialized) {
      chessBot.getBoardState(currentBoard);
      evaluation = chessBot.getEvaluation();
      boardUpdated = true;
    } else if (currentMode == MODE_GAME_3 && modeInitialized) {
      chessBot3.getBoardState(currentBoard);
      evaluation = chessBot3.getEvaluation();
      boardUpdated = true;
    } else if (currentMode == MODE_BOT_VS_BOT && modeInitialized) {
      chessBotVsBot.getBoardState(currentBoard);
      evaluation = chessBotVsBot.getEvaluation();
      boardUpdated = true;
    }
    
    if (boardUpdated) {
      wifiManager.updateBoardState(currentBoard, evaluation, pgn);
    }
    
    lastBoardUpdate = millis();
  }
  
  // Check for WiFi game selection
  int selectedMode = wifiManager.getSelectedGameMode();
  if (selectedMode > 0) {
    Serial.print("DEBUG: WiFi game selection detected: ");
    Serial.println(selectedMode);
    
    switch (selectedMode) {
      case 1:
        currentMode = MODE_CHESS_MOVES;
        break;
      case 2:
        currentMode = MODE_CHESS_BOT;
        break;
      case 3:
        currentMode = MODE_GAME_3;
        break;
      case 4:
        currentMode = MODE_SENSOR_TEST;
        break;
      case 5:
        currentMode = MODE_BOT_VS_BOT;
        break;
      default:
        Serial.println("Invalid game mode selected via WiFi");
        selectedMode = 0;
        break;
    }
    
    if (selectedMode > 0) {
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
        case MODE_CHESS_MOVES:
          chessMoves.update();
          break;
        case MODE_CHESS_BOT:
          chessBot.update();
          break;
        case MODE_GAME_3:
          chessBot3.update();
          break;
        case MODE_SENSOR_TEST:
          sensorTest.update();
          break;
        case MODE_BOT_VS_BOT:
          chessBotVsBot.update();
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
  
  // Light up the 4 selector positions in the middle of the board
  // Each mode has a different color for easy identification
  // Position 1: Chess Moves (row 3, col 3) - Orange
  boardDriver.setSquareLED(3, 3, 255, 165, 0);
  
  // Position 2: Chess Bot (row 3, col 4) - White
  boardDriver.setSquareLED(3, 4, 0, 0, 0, 255);
  
  // Position 3: Black AI Stockfish (row 4, col 3) - Blue
  boardDriver.setSquareLED(4, 3, 0, 0, 255);
  
  // Position 4: Sensor Test (row 4, col 4) - Red
  boardDriver.setSquareLED(4, 4, 255, 0, 0);
  
  // Position 5: Bot vs Bot (row 3, col 2) - Purple/Magenta
  boardDriver.setSquareLED(3, 2, 255, 0, 255);
  
  boardDriver.showLEDs();
}

void handleGameSelection() {
  boardDriver.readSensors();
  
  // Check for piece placement on selector squares
  if (boardDriver.getSensorState(3, 3)) {
    // Chess Moves selected
    Serial.println("Chess Moves mode selected!");
    currentMode = MODE_CHESS_MOVES;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500); // Debounce delay
  }
  else if (boardDriver.getSensorState(3, 4)) {
    // Chess Bot selected
    Serial.println("Chess Bot mode selected (Human vs AI)!");
    currentMode = MODE_CHESS_BOT;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
  }
  else if (boardDriver.getSensorState(4, 3)) {
    // Game Mode 3 selected - Black AI Stockfish (Hard)
    Serial.println("Game Mode 3 selected (Black AI Stockfish - Hard)!");
    currentMode = MODE_GAME_3;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
  }
  else if (boardDriver.getSensorState(4, 4)) {
    // Sensor Test selected
    Serial.println("Sensor Test mode selected!");
    currentMode = MODE_SENSOR_TEST;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
  }
  else if (boardDriver.getSensorState(3, 2)) {
    // Bot vs Bot selected
    Serial.println("Bot vs Bot mode selected (AI vs AI)!");
    currentMode = MODE_BOT_VS_BOT;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
  }
  
  delay(100);
}

void initializeSelectedMode(GameMode mode) {
  switch (mode) {
    case MODE_CHESS_MOVES:
      Serial.println("Starting Chess Moves (Human vs Human)...");
      chessMoves.begin();
      break;
    case MODE_CHESS_BOT:
      Serial.println("Starting Chess Bot (Player White vs AI Black - Medium)...");
      chessBot.begin();
      break;
    case MODE_SENSOR_TEST:
      Serial.println("Starting Sensor Test...");
      sensorTest.begin();
      break;
    case MODE_GAME_3:
      Serial.println("Starting Black AI Stockfish (Player Black vs AI White - Hard)...");
      chessBot3.begin();
      break;
    case MODE_BOT_VS_BOT:
      Serial.println("Starting Bot vs Bot (AI vs AI)...");
      chessBotVsBot.begin();
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
    case MODE_CHESS_MOVES:
      chessMoves.getBoardState(boardState);
      hasBoardState = true;
      break;
    case MODE_CHESS_BOT:
      chessBot.getBoardState(boardState);
      hasBoardState = true;
      break;
    case MODE_GAME_3:
      chessBot3.getBoardState(boardState);
      hasBoardState = true;
      break;
    case MODE_BOT_VS_BOT:
      chessBotVsBot.getBoardState(boardState);
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
