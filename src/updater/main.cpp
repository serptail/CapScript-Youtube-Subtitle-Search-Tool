#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

namespace {

QFile gLogFile;

void logLine(const QString &message) {
  const QString line = QString("[%1] %2").arg(
      QDateTime::currentDateTime().toString(Qt::ISODate), message);

  QTextStream out(stdout);
  out << line << '\n';
  out.flush();

  if (gLogFile.isOpen()) {
    QTextStream fileOut(&gLogFile);
    fileOut << line << '\n';
    fileOut.flush();
  }
}

bool extractZip(const QString &zipPath, const QString &destination) {
  logLine(QString("Extracting zip: %1 -> %2").arg(zipPath, destination));
  QDir().mkpath(destination);

  auto runExtractor = [&](const QString &program, const QStringList &args,
                          const QString &label) {
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(5000)) {
      logLine(QString("Failed to start %1 process for extraction.").arg(label));
      return false;
    }

    proc.waitForFinished(-1);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
      logLine(QString("%1 extraction failed. ExitCode=%2")
                  .arg(label)
                  .arg(proc.exitCode()));
      const QString stdErr =
          QString::fromUtf8(proc.readAllStandardError()).trimmed();
      const QString stdOut =
          QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
      if (!stdErr.isEmpty())
        logLine(QString("%1 stderr: %2").arg(label, stdErr));
      if (!stdOut.isEmpty())
        logLine(QString("%1 stdout: %2").arg(label, stdOut));
      return false;
    }

    return true;
  };

#ifdef Q_OS_WIN
  if (!runExtractor("tar", {"-xf", zipPath, "-C", destination}, "Tar"))
    return false;
#else
  const QString unzipExe = QStandardPaths::findExecutable("unzip");
  if (!unzipExe.isEmpty()) {
    if (!runExtractor(unzipExe, {"-o", zipPath, "-d", destination}, "Unzip"))
      return false;
  } else {
    const QString bsdtarExe = QStandardPaths::findExecutable("bsdtar");
    if (!bsdtarExe.isEmpty()) {
      if (!runExtractor(bsdtarExe, {"-xf", zipPath, "-C", destination},
                        "bsdtar"))
        return false;
    } else {
      logLine("Neither unzip nor bsdtar found on system; cannot extract update "
              "zip.");
      return false;
    }
  }
#endif

  logLine("Zip extraction completed.");
  return true;
}

QString detectCopyRoot(const QString &extractedRoot) {
  QDir root(extractedRoot);
  const QFileInfoList entries =
      root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

  if (entries.size() == 1 && entries.first().isDir())
    return entries.first().absoluteFilePath();

  return extractedRoot;
}

bool shouldSkipCopiedFile(const QString &fileName) {
  static const QStringList skipNames = {
      QStringLiteral("capscript_updater.log"),
      QStringLiteral("capscript_update_launcher.log"),
      QStringLiteral("capscript_run_updater.cmd")};
  return skipNames.contains(fileName, Qt::CaseInsensitive);
}

bool copyRecursively(const QString &sourcePath, const QString &targetPath,
                     const QString &selfExePath) {
  logLine(QString("Copy phase: %1 -> %2").arg(sourcePath, targetPath));
  QDir sourceDir(sourcePath);
  if (!sourceDir.exists()) {
    logLine("Source directory does not exist during copy.");
    return false;
  }

  QDir targetDir(targetPath);
  targetDir.mkpath(".");

  const QFileInfoList entries =
      sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
  for (const QFileInfo &entry : entries) {
    const QString sourceEntry = entry.absoluteFilePath();
    const QString targetEntry = targetDir.filePath(entry.fileName());

    if (shouldSkipCopiedFile(entry.fileName())) {
      logLine(QString("Skipping volatile file: %1").arg(sourceEntry));
      continue;
    }

    if (QDir::toNativeSeparators(QFileInfo(targetEntry).absoluteFilePath())
            .compare(QDir::toNativeSeparators(
                         QFileInfo(selfExePath).absoluteFilePath()),
                     Qt::CaseInsensitive) == 0) {
      logLine(QString("Skipping currently running updater binary: %1")
                  .arg(targetEntry));
      continue;
    }

    if (entry.isDir()) {
      if (!copyRecursively(sourceEntry, targetEntry, selfExePath))
        return false;
      continue;
    }

    QDir().mkpath(QFileInfo(targetEntry).absolutePath());

    QFile::remove(targetEntry);
    if (!QFile::copy(sourceEntry, targetEntry)) {

      bool copied = false;
      for (int i = 0; i < 8; ++i) {
        QThread::msleep(350);
        QFile::remove(targetEntry);
        if (QFile::copy(sourceEntry, targetEntry)) {
          copied = true;
          break;
        }
      }
      if (!copied) {
        logLine(QString("Failed to copy file: %1 -> %2")
                    .arg(sourceEntry, targetEntry));
        return false;
      }
    }
  }

  return true;
}

