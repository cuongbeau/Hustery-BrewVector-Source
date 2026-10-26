// File: src/ui/ui_brew.h
// Tách từ ui_display.h dòng 571-812 — Brew/Run screen
// Refactor: 2026-04-20

#ifndef UI_BREW_H
#define UI_BREW_H

#include "ui_common.h"

void uiRunStart();
void uiRunUpdate(const ShotData& s);
void uiRunDeinit();

#endif // UI_BREW_H
