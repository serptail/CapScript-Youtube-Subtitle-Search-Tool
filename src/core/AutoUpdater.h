#pragma once
#ifndef CAPSCRIPT_AUTOUPDATER_H
#define CAPSCRIPT_AUTOUPDATER_H

#include <QJsonArray>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace CapScript {

class AutoUpdater : public QObject {
  Q_OBJECT

public:
  explicit AutoUpdater(QObject *parent = nullptr);

  void checkForUpdates(bool silentNoUpdate = false);
  void downloadLatestRelease();

  bool stagePendingUpdate(const QString &zipPath, const QString &version);
  bool applyPendingUpdate(bool relaunchAfterUpdate);

  bool hasPendingUpdate() const;

  QString latestVersionTag() const { return m_latestTag; }
  QString latestReleaseNotes() const { return m_latestNotes; }

signals:
  void statusChanged(const QString &status);
  void updateAvailable(const QString &latestVersion,
                       const QString &releaseNotes);
  void noUpdateAvailable(const QString &message);
  void updateError(const QString &message);
  void downloadProgress(int percent);
  void downloadFinished(const QString &zipPath, const QString &version);

private:
  struct AssetInfo {
    QString name;
    QString downloadUrl;
  };

  static QString normalizeVersion(const QString &versionText);
  static bool isVersionNewer(const QString &candidate, const QString &current);

  QString bootstrapperPath() const;
  QString appExecutablePath() const;
  QString updateStagingPath(const QString &tag) const;

  AssetInfo pickBestAsset(const QJsonArray &assets) const;
  void handleReleaseReply(QNetworkReply *reply);

  QNetworkAccessManager *m_net = nullptr;
  QNetworkReply *m_downloadReply = nullptr;
  QString m_downloadPath;
  QString m_latestTag;
  QString m_latestNotes;
  QString m_latestAssetUrl;
  bool m_silentNoUpdate = false;
};

}

#endif
