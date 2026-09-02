#ifndef UPDATER_H
#define UPDATER_H

#include <Arduino.h>

// Result of checking/installing updates on GitHub
enum UpdateCheckResult {
  UPDATE_RESULT_OK = 0,         // installation succeeded / update available (see installed flag)
  UPDATE_RESULT_AVAILABLE = 1,  // new version found (after check)
  UPDATE_RESULT_NO_UPDATES = 2, // latest version already installed
  UPDATE_RESULT_ERROR = 3,      // error (network/JSON/no asset)
};

// Initialization (Reserved flash info etc.) — called in setup
void updaterInit();

// Checks GitHub (api.github.com) for a new version, but does NOT install it.
// If a new version is found, it is remembered and UPDATE_RESULT_AVAILABLE is returned.
// Call updaterInstall() next.
UpdateCheckResult updaterCheck();

// Installs a previously found firmware update (asset from the latest release).
// Call only if updaterAvailable() == true (after a successful check).
// On success the device reboots. Returns UPDATE_RESULT_OK/NO_UPDATES/ERROR.
UpdateCheckResult updaterInstall();

// Whether a new version has been found but not yet installed.
bool updaterAvailable();

// Latest release version (for display), empty string if not found.
const String &updaterLatestVersion();

#endif
