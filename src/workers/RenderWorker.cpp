#include "RenderWorker.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <optional>


namespace CapScript {

static QString rlogHtml(const QString &msg, const QString &color = "#aaaaaa",
                        bool bold = false) {
  const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
  QString body = QStringLiteral("%1 - %2").arg(ts, msg.toHtmlEscaped());
  if (bold)
    body = QStringLiteral("<b>%1</b>").arg(body);
  return QStringLiteral("<p style='margin:0'><font color='%1'>%2</font></p>")
      .arg(color, body);
}


static double parseTimestamp(const QString &ts) {

  static QRegularExpression re(R"((\d+):(\d{2}):(\d{2})(?:\.(\d+))?)");
  const auto m = re.match(ts);
  if (!m.hasMatch())
    return -1.0;
  double secs = m.captured(1).toDouble() * 3600.0 +
                m.captured(2).toDouble() * 60.0 + m.captured(3).toDouble();
  if (!m.captured(4).isEmpty()) {

    const QString frac = m.captured(4);
    secs += frac.toDouble() / std::pow(10.0, frac.size());
  }
  return secs;
}

RenderWorker::RenderWorker(const QString &clipsFolder,
                           const QString &outputPath, const QString &ffmpegPath,
                           QObject *parent)
    : QObject(parent), m_clipsFolder(clipsFolder), m_outputPath(outputPath),
      m_ffmpegPath(ffmpegPath) {}

void RenderWorker::stop() {
  m_running.store(false);
  emit logOutput(rlogHtml("Stop requested.", "orange", true));
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->terminate();
    if (!m_process->waitForFinished(2000))
      m_process->kill();
  }
}

QString RenderWorker::ffprobePath() const {

  QFileInfo fi(m_ffmpegPath);
  QString probe = fi.dir().filePath(
      fi.baseName().replace("ffmpeg", "ffprobe", Qt::CaseInsensitive) +
      (fi.suffix().isEmpty() ? QString{} : '.' + fi.suffix()));
  return QFile::exists(probe) ? probe : "ffprobe";
}

std::optional<RenderWorker::ClipFormat>
RenderWorker::detectFormat(QStringList &outFiles) {
  QDir dir(m_clipsFolder);

  struct Candidate {
    const char *ext;
    ClipFormat fmt;
  };
  static constexpr Candidate candidates[] = {
      {"*.mp4", ClipFormat::Mp4},
      {"*.mkv", ClipFormat::Mkv},
      {"*.webm", ClipFormat::Webm},
  };

  ClipFormat detected{};
  QStringList detectedFiles;
  int formatsFound = 0;

  for (const auto &c : candidates) {
    QStringList found = dir.entryList({c.ext}, QDir::Files);
    if (!found.isEmpty()) {
      ++formatsFound;
      detected = c.fmt;
      detectedFiles = std::move(found);
    }
  }

  if (formatsFound == 0) {
    emit error(rlogHtml("No .mp4 / .mkv / .webm files found in clips folder.",
                        "#ff6666", true));
    emit finished(false, "No clips found.");
    return std::nullopt;
  }

  if (formatsFound > 1) {
    emit error(rlogHtml("Mixed clip formats detected. "
                        "All clips must share the same container format.",
                        "#ff6666", true));
    emit finished(false, "Mixed clip formats.");
    return std::nullopt;
  }

  static QRegularExpression tsRe(R"((\d{1,2})-(\d{2})-(\d{2}))");
  std::sort(detectedFiles.begin(), detectedFiles.end(),
            [&](const QString &a, const QString &b) {
              auto toSec = [&](const QString &name) -> int {
                const auto m = tsRe.match(name);
                if (!m.hasMatch())
                  return INT_MAX;
                return m.captured(1).toInt() * 3600 +
                       m.captured(2).toInt() * 60 + m.captured(3).toInt();
              };
              return toSec(a) < toSec(b);
            });

  outFiles = std::move(detectedFiles);
  return detected;
}

double RenderWorker::probeTotalDuration(const QStringList &absPaths) const {

  double total = 0.0;
  const QString probe = ffprobePath();

  for (const QString &path : absPaths) {
    QProcess p;
    p.setProgram(probe);
    p.setArguments({"-v", "error", "-show_entries", "format=duration", "-of",
                    "default=noprint_wrappers=1:nokey=1", path});
#ifdef _WIN32
    p.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *a) {
          a->flags |= 0x08000000;
        });
#endif
    p.start();
    if (!p.waitForFinished(8000)) {
      p.kill();
      return 0.0;
    }
    bool ok = false;
    double d = QString(p.readAllStandardOutput()).trimmed().toDouble(&ok);
    if (ok)
      total += d;
  }
  return total;
}

void RenderWorker::handleOutputLine(const QString &line, double totalSecs,
                                    int &lastPct) {

  emit logOutput(rlogHtml(line.trimmed(), "#777777"));

  if (totalSecs <= 0.0)
    return;

  static QRegularExpression timeRe(R"(\btime=(\S+))");
  const auto m = timeRe.match(line);
  if (!m.hasMatch())
    return;

  const double elapsed = parseTimestamp(m.captured(1));
  if (elapsed < 0.0)
    return;

  const int pct = static_cast<int>(10.0 + (elapsed / totalSecs) * 89.0);
  const int clamped = qBound(10, pct, 99);
  if (clamped > lastPct) {
    lastPct = clamped;
    emit progressUpdate(clamped);
  }
}

