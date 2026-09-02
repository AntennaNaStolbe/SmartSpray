#ifndef WEBSERVER_H
#define WEBSERVER_H

// Init the HTTP server in setup mode (AP: captive portal + config page).
void webInitAp();

// Init the HTTP server in working mode (STA: OTA page, buttons, web firmware upload).
void webInitSta();

// Process incoming requests. Call in loop().
void webLoop();

#endif
