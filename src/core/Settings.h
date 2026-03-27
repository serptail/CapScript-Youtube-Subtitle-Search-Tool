#pragma once
#ifndef CAPSCRIPT_SETTINGS_H
#define CAPSCRIPT_SETTINGS_H

#include <QSettings>
#include <QString>

namespace CapScript {

class Settings {
public:
  static constexpr const char *THEME = "Appearance/Theme";
  static constexpr const char *SIDEBAR_COLLAPSED = "UI/SidebarCollapsed";
  static constexpr const char *LAST_OUTPUT_DIR = "Paths/LastOutputDir";
  static constexpr const char *WINDOW_GEOMETRY = "Window/Geometry";
  static constexpr const char *WINDOW_STATE = "Window/State";
  static constexpr const char *SPLITTER_STATE = "Viewer/SplitterState";
  static constexpr const char *GEOMETRY_VERSION_KEY = "Window/GeometryVersion";
  static constexpr int GEOMETRY_VERSION = 3;

  static QSettings &get() {
    static QSettings s(QStringLiteral(ORG_NAME), QStringLiteral(APP_NAME));
    return s;
  }

  static QString theme() { return get().value(THEME, "light").toString(); }
  static void setTheme(const QString &t) { get().setValue(THEME, t); }

  static bool sidebarCollapsed() {
    return get().value(SIDEBAR_COLLAPSED, false).toBool();
  }
  static void setSidebarCollapsed(bool c) {
    get().setValue(SIDEBAR_COLLAPSED, c);
  }

  static QString lastOutputDir() {
    return get().value(LAST_OUTPUT_DIR, "transcripts").toString();
  }
  static void setLastOutputDir(const QString &d) {
    get().setValue(LAST_OUTPUT_DIR, d);
  }

  static QByteArray windowGeometry() {
    return get().value(WINDOW_GEOMETRY).toByteArray();
  }
  static void setWindowGeometry(const QByteArray &g) {
    get().setValue(WINDOW_GEOMETRY, g);
  }

  static QByteArray windowState() {
    return get().value(WINDOW_STATE).toByteArray();
  }
  static void setWindowState(const QByteArray &s) {
    get().setValue(WINDOW_STATE, s);
  }
};

}

#endif