void RenderWorker::run() {
  emit logOutput(rlogHtml("Render worker started.", "#66ccff", true));
  const auto startTime = QDateTime::currentDateTime();

  QStringList files;
  const auto fmtOpt = detectFormat(files);
  if (!fmtOpt)
    return;

  const ClipFormat fmt = *fmtOpt;
  const QString fmtName = (fmt == ClipFormat::Mp4)   ? "mp4"
                          : (fmt == ClipFormat::Mkv) ? "mkv"
                                                     : "webm";

  emit logOutput(rlogHtml(
      QStringLiteral("Found %1 .%2 clip(s). Using stream-copy (no re-encode).")
          .arg(files.size())
          .arg(fmtName),
      "#66ccff"));

  if (!m_running.load()) {
    emit finished(false, "Cancelled");
    return;
  }

  const QDir clipsDir(m_clipsFolder);
  QStringList absPaths;
  absPaths.reserve(files.size());
  for (const QString &f : files)
    absPaths << clipsDir.absoluteFilePath(f);

  emit logOutput(rlogHtml("Probing clip durations…", "#acacac"));
  const double totalSecs = probeTotalDuration(absPaths);
  if (totalSecs > 0.0) {
    emit logOutput(rlogHtml(
        QStringLiteral("Total source duration: %1s.").arg(totalSecs, 0, 'f', 2),
        "#acacac"));
  } else {
    emit logOutput(rlogHtml(
        "Could not probe duration — progress bar will be indeterminate.",
        "#888888"));
  }

  if (!m_running.load()) {
    emit finished(false, "Cancelled");
    return;
  }

  const QString fileListPath =
      QFileInfo(m_outputPath).absolutePath() + "/ffmpeg_filelist.txt";
  {
    QFile f(fileListPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      emit error(rlogHtml("Cannot write concat file list.", "#ff6666", true));
      emit finished(false, "File list creation failed.");
      return;
    }
    QTextStream ts(&f);
    for (const QString &abs : absPaths) {

      QString safe = abs;
      safe.replace("'", "'\\''");
      ts << "file '" << safe << "'\n";
    }
  }

  if (!m_running.load()) {
    QFile::remove(fileListPath);
    emit finished(false, "Cancelled");
    return;
  }

  QStringList args = {
      "-f",      "concat",     "-safe", "0",
      "-i",      fileListPath, "-c",    "copy",
      "-fflags", "+genpts",
  };

  if (fmt == ClipFormat::Mp4)
    args << "-movflags" << "+faststart";

  args << "-y"
       << "-hide_banner"
       << "-loglevel" << "info"
       << m_outputPath;

  QProcess proc;
  m_process = &proc;
  proc.setProcessChannelMode(QProcess::MergedChannels);
  proc.setProgram(m_ffmpegPath);
  proc.setArguments(args);
#ifdef _WIN32
  proc.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) {
        a->flags |= 0x08000000;
      });
#endif

  emit logOutput(rlogHtml("Launching ffmpeg…", "#acacac", true));
  emit logOutput(
      rlogHtml("Command: " + m_ffmpegPath + " " + args.join(' '), "#555555"));
  emit progressUpdate(10);

  proc.start();
  if (!proc.waitForStarted(5000)) {
    m_process = nullptr;
    QFile::remove(fileListPath);
    emit error(rlogHtml("ffmpeg did not start within 5 s.", "#ff6666", true));
    emit finished(false, "ffmpeg launch failed.");
    return;
  }

  int lastPct = 10;
  QString leftover;

  while (!proc.waitForFinished(120)) {
    if (!m_running.load())
      proc.terminate();

    const QByteArray raw = proc.readAll();
    if (raw.isEmpty())
      continue;

    leftover += QString::fromUtf8(raw);
    int nl;
    while ((nl = leftover.indexOf('\n')) != -1) {
      const QString line = leftover.left(nl).trimmed();
      leftover = leftover.mid(nl + 1);
      if (!line.isEmpty())
        handleOutputLine(line, totalSecs, lastPct);
    }
  }

  {
    const QByteArray tail = proc.readAll();
    leftover += QString::fromUtf8(tail);
    const QStringList lines = leftover.split('\n', Qt::SkipEmptyParts);
    for (const QString &l : lines)
      if (!l.trimmed().isEmpty())
        handleOutputLine(l.trimmed(), totalSecs, lastPct);
  }

  m_process = nullptr;
  QFile::remove(fileListPath);

  const double elapsed =
      startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;

  if (!m_running.load()) {
    QFile::remove(m_outputPath);
    emit logOutput(rlogHtml(
        QStringLiteral("Render cancelled after %1 s.").arg(elapsed, 0, 'f', 2),
        "orange", true));
    emit finished(false, "Cancelled");
    return;
  }

  if (proc.exitCode() == 0) {
    emit progressUpdate(100);
    emit logOutput(rlogHtml(
        QStringLiteral("Render complete in %1 s.").arg(elapsed, 0, 'f', 2),
        "#90ee90", true));
    emit finished(true, m_outputPath);
  } else {
    const QString errText = QString(proc.readAllStandardError()).trimmed();
    emit error(rlogHtml(
        QStringLiteral("ffmpeg exited with code %1. %2")
            .arg(proc.exitCode())
            .arg(errText.isEmpty() ? QString{} : "Last stderr: " + errText),
        "#ff6666", true));
    emit finished(false, "ffmpeg error");
  }
}

}