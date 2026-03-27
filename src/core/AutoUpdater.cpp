#include "AutoUpdater.h"
#include "Settings.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVersionNumber>

namespace {
constexpr const char *kRepoOwner = "serptail";
constexpr const char *kRepoName = "CapScript-Test";

constexpr const char *kPendingUpdateZipKey = "Updates/PendingZip";
constexpr const char *kPendingUpdateVersionKey = "Updates/PendingVersion";

QString githubLatestReleaseApi() {
  return QString("https://api.github.com/repos/%1/%2/releases/latest")
      .arg(QString::fromUtf8(kRepoOwner), QString::fromUtf8(kRepoName));
}

void appendLauncherLog(const QString &message) {
  const QString logPath = QDir(QCoreApplication::applicationDirPath())
                              .filePath("capscript_update_launcher.log");

  QFile logFile(logPath);
  if (!logFile.open(QIODevice::Append | QIODevice::Text))
    return;

  QTextStream out(&logFile);
  out << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] "
      << message << '\n';
}

QString prepareTempBootstrapperRuntime(const QString &bootstrapperPath) {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString tempRoot =
      QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
          .filePath("capscript_updater_runtime");

  QDir(tempRoot).removeRecursively();
  QDir().mkpath(tempRoot);

  const QString exeName = QFileInfo(bootstrapperPath).fileName();
  const QStringList runtimeFiles = {exeName, QStringLiteral("Qt6Core.dll"),
                                    QStringLiteral("libgcc_s_seh-1.dll"),
                                    QStringLiteral("libstdc++-6.dll"),
                                    QStringLiteral("libwinpthread-1.dll")};

  for (const QString &fileName : runtimeFiles) {
    const QString src = QDir(appDir).filePath(fileName);
    if (!QFileInfo::exists(src)) {
      if (fileName.compare(exeName, Qt::CaseInsensitive) == 0)
        return {};
      continue;
    }

    const QString dst = QDir(tempRoot).filePath(fileName);
    QFile::remove(dst);
    if (!QFile::copy(src, dst)) {
      appendLauncherLog(
          QString("Failed copying updater runtime file to temp: %1")
              .arg(fileName));
      if (fileName.compare(exeName, Qt::CaseInsensitive) == 0)
        return {};
    }
  }

  return QDir(tempRoot).filePath(exeName);
}

QString normalisedPlatformToken() { return QStringLiteral("win"); }
}

