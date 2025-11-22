#ifndef WIFI_HTML_TEMPLATES_H
#define WIFI_HTML_TEMPLATES_H

#include <Arduino.h>

// Include separate HTML/JS/CSS files (auto-generated from html_source/ and js_source/)
#include "html/common_styles.h"
#include "html/game_selection_styles.h"
#include "html/board_view_styles.h"
#include "html/board_edit_styles.h"
#include "html/game_selection.html.h"
#include "html/game_selection_script.h"
#include "html/evaluation_bar.html.h"
#include "html/board_view.html.h"
#include "html/board_edit.html.h"
#include "js/piece_symbols.js.h"
#include "js/evaluation_bar.js.h"
#include "js/board_update.js.h"
#include "js/board_view.js.h"
#include "js/board_edit.js.h"

// Shared HTML templates for both WiFiNINA and ESP32 implementations
namespace WiFiHTMLTemplates {

// Common CSS styles
inline String getCommonStyles() {
    return String("<style>") + String(HTML_COMMON_STYLES) + String("</style>");
}

// Game selection page styles
inline String getGameSelectionStyles() {
    return String("<style>") + String(HTML_GAME_SELECTION_STYLES) + String("</style>");
}

// Board view page styles
inline String getBoardViewStyles() {
    return String("<style>") + String(HTML_BOARD_VIEW_STYLES) + String("</style>");
}

// Board edit page styles
inline String getBoardEditStyles() {
    return String("<style>") + String(HTML_BOARD_EDIT_STYLES) + String("</style>");
}

// Generate HTML head section
inline String generateHTMLHead(String title, String styles) {
    String html = "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>" + title + "</title>";
    html += styles;
    html += "</head>";
    html += "<body>";
    return html;
}

// Generate configuration page
inline String generateConfigPage(String wifiSSID, String wifiPassword, String lichessToken, 
                                  String gameMode, String startupType, String connectionStatus, 
                                  bool showConnectButton) {
    String html = generateHTMLHead("OPENCHESSBOARD CONFIGURATION", getCommonStyles());
    html += "<div class=\"container\">";
    html += "<h2>OPENCHESSBOARD CONFIGURATION</h2>";
    html += "<form action=\"/submit\" method=\"POST\">";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"ssid\">WiFi SSID:</label>";
    html += "<input type=\"text\" name=\"ssid\" id=\"ssid\" value=\"" + wifiSSID + "\" placeholder=\"Enter Your WiFi SSID\">";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"password\">WiFi Password:</label>";
    html += "<input type=\"password\" name=\"password\" id=\"password\" value=\"\" placeholder=\"Enter Your WiFi Password\">";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"token\">Lichess Token (Optional):</label>";
    html += "<input type=\"text\" name=\"token\" id=\"token\" value=\"" + lichessToken + "\" placeholder=\"Enter Your Lichess Token (Future Feature)\">";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"gameMode\">Default Game Mode:</label>";
    html += "<select name=\"gameMode\" id=\"gameMode\">";
    html += "<option value=\"None\"";
    if (gameMode == "None") html += " selected";
    html += ">Local Chess Only</option>";
    html += "<option value=\"5+3\"";
    if (gameMode == "5+3") html += " selected";
    html += ">5+3 (Future)</option>";
    html += "<option value=\"10+5\"";
    if (gameMode == "10+5") html += " selected";
    html += ">10+5 (Future)</option>";
    html += "<option value=\"15+10\"";
    if (gameMode == "15+10") html += " selected";
    html += ">15+10 (Future)</option>";
    html += "<option value=\"AI level 1\"";
    if (gameMode == "AI level 1") html += " selected";
    html += ">AI level 1 (Future)</option>";
    html += "<option value=\"AI level 2\"";
    if (gameMode == "AI level 2") html += " selected";
    html += ">AI level 2 (Future)</option>";
    html += "</select>";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"startupType\">Default Startup Type:</label>";
    html += "<select name=\"startupType\" id=\"startupType\">";
    html += "<option value=\"WiFi\"";
    if (startupType == "WiFi") html += " selected";
    html += ">WiFi Mode</option>";
    html += "<option value=\"Local\"";
    if (startupType == "Local") html += " selected";
    html += ">Local Mode</option>";
    html += "</select>";
    html += "</div>";
    
    html += "<input type=\"submit\" value=\"Save Configuration\">";
    html += "</form>";
    
    // WiFi Connection Status
    html += "<div class=\"form-group\" style=\"margin-top: 30px; padding: 15px; background-color: #444; border-radius: 5px;\">";
    html += "<h3 style=\"color: #ec8703; margin-top: 0;\">WiFi Connection</h3>";
    html += "<p style=\"color: #ec8703;\">Status: " + connectionStatus + "</p>";
    if (showConnectButton) {
        html += "<form action=\"/connect-wifi\" method=\"POST\" style=\"margin-top: 15px;\">";
        html += "<input type=\"hidden\" name=\"ssid\" value=\"" + wifiSSID + "\">";
        html += "<input type=\"hidden\" name=\"password\" value=\"" + wifiPassword + "\">";
        html += "<button type=\"submit\" class=\"button\" style=\"background-color: #4CAF50;\">Connect to WiFi</button>";
        html += "</form>";
        html += "<p style=\"font-size: 12px; color: #ec8703; margin-top: 10px;\">Enter WiFi credentials above and click 'Connect to WiFi' to join your network.</p>";
    }
    html += "</div>";
    
    html += "<a href=\"/game\" class=\"button\">Game Selection Interface</a>";
    html += "<a href=\"/board-view\" class=\"button\">View Chess Board</a>";
    html += "<div class=\"note\">";
    html += "<p>Configure your OpenChess board settings and WiFi connection.</p>";
    html += "</div>";
    html += "</div>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

// Generate game selection page
inline String generateGameSelectionPage() {
    String html = generateHTMLHead("OPENCHESSBOARD GAME SELECTION", getGameSelectionStyles());
    html += "<div class=\"container\">";
    html += "<h2>GAME SELECTION</h2>";
    html += String(HTML_GAME_SELECTION_CONTENT);
    html += "<a href=\"/board-view\" class=\"button\">View Chess Board</a>";
    html += "<a href=\"/\" class=\"back-button\">Back to Configuration</a>";
    html += "</div>";
    html += "<script>";
    html += String(HTML_GAME_SELECTION_SCRIPT);
    html += "</script>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

// Generate piece symbol JavaScript function
inline String generatePieceSymbolJS() {
    return String(JS_PIECE_SYMBOLS);
}

// Generate board view page (fetches board state via JSON)
inline String generateBoardViewPageTemplate() {
    String html = generateHTMLHead("OpenChess Board View", getBoardViewStyles());
    html += "<meta http-equiv=\"refresh\" content=\"2\">"; // Auto-refresh every 2 seconds
    html += "</head>";
    html += "<body>";
    html += String(HTML_BOARD_VIEW_CONTENT);
    html += "<script>";
    html += String(JS_PIECE_SYMBOLS);
    html += String(JS_BOARD_VIEW);
    html += "</script>";
    html += "</body>";
    html += "</html>";
    return html;
}

// Generate board edit page (fetches board state via JSON)
inline String generateBoardEditPageTemplate() {
    String html = generateHTMLHead("Edit Chess Board", getBoardEditStyles());
    html += "</head>";
    html += "<body>";
    html += String(HTML_BOARD_EDIT_CONTENT);
    html += "<script>";
    html += String(JS_BOARD_EDIT);
    html += "</script>";
    html += "</body>";
    html += "</html>";
    return html;
}

// Generate evaluation update JavaScript
inline String generateEvaluationUpdateJS() {
    return String(JS_EVALUATION_UPDATE);
}

} // namespace WiFiHTMLTemplates

#endif // WIFI_HTML_TEMPLATES_H

