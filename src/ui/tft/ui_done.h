// File: src/ui/ui_done.h
// Tách từ ui_display.h dòng 814-861

#ifndef UI_DONE_H
#define UI_DONE_H

#include "ui_common.h"

void uiEndStart();
void uiEndUpdate(const ShotData& s, float tsec, bool hasScale);
void uiEndDeinit();
void uiEndRotate(int delta);

#endif // UI_DONE_H
