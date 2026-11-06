#pragma once
#include <TFT_eSPI.h>
void drawStorageStatus(TFT_eSPI &display, uint16_t background, uint16_t foreground, bool force);

void setPcConnectionStatus(bool connected);