namespace CapScript {

AutoUpdater::AutoUpdater(QObject *parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

QString AutoUpdater::normalizeVersion(const QString &versionText) {
  QString cleaned = versionText.trimmed();
  if (cleaned.startsWith('v', Qt::CaseInsensitive))
    cleaned.remove(0, 1);

  QRegularExpression re("(\\d+(?:\\.\\d+)+)");
  const auto match = re.match(cleaned);
  return match.hasMatch() ? match.captured(1) : cleaned;
}

bool AutoUpdater::isVersionNewer(const QString &candidate,
                                 const QString &current) {
  const QString candidateNorm = normalizeVersion(candidate);
  const QString currentNorm = normalizeVersion(current);

  QVersionNumber candidateVersion = QVersionNumber::fromString(candidateNorm);
  QVersionNumber currentVersion = QVersionNumber::fromString(currentNorm);

  if (candidateVersion.isNull() || currentVersion.isNull())
    return candidateNorm > currentNorm;

  return QVersionNumber::compare(candidateVersion, currentVersion) > 0;
}

AutoUpdater::AssetInfo
AutoUpdater::pickBestAsset(const QJsonArray &assets) const {
  const QString platform = normalisedPlatformToken();

  AssetInfo firstZip;

  for (const auto &value : assets) {
    const QJsonObject asset = value.toObject();
    const QString name = asset.value("name").toString();
    const QString url = asset.value("browser_download_url").toString();
    const QString lowerName = name.toLower();

    if (!lowerName.endsWith(".zip") || url.isEmpty())
      continue;

    if (firstZip.name.isEmpty()) {
      firstZip.name = name;
      firstZip.downloadUrl = url;
    }

    const bool looksPlatformSpecific =
        lowerName.contains(platform) || lowerName.contains("windows");

    if (looksPlatformSpecific && lowerName.contains(platform))
      return {name, url};
  }

  return firstZip;
}

void AutoUpdater::checkForUpdates(bool silentNoUpdate) {
  m_silentNoUpdate = silentNoUpdate;
  emit statusChanged("Checking for updates...");

  QNetworkRequest request{QUrl(githubLatestReleaseApi())};
  request.setHeader(QNetworkRequest::UserAgentHeader, "CapScriptPro-Updater");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply *reply = m_net->get(request);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { handleReleaseReply(reply); });
}

void AutoUpdater::handleReleaseReply(QNetworkReply *reply) {
  const QByteArray payload = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    emit updateError("Failed to check releases: " + reply->errorString());
    reply->deleteLater();
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(payload);
  if (!doc.isObject()) {
    emit updateError("Release API response was invalid.");
    reply->deleteLater();
    return;
  }

  const QJsonObject release = doc.object();
  m_latestTag = release.value("tag_name").toString();
  m_latestNotes = release.value("body").toString();

  const AssetInfo asset = pickBestAsset(release.value("assets").toArray());
  m_latestAssetUrl = asset.downloadUrl;

  const QString currentVersion =
      qApp ? qApp->applicationVersion() : QStringLiteral("0.0.0");

  if (m_latestTag.isEmpty()) {
    emit updateError("Release information did not include a tag.");
    reply->deleteLater();
    return;
  }

  if (!isVersionNewer(m_latestTag, currentVersion)) {
    const QString msg = QString("You are already on the latest version (%1).")
                            .arg(currentVersion);
    emit statusChanged(msg);
    if (!m_silentNoUpdate)
      emit noUpdateAvailable(msg);
    reply->deleteLater();
    return;
  }

  if (m_latestAssetUrl.isEmpty()) {
    emit updateError("A newer release exists, but no .zip asset was found for "
                     "this platform.");
    reply->deleteLater();
    return;
  }

  emit updateAvailable(m_latestTag, m_latestNotes);
  reply->deleteLater();
}

QString AutoUpdater::updateStagingPath(const QString &tag) const {
  const QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(appDataDir);
  dir.mkpath("updates");
  dir.mkpath("updates/" + tag);
  return dir.filePath("updates/" + tag + "/capscript_update.zip");
}

void AutoUpdater::downloadLatestRelease() {
  if (m_latestAssetUrl.isEmpty()) {
    emit updateError("No downloadable update artifact selected.");
    return;
  }

  if (m_downloadReply) {
    emit updateError("An update download is already in progress.");
    return;
  }

  m_downloadPath = updateStagingPath(normalizeVersion(m_latestTag));
  QDir().mkpath(QFileInfo(m_downloadPath).absolutePath());
  QFile::remove(m_downloadPath);

  emit statusChanged(QString("Downloading update %1...").arg(m_latestTag));

  QNetworkRequest request{QUrl(m_latestAssetUrl)};
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setHeader(QNetworkRequest::UserAgentHeader, "CapScriptPro-Updater");

  m_downloadReply = m_net->get(request);

  connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
          [this](qint64 received, qint64 total) {
            if (total <= 0) {
              emit downloadProgress(0);
              return;
            }
            const int percent = static_cast<int>((received * 100) / total);
            emit downloadProgress(percent);
          });

  connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
      emit updateError("Failed to download update: " + reply->errorString());
      reply->deleteLater();
      return;
    }

    QFile file(m_downloadPath);
    if (!file.open(QIODevice::WriteOnly)) {
      emit updateError("Unable to write update package to disk.");
      reply->deleteLater();
      return;
    }

    file.write(reply->readAll());
    file.close();

    emit statusChanged("Update downloaded. Ready to apply.");
    emit downloadProgress(100);
    emit downloadFinished(m_downloadPath, m_latestTag);

    reply->deleteLater();
  });
}

