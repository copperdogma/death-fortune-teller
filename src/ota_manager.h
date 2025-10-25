#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <functional>

class OTAManager {
public:
    OTAManager();
    
    bool begin(const String& hostname, const String& password = "");
    void update();
    bool isEnabled() const;
    bool isUpdating() const;
    bool isPasswordProtected() const;
    bool disabledForMissingPassword() const;
    static OTAManager* instance();
    
    // OTA event callbacks
    void setOnStartCallback(std::function<void()> callback);
    void setOnEndCallback(std::function<void()> callback);
    void setOnProgressCallback(std::function<void(unsigned int, unsigned int)> callback);
    void setOnErrorCallback(std::function<void(ota_error_t)> callback);

private:
    bool m_enabled;
    bool m_updating;
    bool m_passwordProtected;
    bool m_disabledForMissingPassword;
    String m_hostname;
    uint8_t m_lastProgressPercent;
    
    std::function<void()> m_onStartCallback;
    std::function<void()> m_onEndCallback;
    std::function<void(unsigned int, unsigned int)> m_onProgressCallback;
    std::function<void(ota_error_t)> m_onErrorCallback;
    
    void setupCallbacks();
    static void onStart();
    static void onEnd();
    static void onProgress(unsigned int progress, unsigned int total);
    static void onError(ota_error_t error);
    
    static OTAManager* s_instance;
};

#endif // OTA_MANAGER_H
