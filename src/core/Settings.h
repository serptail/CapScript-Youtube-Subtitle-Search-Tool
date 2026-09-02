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
  static constexpr const char *LAST_COOKIES_FILE = "Paths/LastCookiesFile";
  static constexpr const char *LAST_COOKIES_BROWSER = "Paths/LastCookiesBrowser";
    static constexpr const char *YTDLP_VERSION_SAVED = "Tools/YtDlpVersionSaved";
    static constexpr const char *YTDLP_LAST_CHECKED_LATEST =
      "Tools/YtDlpLastCheckedLatest";
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

  static QString lastCookiesFile() {
    return get().value(LAST_COOKIES_FILE, "").toString();
  }
  static void setLastCookiesFile(const QString &path) {
    get().setValue(LAST_COOKIES_FILE, path);
  }

  static QString lastCookiesBrowser() {
    return get().value(LAST_COOKIES_BROWSER, "none").toString();
  }
  static void setLastCookiesBrowser(const QString &browser) {
    get().setValue(LAST_COOKIES_BROWSER, browser.trimmed().toLower());
  }

  static QString ytDlpVersionSaved() {
    return get().value(YTDLP_VERSION_SAVED, "").toString().trimmed();
  }
  static void setYtDlpVersionSaved(const QString &version) {
    get().setValue(YTDLP_VERSION_SAVED, version.trimmed());
  }

  static QString ytDlpLastCheckedLatest() {
    return get().value(YTDLP_LAST_CHECKED_LATEST, "").toString().trimmed();
  }
  static void setYtDlpLastCheckedLatest(const QString &version) {
    get().setValue(YTDLP_LAST_CHECKED_LATEST, version.trimmed());
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
