// Firmware updates over Wi-Fi: POST the .bin to http://<panel>/update
// with basic auth ota:OTA_PASSWORD (see ota.sh). The VacuGlide is stopped first.
#pragma once

void otaBegin();
void otaLoop();