bool waitUntilAppLikelyClosed(const QString &appPath) {
  logLine(QString("Waiting for app file unlock: %1").arg(appPath));
  QFile appFile(appPath);
  for (int i = 0; i < 120; ++i) {
    if (appFile.open(QIODevice::ReadWrite)) {
      appFile.close();
      logLine("Application file appears unlocked.");
      return true;
    }
    QThread::msleep(500);
  }
  logLine("Timed out waiting for app file unlock.");
  return false;
}

QString argumentValue(const QStringList &args, const QString &key) {
  const int i = args.indexOf(key);
  if (i < 0 || i + 1 >= args.size())
    return {};
  return args.at(i + 1);
}

}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("CapScript Updater");
  app.setApplicationVersion("2.0.0");

  const QStringList args = app.arguments();
  const QString zipPath = argumentValue(args, "--zip");
  const QString targetDir = argumentValue(args, "--target");
  const QString appPath = argumentValue(args, "--app");
  const bool relaunch = argumentValue(args, "--relaunch") == "1";

  if (!targetDir.isEmpty()) {
    gLogFile.setFileName(QDir(targetDir).filePath("capscript_updater.log"));
    if (!gLogFile.open(QIODevice::Append | QIODevice::Text)) {
      QTextStream out(stdout);
      out << "[" << QDateTime::currentDateTime().toString(Qt::ISODate)
          << "] Warning: unable to open updater log file in target directory."
          << '\n';
      out.flush();
    }
  }

  logLine("Updater bootstrapper started.");
  logLine(QString("Args: zip=%1 target=%2 app=%3 relaunch=%4")
              .arg(zipPath, targetDir, appPath, relaunch ? "1" : "0"));

  if (zipPath.isEmpty() || targetDir.isEmpty() || appPath.isEmpty()) {
    logLine("Missing required updater arguments.");
    return 2;
  }

  if (!QFileInfo::exists(zipPath) || !QDir(targetDir).exists()) {
    logLine("Zip file or target directory does not exist.");
    return 3;
  }

  waitUntilAppLikelyClosed(appPath);

  const QString extractDir = QDir::temp().filePath("capscript_update_extract");
  QDir(extractDir).removeRecursively();

  if (!extractZip(zipPath, extractDir))
    return 4;

  const QString copyRoot = detectCopyRoot(extractDir);
  const QString selfExePath = QCoreApplication::applicationFilePath();

  if (!copyRecursively(copyRoot, targetDir, selfExePath))
    return 5;

  if (!QFile::remove(zipPath))
    logLine(QString("Warning: failed to delete staged zip: %1").arg(zipPath));
  if (!QDir(extractDir).removeRecursively())
    logLine(
        QString("Warning: failed to clean extract dir: %1").arg(extractDir));

  if (relaunch) {
    if (!QProcess::startDetached(appPath, {},
                                 QFileInfo(appPath).absolutePath()))
      logLine("Warning: failed to relaunch app.");
  }

  logLine("Updater completed successfully.");

  return 0;
}
