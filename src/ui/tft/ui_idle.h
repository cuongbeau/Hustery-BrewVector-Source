// File: src/ui/ui_idle.h
// Tách từ ui_display.h dòng 164-569 — Idle screen
// Refactor: 2026-04-20

#ifndef UI_IDLE_H
#define UI_IDLE_H

#include "ui_common.h"

void uiIdleStatic();
void uiIdleDynamic();
void uiIdleDeinit();
void uiIdleToggleProfileDetail();

#endif // UI_IDLE_H
