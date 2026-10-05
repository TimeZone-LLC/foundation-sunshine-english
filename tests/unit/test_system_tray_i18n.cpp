#include <gtest/gtest.h>

#include "src/tray/system_tray_i18n.h"

TEST(SystemTrayCopy, UsesFunctionalQuitPrompt) {
  EXPECT_EQ(
    system_tray_i18n::get_localized_string(system_tray_i18n::KEY_QUIT_TITLE),
    "Quit Sunshine"
  );
  EXPECT_EQ(
    system_tray_i18n::get_localized_string(system_tray_i18n::KEY_QUIT_MESSAGE),
    "Stop Sunshine and close the desktop control panel?"
  );
}
