#pragma once
#ifndef CAPSCRIPT_TOOLPATHS_H
#define CAPSCRIPT_TOOLPATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

namespace CapScript {

class ToolPaths {
public:
  static QString ytdlp() { return findTool(QStringLiteral("yt-dlp")); }

  static QString ffmpeg() { return findTool(QStringLiteral("ffmpeg")); }

  static QString findTool(const QString &name) {
    QString exeName = name + QStringLiteral(".exe");

    QString appDir = QCoreApplication::applicationDirPath();
    QStringList localPaths = {
        appDir + QStringLiteral("/bin/") + exeName,
        appDir + QStringLiteral("/") + exeName,
    };

    for (const QString &path : localPaths) {
      QFileInfo fi(path);
      if (fi.exists() && fi.isExecutable())
        return fi.absoluteFilePath();
    }

    QString found = QStandardPaths::findExecutable(name);
    if (!found.isEmpty())
      return found;

    return {};
  }

  static bool allAvailable() {
    return !ytdlp().isEmpty() && !ffmpeg().isEmpty();
  }
};

}

#endif
