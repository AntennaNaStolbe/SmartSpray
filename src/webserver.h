#ifndef WEBSERVER_H
#define WEBSERVER_H

// Init the HTTP server in setup mode (AP: captive portal + config page).
void webInitAp();

// Init the HTTP server in working mode (STA: OTA page, buttons, web firmware upload).
void webInitSta();

// Stop the HTTP server and free its memory (used before an in-app firmware
// download to maximise available heap). Call webInitSta() again to bring it
// back up.
void webStop();

// Process incoming requests. Call in loop().
void webLoop();

#endif