bool AutoUpdater::stagePendingUpdate(const QString &zipPath,
                                     const QString &version) {
  if (!QFileInfo::exists(zipPath))
    return false;

  Settings::get().setValue(kPendingUpdateZipKey, zipPath);
  Settings::get().setValue(kPendingUpdateVersionKey, version);
  return true;
}

bool AutoUpdater::hasPendingUpdate() const {
  const QString pendingZip =
      Settings::get().value(kPendingUpdateZipKey).toString();
  return !pendingZip.isEmpty() && QFileInfo::exists(pendingZip);
}

QString AutoUpdater::appExecutablePath() const {
  return QCoreApplication::applicationFilePath();
}

QString AutoUpdater::bootstrapperPath() const {
  const QString dir = QCoreApplication::applicationDirPath();
  return QDir(dir).filePath("CapScriptUpdater.exe");
}

bool AutoUpdater::applyPendingUpdate(bool relaunchAfterUpdate) {
  appendLauncherLog(QString("applyPendingUpdate called. relaunch=%1")
                        .arg(relaunchAfterUpdate ? "1" : "0"));

  const QString pendingZip =
      Settings::get().value(kPendingUpdateZipKey).toString();
  if (pendingZip.isEmpty() || !QFileInfo::exists(pendingZip)) {
    appendLauncherLog(
        QString("No pending zip or file missing: %1").arg(pendingZip));
    Settings::get().remove(kPendingUpdateZipKey);
    Settings::get().remove(kPendingUpdateVersionKey);
    return false;
  }

  appendLauncherLog(QString("Pending zip found: %1").arg(pendingZip));

  const QString bootstrapper = bootstrapperPath();
  if (!QFileInfo::exists(bootstrapper)) {
    appendLauncherLog(QString("Bootstrapper missing: %1").arg(bootstrapper));
    emit updateError(
        "Updater bootstrapper is missing in the application directory.");
    return false;
  }

  appendLauncherLog(QString("Bootstrapper path: %1").arg(bootstrapper));

  const QStringList args = {
      "--zip",      pendingZip,
      "--target",   QCoreApplication::applicationDirPath(),
      "--app",      appExecutablePath(),
      "--relaunch", relaunchAfterUpdate ? "1" : "0"};

  appendLauncherLog(QString("Launch args: %1").arg(args.join(" | ")));

  bool started = false;
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString runtimeBootstrapper =
      prepareTempBootstrapperRuntime(bootstrapper);
  if (runtimeBootstrapper.isEmpty()) {
    appendLauncherLog("Failed to prepare temp updater runtime.");
    emit updateError("Failed to prepare updater runtime files.");
    return false;
  }
  appendLauncherLog(
      QString("Runtime bootstrapper path: %1").arg(runtimeBootstrapper));

  const QString scriptPath = QDir(appDir).filePath("capscript_run_updater.cmd");
  QFile script(scriptPath);
  if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream s(&script);
    s << "@echo off\r\n";
    s << "echo [CapScript] Launching updater...\r\n";
    s << "\"" << runtimeBootstrapper << "\"";
    for (const QString &arg : args)
      s << " \"" << arg << "\"";
    s << "\r\n";
    s << "set EXIT_CODE=%ERRORLEVEL%\r\n";
    s << "echo [CapScript] Updater exited with code %EXIT_CODE%\r\n";
    s << "echo.\r\n";
    s << "pause\r\n";
    script.close();

    appendLauncherLog(QString("Windows launcher script: %1").arg(scriptPath));
    started = QProcess::startDetached("cmd.exe", {"/k", scriptPath}, appDir);
  } else {
    appendLauncherLog(
        QString("Failed to write launcher script: %1").arg(scriptPath));
  }

  appendLauncherLog(
      QString("startDetached result: %1").arg(started ? "true" : "false"));

  if (!started) {
    emit updateError("Failed to launch updater bootstrapper.");
    return false;
  }

  Settings::get().remove(kPendingUpdateZipKey);
  Settings::get().remove(kPendingUpdateVersionKey);
  return true;
}

}
