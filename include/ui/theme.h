#pragma once

class QApplication;

// Dark Fusion theme matching Monaco's "vs-dark".
// Skipped when COSBY_SYSTEM_THEME=1 is set (keeps the desktop's style).
void apply_app_theme(QApplication& app);
