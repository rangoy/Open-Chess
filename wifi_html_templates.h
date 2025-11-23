#ifndef WIFI_HTML_TEMPLATES_H
#define WIFI_HTML_TEMPLATES_H

#include <Arduino.h>

// Include SPA files (auto-generated from html_source/ and js_source/)
#include "html/spa.html.h"
#include "html/spa.css.h"
#include "js/spa.js.h"

// Shared HTML templates for both WiFiNINA and ESP32 implementations
namespace WiFiHTMLTemplates {

// Generate SPA HTML page
inline String generateSPAHTML() {
    return String(SPA_HTML);
}

// Generate SPA CSS
inline String generateSPACSS() {
    return String(SPA_CSS);
}

// Generate SPA JavaScript
inline String generateSPAJS() {
    return String(SPA_JS);
}

} // namespace WiFiHTMLTemplates

#endif // WIFI_HTML_TEMPLATES_H

