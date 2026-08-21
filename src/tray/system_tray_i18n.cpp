#include "system_tray_i18n.h"
#include "src/config.h"

#include <map>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace system_tray_i18n {
  // String key constants
  const std::string KEY_QUIT_TITLE = "quit_title";
  const std::string KEY_QUIT_MESSAGE = "quit_message";
  
  // Menu item keys
  const std::string KEY_OPEN_SUNSHINE = "open_sunshine";
  const std::string KEY_VDD_BASE_DISPLAY = "vdd_base_display";
  const std::string KEY_VDD_CREATE = "vdd_create";
  const std::string KEY_VDD_CLOSE = "vdd_close";
  const std::string KEY_VDD_PERSISTENT = "vdd_persistent";
  const std::string KEY_VDD_HEADLESS_CREATE = "vdd_headless_create";
  const std::string KEY_VDD_HEADLESS_CREATE_CONFIRM_TITLE = "vdd_headless_create_confirm_title";
  const std::string KEY_VDD_HEADLESS_CREATE_CONFIRM_MSG = "vdd_headless_create_confirm_msg";
  const std::string KEY_VDD_CONFIRM_CREATE_TITLE = "vdd_confirm_create_title";
  const std::string KEY_VDD_CONFIRM_CREATE_MSG = "vdd_confirm_create_msg";
  const std::string KEY_VDD_CONFIRM_KEEP_TITLE = "vdd_confirm_keep_title";
  const std::string KEY_VDD_CONFIRM_KEEP_MSG = "vdd_confirm_keep_msg";
  const std::string KEY_VDD_CANCEL_CREATE_LOG = "vdd_cancel_create_log";
  const std::string KEY_VDD_PERSISTENT_CONFIRM_TITLE = "vdd_persistent_confirm_title";
  const std::string KEY_VDD_PERSISTENT_CONFIRM_MSG = "vdd_persistent_confirm_msg";
  const std::string KEY_VDD_PREREQUISITE_TITLE = "vdd_prerequisite_title";
  const std::string KEY_VDD_PREREQUISITE_MSG = "vdd_prerequisite_msg";
  const std::string KEY_IMPORT_CONFIG = "import_config";
  const std::string KEY_EXPORT_CONFIG = "export_config";
  const std::string KEY_RESET_TO_DEFAULT = "reset_to_default";
  const std::string KEY_STAR_PROJECT = "star_project";
  const std::string KEY_VISIT_PROJECT = "visit_project";
  const std::string KEY_VISIT_PROJECT_SUNSHINE = "visit_project_sunshine";
  const std::string KEY_VISIT_PROJECT_MOONLIGHT = "visit_project_moonlight";
  const std::string KEY_ADVANCED_SETTINGS = "advanced_settings";
  const std::string KEY_CLOSE_APP = "clear_cache";
  const std::string KEY_CLOSE_APP_CONFIRM_TITLE = "clear_cache_confirm_title";
  const std::string KEY_CLOSE_APP_CONFIRM_MSG = "clear_cache_confirm_msg";
  const std::string KEY_RESET_DISPLAY_DEVICE_CONFIG = "reset_display_device_config";
  const std::string KEY_RESET_DISPLAY_CONFIRM_TITLE = "reset_display_confirm_title";
  const std::string KEY_RESET_DISPLAY_CONFIRM_MSG = "reset_display_confirm_msg";
  const std::string KEY_RESTART = "restart";
  const std::string KEY_QUIT = "quit";
  
  // Notification message keys
  const std::string KEY_STREAM_STARTED = "stream_started";
  const std::string KEY_STREAMING_STARTED_FOR = "streaming_started_for";
  const std::string KEY_STREAM_PAUSED = "stream_paused";
  const std::string KEY_STREAMING_PAUSED_FOR = "streaming_paused_for";
  const std::string KEY_APPLICATION_STOPPED = "application_stopped";
  const std::string KEY_APPLICATION_STOPPED_MSG = "application_stopped_msg";
  const std::string KEY_INCOMING_PAIRING_REQUEST = "incoming_pairing_request";
  const std::string KEY_CLICK_TO_COMPLETE_PAIRING = "click_to_complete_pairing";
  
  // MessageBox keys
  const std::string KEY_IMPORT_SUCCESS_TITLE = "import_success_title";
  const std::string KEY_IMPORT_ERROR_TITLE = "import_error_title";
  const std::string KEY_IMPORT_ERROR_WRITE = "import_error_write";
  const std::string KEY_IMPORT_ERROR_EXCEPTION = "import_error_exception";
  const std::string KEY_EXPORT_SUCCESS_TITLE = "export_success_title";
  const std::string KEY_EXPORT_SUCCESS_MSG = "export_success_msg";
  const std::string KEY_EXPORT_ERROR_TITLE = "export_error_title";
  const std::string KEY_EXPORT_ERROR_WRITE = "export_error_write";
  const std::string KEY_EXPORT_ERROR_NO_CONFIG = "export_error_no_config";
  const std::string KEY_EXPORT_ERROR_EXCEPTION = "export_error_exception";
  const std::string KEY_RESET_CONFIRM_TITLE = "reset_confirm_title";
  const std::string KEY_RESET_CONFIRM_MSG = "reset_confirm_msg";
  const std::string KEY_RESET_SUCCESS_TITLE = "reset_success_title";
  const std::string KEY_RESET_SUCCESS_MSG = "reset_success_msg";
  const std::string KEY_RESET_ERROR_TITLE = "reset_error_title";
  const std::string KEY_RESET_ERROR_MSG = "reset_error_msg";
  const std::string KEY_RESET_ERROR_EXCEPTION = "reset_error_exception";
  const std::string KEY_FILE_DIALOG_SELECT_IMPORT = "file_dialog_select_import";
  const std::string KEY_FILE_DIALOG_SAVE_EXPORT = "file_dialog_save_export";
  const std::string KEY_FILE_DIALOG_CONFIG_FILES = "file_dialog_config_files";

  // English strings. This product ships English-only; there is no other table.
  const std::map<std::string, std::string> DEFAULT_STRINGS = {
    { KEY_QUIT_TITLE, "Wait! Don't Leave Me! T_T" },
    { KEY_QUIT_MESSAGE, "Nooo! You can't just quit like that!\nAre you really REALLY sure you want to leave?\nI'll miss you... but okay, if you must...\n\n(This will also close the Sunshine GUI application.)" },
    { KEY_OPEN_SUNSHINE, "Open GUI" },
    { KEY_VDD_BASE_DISPLAY, "Foundation Display" },
    { KEY_VDD_CREATE, "Create Virtual Display" },
    { KEY_VDD_CLOSE, "Close Virtual Display" },
    { KEY_VDD_PERSISTENT, "Keep Enabled" },
    { KEY_VDD_HEADLESS_CREATE, "Server Mode (Beta)" },
    { KEY_VDD_HEADLESS_CREATE_CONFIRM_TITLE, "[Beta] Enable: Server Mode" },
    { KEY_VDD_HEADLESS_CREATE_CONFIRM_MSG, "This is an internal beta feature. It works differently from \"Keep Enabled\".\n\nThis feature runs only when Sunshine starts or when a stream ends; if a headless host has no display after stream end, it will auto-create the base display to avoid app issues.\n\nExplanation:\nHeadless host: a computer with no physical display connected (or no available display).\nBase display: the built-in screen used by this software, with streaming second screen, privacy screen, custom resolution and refresh rate.\n\nAfter reconnecting a physical display, the base display may stay on. If you get a black screen, try:\n1. Shortcut: Ctrl+Alt+Win+B\n2. Win+P twice then Enter\n3. Restart the app\n4. Start a stream then end it\nIf none works, check HDMI cable, display and keyboard.\n\nEnable this feature?" },
    { KEY_VDD_CONFIRM_CREATE_TITLE, "Create Virtual Display" },
    { KEY_VDD_CONFIRM_CREATE_MSG, "Are you sure you want to manually create a base display?\n\nCreating it may cause a brief black screen, which is normal. If that happens, press Win+P twice to recover.\n\nNote: Prefer creating when not streaming; creating during a stream won't switch to the base display automatically." },
    { KEY_VDD_CONFIRM_KEEP_TITLE, "Confirm Virtual Display" },
    { KEY_VDD_CONFIRM_KEEP_MSG, "Virtual display created, do you want to keep using it?\n\nIf not confirmed, it will be automatically closed in 20 seconds." },
    { KEY_VDD_CANCEL_CREATE_LOG, "User cancelled creating virtual display" },
    { KEY_VDD_PERSISTENT_CONFIRM_TITLE, "Keep Virtual Display Enabled" },
    { KEY_VDD_PERSISTENT_CONFIRM_MSG, "By enabling this option, the virtual display will NOT be closed after you stop streaming.\n\nDo you want to enable this feature?" },
    { KEY_VDD_PREREQUISITE_TITLE, "Virtual Display Driver Unavailable" },
    { KEY_VDD_PREREQUISITE_MSG, "ZakoVDD is missing or unhealthy. Open the Sunshine desktop app and install or repair it from VDD settings before using this action." },
    { KEY_IMPORT_CONFIG, "Import Config" },
    { KEY_EXPORT_CONFIG, "Export Config" },
    { KEY_RESET_TO_DEFAULT, "Reset Config" },
    { KEY_STAR_PROJECT, "Visit Website" },
    { KEY_VISIT_PROJECT, "Visit Project" },
    { KEY_VISIT_PROJECT_SUNSHINE, "Sunshine" },
    { KEY_VISIT_PROJECT_MOONLIGHT, "Moonlight" },
    { KEY_ADVANCED_SETTINGS, "Advanced Settings" },
    { KEY_CLOSE_APP, "Clear Cache" },
    { KEY_CLOSE_APP_CONFIRM_TITLE, "Clear Cache" },
    { KEY_CLOSE_APP_CONFIRM_MSG, "This operation will clear streaming state, may terminate the streaming application, and clean up related processes and state. Do you want to continue?" },
    { KEY_RESET_DISPLAY_DEVICE_CONFIG, "Reset Display" },
    { KEY_RESET_DISPLAY_CONFIRM_TITLE, "Reset Display" },
    { KEY_RESET_DISPLAY_CONFIRM_MSG, "Are you sure you want to reset display device memory? This action cannot be undone." },
    { KEY_RESTART, "Restart" },
    { KEY_QUIT, "Quit" },
    { KEY_STREAM_STARTED, "Stream Started" },
    { KEY_STREAMING_STARTED_FOR, "Streaming started for %s" },
    { KEY_STREAM_PAUSED, "Stream Paused" },
    { KEY_STREAMING_PAUSED_FOR, "Streaming paused for %s" },
    { KEY_APPLICATION_STOPPED, "Application Stopped" },
    { KEY_APPLICATION_STOPPED_MSG, "Application %s successfully stopped" },
    { KEY_INCOMING_PAIRING_REQUEST, "Incoming PIN Request From: %s" },
    { KEY_CLICK_TO_COMPLETE_PAIRING, "Click here to enter PIN" },
    { KEY_IMPORT_SUCCESS_TITLE, "Import Success" },
    { KEY_IMPORT_ERROR_TITLE, "Import Error" },
    { KEY_IMPORT_ERROR_WRITE, "Failed to import configuration file." },
    { KEY_IMPORT_ERROR_EXCEPTION, "An error occurred while importing configuration." },
    { KEY_EXPORT_SUCCESS_TITLE, "Export Success" },
    { KEY_EXPORT_SUCCESS_MSG, "Configuration exported successfully!" },
    { KEY_EXPORT_ERROR_TITLE, "Export Error" },
    { KEY_EXPORT_ERROR_WRITE, "Failed to export configuration file." },
    { KEY_EXPORT_ERROR_NO_CONFIG, "No configuration found to export." },
    { KEY_EXPORT_ERROR_EXCEPTION, "An error occurred while exporting configuration." },
    { KEY_RESET_CONFIRM_TITLE, "Reset Configuration" },
    { KEY_RESET_CONFIRM_MSG, "This will reset all configuration to default values.\nThis action cannot be undone.\n\nDo you want to continue?" },
    { KEY_RESET_SUCCESS_TITLE, "Reset Success" },
    { KEY_RESET_SUCCESS_MSG, "Configuration has been reset to default values.\nPlease restart Sunshine to apply changes." },
    { KEY_RESET_ERROR_TITLE, "Reset Error" },
    { KEY_RESET_ERROR_MSG, "Failed to reset configuration file." },
    { KEY_RESET_ERROR_EXCEPTION, "An error occurred while resetting configuration." },
    { KEY_FILE_DIALOG_SELECT_IMPORT, "Select Configuration File to Import" },
    { KEY_FILE_DIALOG_SAVE_EXPORT, "Save Configuration File As" },
    { KEY_FILE_DIALOG_CONFIG_FILES, "Configuration Files" }
  };

  // Get localized string
  std::string
  get_localized_string(const std::string &key) {
    auto it = DEFAULT_STRINGS.find(key);
    if (it != DEFAULT_STRINGS.end()) {
      return it->second;
    }

    return key;  // Return key if not found
  }

  // Convert UTF-8 string to wide string
  std::wstring
  utf8_to_wstring(const std::string &utf8_str) {
    // Modern C++ approach: use Windows API on Windows, simple conversion on other platforms
  #ifdef _WIN32
    if (utf8_str.empty()) {
      return L"";
    }
    
    // Get required buffer size
    int wide_size = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
    if (wide_size == 0) {
      // Fallback: simple char-by-char conversion
      std::wstring result;
      result.reserve(utf8_str.length());
      for (char c : utf8_str) {
        result += static_cast<wchar_t>(c);
      }
      return result;
    }
    
    // Convert to wide string
    std::wstring result(wide_size - 1, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &result[0], wide_size) == 0) {
      // Fallback: simple char-by-char conversion
      result.clear();
      result.reserve(utf8_str.length());
      for (char c : utf8_str) {
        result += static_cast<wchar_t>(c);
      }
    }
    return result;
  #else
    // On non-Windows platforms, use simple char-by-char conversion
    // This is not perfect for UTF-8, but it's a reasonable fallback
    std::wstring result;
    result.reserve(utf8_str.length());
    for (char c : utf8_str) {
      result += static_cast<wchar_t>(c);
    }
    return result;
  #endif
  }
}  // namespace system_tray_i18n
