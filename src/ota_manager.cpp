#include "ota_manager.h"
#include "logging_manager.h"

static constexpr const char* TAG = "OTAManager";

OTAManager* OTAManager::s_instance = nullptr;

OTAManager::OTAManager() 
    : m_enabled(false),
      m_passwordProtected(false),
      m_disabledForMissingPassword(false),
      m_lastProgressPercent(255)
{
    s_instance = this;
}

bool OTAManager::begin(const String& hostname, const String& password) {
    if (hostname.length() == 0) {
        LOG_WARN(TAG, "No hostname provided, OTA disabled");
        m_disabledForMissingPassword = false;
        return false;
    }

    if (password.length() == 0) {
        LOG_ERROR(TAG, "OTA password not configured. OTA will remain disabled.");
        m_enabled = false;
        m_passwordProtected = false;
        m_disabledForMissingPassword = true;
        return false;
    }
    
    m_hostname = hostname;
    
    LOG_INFO(TAG, "Initializing with hostname '%s'", hostname.c_str());
    
    // Set hostname and timeouts
    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setTimeout(20000); // allow slower transfers before timing out

    // Password authentication is mandatory
    ArduinoOTA.setPassword(password.c_str());
    m_passwordProtected = true;
    m_disabledForMissingPassword = false;
    LOG_INFO(TAG, "Password authentication enabled");
    
    // Set port (default is 3232)
    ArduinoOTA.setPort(3232);
    
    // Set up callbacks
    setupCallbacks();
    
    // Start OTA
    ArduinoOTA.begin();
    
    m_enabled = true;
    m_lastProgressPercent = 255;
    LOG_INFO(TAG, "Ready for updates on port 3232");
    
    return true;
}

void OTAManager::update() {
    if (m_enabled) {
        ArduinoOTA.handle();
    }
}

bool OTAManager::isEnabled() const {
    return m_enabled;
}

bool OTAManager::isPasswordProtected() const {
    return m_passwordProtected;
}

bool OTAManager::disabledForMissingPassword() const {
    return m_disabledForMissingPassword;
}

OTAManager* OTAManager::instance() {
    return s_instance;
}

void OTAManager::setOnStartCallback(std::function<void()> callback) {
    m_onStartCallback = callback;
}

void OTAManager::setOnEndCallback(std::function<void()> callback) {
    m_onEndCallback = callback;
}

void OTAManager::setOnProgressCallback(std::function<void(unsigned int, unsigned int)> callback) {
    m_onProgressCallback = callback;
}

void OTAManager::setOnErrorCallback(std::function<void(ota_error_t)> callback) {
    m_onErrorCallback = callback;
}

void OTAManager::setupCallbacks() {
    ArduinoOTA.onStart(onStart);
    ArduinoOTA.onEnd(onEnd);
    ArduinoOTA.onProgress(onProgress);
    ArduinoOTA.onError(onError);
}

void OTAManager::onStart() {
    if (s_instance) {
        LOG_INFO(TAG, "🔄 Update started");
        if (s_instance->m_onStartCallback) {
            s_instance->m_onStartCallback();
        }
    }
}

void OTAManager::onEnd() {
    if (s_instance) {
        LOG_INFO(TAG, "✅ Update completed");
        s_instance->m_lastProgressPercent = 255;
        if (s_instance->m_onEndCallback) {
            s_instance->m_onEndCallback();
        }
    }
}

void OTAManager::onProgress(unsigned int progress, unsigned int total) {
    if (s_instance) {
        unsigned int percent = (progress * 100) / total;
        if (s_instance->m_lastProgressPercent == 255 ||
            percent == 0 ||
            percent >= s_instance->m_lastProgressPercent + 5 ||
            percent == 100) {
            LOG_INFO(TAG, "Progress %u%%", percent);
            s_instance->m_lastProgressPercent = percent;
        }
        if (s_instance->m_onProgressCallback) {
            s_instance->m_onProgressCallback(progress, total);
        }
    }
}

void OTAManager::onError(ota_error_t error) {
    if (s_instance) {
        const char* reason = "Unknown";
        switch (error) {
            case OTA_AUTH_ERROR:      reason = "Authentication failed"; break;
            case OTA_BEGIN_ERROR:     reason = "Begin failed"; break;
            case OTA_CONNECT_ERROR:   reason = "Connection failed"; break;
            case OTA_RECEIVE_ERROR:   reason = "Receive failed"; break;
            case OTA_END_ERROR:       reason = "End failed"; break;
            default: break;
        }
        LOG_ERROR(TAG, "❌ Error %u (%s)", static_cast<unsigned>(error), reason);
        if (error == OTA_AUTH_ERROR) {
            LOG_ERROR(TAG, "🔐 Authentication failed – ensure host upload password matches ota_password on SD");
        } else if (error == OTA_RECEIVE_ERROR) {
            LOG_ERROR(TAG, "📶 Receive failure – check Wi-Fi signal quality and minimize peripheral activity during OTA");
        }
        s_instance->m_lastProgressPercent = 255;
        if (s_instance->m_onErrorCallback) {
            s_instance->m_onErrorCallback(error);
        }
    }
}
