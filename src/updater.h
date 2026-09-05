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

// Called in setup once WiFi is connected, before the web server / MQTT start:
// installs a pending update on the clean boot heap, if the user requested one
// last session. Returns true if a pending update was processed.
bool updaterRunPendingOnBoot();

// Checks GitHub (api.github.com) for a new version, but does NOT install it.
// If a new version is found, it is remembered and UPDATE_RESULT_AVAILABLE is returned.
// Call updaterInstall() next.
UpdateCheckResult updaterCheck();

// Installs a previously found firmware update (asset from the latest release).
// Call only if updaterAvailable() == true (after a successful check).
// On success the device reboots. Returns UPDATE_RESULT_OK/NO_UPDATES/ERROR.
UpdateCheckResult updaterInstall();

// Request installing the found update. Persists a pending flag (the update
// happens at the next boot on the clean heap - the device reboots now).
bool updaterRequestInstall();
// If an install was requested, reboots the device so the pending update is
// installed on the clean boot heap by updaterRunPendingOnBoot(). Called from
// loop(). Returns true if a request was processed.
bool updaterRunInstall();

// Whether a new version has been found but not yet installed.
bool updaterAvailable();

// Latest release version (for display), empty string if not found.
const String &updaterLatestVersion();

// Human-readable outcome of the most recent install attempt ("", "OK",
// "installing...", or an error string).
const char *updaterLastResult();

#endif
