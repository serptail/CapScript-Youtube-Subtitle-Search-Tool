#include "ClipWorker.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QProcess>
#include <QThread>
#include <QtConcurrent>


namespace CapScript {

static void
drainProcessLogs(QProcess &proc, QString &pending,
                 const std::function<void(const QString &)> &emitLine) {
  pending += QString::fromLocal8Bit(proc.readAllStandardOutput());
  if (proc.processChannelMode() != QProcess::MergedChannels)
    pending += QString::fromLocal8Bit(proc.readAllStandardError());

  pending.replace('\r', '\n');
  QStringList parts = pending.split('\n');
  if (parts.isEmpty())
    return;

  pending = parts.takeLast();
  for (const QString &raw : parts) {
    const QString line = raw.trimmed();
    if (!line.isEmpty())
      emitLine(line);
  }
}

static QString formatHHMMSS(int totalSeconds) {
  int h = totalSeconds / 3600;
  int m = (totalSeconds % 3600) / 60;
  int s = totalSeconds % 60;
  return QStringLiteral("%1:%2:%3")
      .arg(h, 2, 10, QLatin1Char('0'))
      .arg(m, 2, 10, QLatin1Char('0'))
      .arg(s, 2, 10, QLatin1Char('0'));
}

static QString logHtml(const QString &msg, const QString &color = "#aaaaaa",
                       bool bold = false) {
  const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
  QString body = QStringLiteral("%1 - %2").arg(ts, msg.toHtmlEscaped());
  if (bold)
    body = QStringLiteral("<b>%1</b>").arg(body);
  return QStringLiteral("<p style='margin:0'><font color='%1'>%2</font></p>")
      .arg(color, body);
}

static void killProcessTree(QProcess *proc) {
  if (!proc || proc->state() == QProcess::NotRunning)
    return;
#ifdef _WIN32
  qint64 pid = proc->processId();
  if (pid > 0)
    QProcess::execute("taskkill", {"/F", "/T", "/PID", QString::number(pid)});
#else
  proc->terminate();
  if (!proc->waitForFinished(2000))
    proc->kill();
#endif
}

static int runProcess(QProcess &proc, const QString &key,
                      QMap<QString, QProcess *> &activeMap, QMutex &mapMutex,
                      QMutex &logMutex, std::atomic<bool> &running,
                      QString &processLog,
                      const std::function<void(const QString &)> &logFn) {
  {
    QMutexLocker lk(&mapMutex);
    if (!running.load())
      return -1;
    activeMap[key] = &proc;
  }

  proc.start();

  QString pending;
  while (!proc.waitForFinished(100)) {
    drainProcessLogs(proc, pending, [&](const QString &line) {
      processLog += line + '\n';
      QMutexLocker lk(&logMutex);
      logFn(line);
    });
    QMutexLocker lk(&mapMutex);
    if (!running.load()) {
      killProcessTree(&proc);
      break;
    }
  }

  drainProcessLogs(proc, pending, [&](const QString &line) {
    processLog += line + '\n';
    QMutexLocker lk(&logMutex);
    logFn(line);
  });
  if (!pending.trimmed().isEmpty()) {
    const QString line = pending.trimmed();
    processLog += line + '\n';
    QMutexLocker lk(&logMutex);
    logFn(line);
  }

  {
    QMutexLocker lk(&mapMutex);
    activeMap.remove(key);
  }

  if (!running.load())
    return -1;
  return proc.exitCode();
}

ClipWorker::ClipWorker(const QList<TimestampEntry> &entries,
                       const QString &outputDir, int clipDuration,
                       const QString &ytdlpPath, const QString &ffmpegPath,
                       Quality quality, Format format, int maxConcurrent,
                       int maxRetries, QObject *parent)
    : QObject(parent), m_entries(entries), m_outputDir(outputDir),
      m_clipDuration(clipDuration), m_ytdlpPath(ytdlpPath),
      m_ffmpegPath(ffmpegPath), m_quality(quality), m_format(format),
      m_maxConcurrent(qBound(1, maxConcurrent, 8)),
      m_maxRetries(qBound(1, maxRetries, 10)) {}

void ClipWorker::stop() {
  m_running.store(false);
  emit logOutput(logHtml("Stop request received.", "orange", true));
  QMutexLocker lock(&m_mutex);
  for (auto *proc : m_activeProcesses)
    killProcessTree(proc);
  m_activeProcesses.clear();
}

QString ClipWorker::buildFormatSelector() const {
  if (m_format == Format::MP3)
    return "bestaudio[protocol=m3u8_native]/bestaudio";

  switch (m_quality) {
  case Quality::Best:
    return "bestvideo[protocol=m3u8_native]+bestaudio[protocol=m3u8_native]"
           "/best[protocol=m3u8_native]";
  case Quality::Q720p:
    return "bestvideo[protocol=m3u8_native][height<=720]+bestaudio[protocol="
           "m3u8_native]"
           "/best[protocol=m3u8_native][height<=720]";
  case Quality::Q480p:
    return "bestvideo[protocol=m3u8_native][height<=480]+bestaudio[protocol="
           "m3u8_native]"
           "/best[protocol=m3u8_native][height<=480]";
  case Quality::Q360p:
    return "bestvideo[protocol=m3u8_native][height<=360]+bestaudio[protocol="
           "m3u8_native]"
           "/best[protocol=m3u8_native][height<=360]";
  case Quality::AudioOnly:
    return "bestaudio[protocol=m3u8_native]/bestaudio";
  }
  return "bestvideo[protocol=m3u8_native]+bestaudio[protocol=m3u8_native]"
         "/best[protocol=m3u8_native]";
}

QString ClipWorker::getFormatExtension() const {
  switch (m_format) {
  case Format::MP4:
    return "mp4";
  case Format::MKV:
    return "mkv";
  case Format::WebM:
    return "webm";
  case Format::MP3:
    return "mp3";
  }
  return "mp4";
}

QString ClipWorker::getMergeFormat() const {
  return (m_format == Format::MP3) ? "mp3" : "mp4";
}

QString ClipWorker::buildYtdlpPostprocessorArgs() const {

  return "ffmpeg:-c:v libx264 -preset fast -crf 23 -c:a aac -b:a 192k -nostdin "
         "-y";
}

QStringList ClipWorker::buildFfmpegTrimArgs(const QString &inputFile,
                                            const QString &outputFile,
                                            int startOffsetSec) const {
  const QString dur = QString::number(m_clipDuration);
  const QString offset = QString::number(startOffsetSec);

  if (m_format == Format::MP3) {
    return {"-y",           "-nostdin", "-i",   inputFile,   "-ss",
            offset,         "-t",       dur,    "-vn",       "-acodec",
            "libmp3lame",   "-b:a",     "192k", "-loglevel", "warning",
            "-hide_banner", outputFile};
  }

  if (m_format == Format::WebM) {

    return {"-y",       "-nostdin",  "-ss",       offset,    "-i",
            inputFile,  "-t",        dur,         "-c:v",    "libvpx-vp9",
            "-b:v",     "0",         "-crf",      "33",      "-deadline",
            "realtime", "-cpu-used", "8",         "-c:a",    "libvorbis",
            "-b:a",     "192k",      "-loglevel", "warning", "-hide_banner",
            outputFile};
  }

  return {"-y",        "-nostdin", "-i",           inputFile,   "-ss",
          offset,
          "-t",        dur,
          "-c:v",      "libx264",  "-preset",      "ultrafast", "-crf",
          "18",        "-c:a",     "aac",          "-b:a",      "192k",
          "-loglevel", "warning",  "-hide_banner", outputFile};
}

bool ClipWorker::downloadClip(const ClipTask &task) {
  if (!m_running.load())
    return false;
  if (m_format == Format::MP3)
    return downloadClipMp3(task);

  const QString clipsDir = m_outputDir + "/clips";
  QDir().mkpath(clipsDir);

  const QString url = "https://youtu.be/" + task.videoId;
  const QString startStr = formatHHMMSS(task.startSeconds);
  const QString endStr = formatHHMMSS(task.startSeconds + m_clipDuration);

  constexpr int kSegmentPad = 6;
  const int paddedStartSec = qMax(0, task.startSeconds - kSegmentPad);
  const int actualPad =
      task.startSeconds - paddedStartSec;
  const QString paddedStart = formatHHMMSS(paddedStartSec);
  const QString section = QStringLiteral("*%1-%2").arg(paddedStart, endStr);

  const QString safeStart = QString(startStr).replace(':', '-');
  const QString safeEnd = QString(endStr).replace(':', '-');
  const QString ext = getFormatExtension();

  const QString finalFile =
      QStringLiteral("%1/%2_%3-%4.%5")
          .arg(clipsDir, task.videoId, safeStart, safeEnd, ext);

  const QString rawStem =
      QStringLiteral("%1/.tmp_%2_%3")
          .arg(clipsDir, task.videoId, QString::number(task.startSeconds));

  const QString rawFile = (m_format == Format::MP3)
                              ? rawStem + ".%(ext)s"
                              : rawStem + ".mp4";

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(QStringLiteral("[Thread %1] %2  %3 → %4")
                               .arg((quintptr)QThread::currentThreadId())
                               .arg(task.videoId, startStr, endStr),
                           "#66ccff"));
  }

  QStringList ytArgs = {
      url,
      "-f",
      buildFormatSelector(),
      "--download-sections",
      section,
      "-o",
      rawFile,
      "--no-warnings",
      "--newline",
      "--progress",
      "--socket-timeout",
      "30",
      "--retries",
      "3",
      "--fragment-retries",
      "3",
  };

  if (m_format != Format::MP3) {
    ytArgs << "--merge-output-format" << getMergeFormat();
    ytArgs << "--postprocessor-args" << buildYtdlpPostprocessorArgs();
  } else {
    ytArgs << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "0";
  }

  if (!m_ffmpegPath.isEmpty())
    ytArgs << "--ffmpeg-location" << QFileInfo(m_ffmpegPath).absolutePath();

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(
        QStringLiteral("[Pass 1] %1 %2").arg(m_ytdlpPath, ytArgs.join(' ')),
        "#444444"));
  }

  QProcess ytdlp;
  ytdlp.setProcessChannelMode(QProcess::MergedChannels);
  ytdlp.setStandardInputFile(QProcess::nullDevice());
  ytdlp.setProgram(m_ytdlpPath);
  ytdlp.setArguments(ytArgs);
#ifdef _WIN32
  ytdlp.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif

  QString ytLog;
  const int ytExit = runProcess(
      ytdlp,
      QStringLiteral("yt_%1_%2").arg(task.videoId).arg(task.startSeconds),
      m_activeProcesses, m_mutex, m_logMutex, m_running, ytLog,
      [this](const QString &line) {
        emit logOutput(logHtml(line, "#555555"));
      });

  if (ytExit == -1) {

    if (m_format == Format::MP3) {
      for (const QString &f : QDir(clipsDir).entryList(
               {QStringLiteral(".tmp_%1_%2.*")
                    .arg(task.videoId, QString::number(task.startSeconds))},
               QDir::Files))
        QFile::remove(clipsDir + "/" + f);
    } else {
      QFile::remove(rawStem + ".mp4");
    }
    return false;
  }

  const QString resolvedRawFile =
      (m_format == Format::MP3) ? rawStem + ".mp3" : rawStem + ".mp4";

  if (ytExit != 0) {
    const bool networkErr =
        ytLog.contains("Error opening input") ||
        ytLog.contains("Connection reset") || ytLog.contains("timed out") ||
        ytLog.contains("Error number -138") ||
        ytLog.contains("HTTP Error 503") || ytLog.contains("HTTP Error 429");

    QString mainErr = "exit code " + QString::number(ytExit);
    for (const QString &l : ytLog.split('\n'))
      if (l.contains("ERROR:")) {
        mainErr = l.trimmed();
        break;
      }

    QMutexLocker lock(&m_logMutex);
    emit logOutput(
        logHtml(QStringLiteral("yt-dlp failed: %1 (%2)%3")
                    .arg(task.videoId, mainErr,
                         networkErr ? " [network — will retry]" : ""),
                "#ff6666"));
    QFile::remove(resolvedRawFile);
    return false;
  }

  if (!QFile::exists(resolvedRawFile)) {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(
        logHtml(QStringLiteral("Pass 1 produced no output file for %1 — "
                               "yt-dlp may have saved under unexpected name")
                    .arg(task.videoId),
                "#ff6666"));
    return false;
  }

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(QStringLiteral("[Pass 2] Trimming to %1s → %2")
                               .arg(m_clipDuration)
                               .arg(QFileInfo(finalFile).fileName()),
                           "#66ccff"));
  }

  const QStringList ffArgs =
      buildFfmpegTrimArgs(resolvedRawFile, finalFile, actualPad);

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(
        QStringLiteral("[Pass 2] %1 %2").arg(m_ffmpegPath, ffArgs.join(' ')),
        "#444444"));
  }

  QProcess ffmpeg;
  ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
  ffmpeg.setStandardInputFile(QProcess::nullDevice());
  ffmpeg.setProgram(m_ffmpegPath);
  ffmpeg.setArguments(ffArgs);
#ifdef _WIN32
  ffmpeg.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif

  QString ffLog;
  const int ffExit = runProcess(
      ffmpeg,
      QStringLiteral("ff_%1_%2").arg(task.videoId).arg(task.startSeconds),
      m_activeProcesses, m_mutex, m_logMutex, m_running, ffLog,
      [this](const QString &line) {
        emit logOutput(logHtml(line, "#555555"));
      });

  QFile::remove(resolvedRawFile);

  if (ffExit == -1)
    return false;

  if (ffExit != 0) {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(QStringLiteral("ffmpeg trim failed for %1: %2")
                               .arg(task.videoId, ffLog.trimmed()),
                           "#ff6666"));
    return false;
  }

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(
        QStringLiteral("Saved: %1").arg(QFileInfo(finalFile).fileName()),
        "#90ee90"));
  }
  return true;
}

bool ClipWorker::downloadClipMp3(const ClipTask &task) {
  if (!m_running.load())
    return false;

  const QString clipsDir = m_outputDir + "/clips";
  QDir().mkpath(clipsDir);

  const QString url = "https://youtu.be/" + task.videoId;
  const QString startStr = formatHHMMSS(task.startSeconds);
  const QString endStr = formatHHMMSS(task.startSeconds + m_clipDuration);
  const QString safeStart = QString(startStr).replace(':', '-');
  const QString safeEnd = QString(endStr).replace(':', '-');

  const QString finalFile =
      QStringLiteral("%1/%2_%3-%4.mp3")
          .arg(clipsDir, task.videoId, safeStart, safeEnd);

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(QStringLiteral("[Thread %1] %2  %3 → %4  (MP3)")
                               .arg((quintptr)QThread::currentThreadId())
                               .arg(task.videoId, startStr, endStr),
                           "#66ccff"));
  }

  const QStringList ytArgs = {
      url,
      "-f",
      "bestaudio",
      "-g",
      "--no-warnings",
      "--socket-timeout",
      "30",
      "--retries",
      "3",
  };

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(
        QStringLiteral("[Pass 1/MP3] %1 %2").arg(m_ytdlpPath, ytArgs.join(' ')),
        "#444444"));
  }

  QProcess ytdlp;
  ytdlp.setProcessChannelMode(QProcess::MergedChannels);
  ytdlp.setStandardInputFile(QProcess::nullDevice());
  ytdlp.setProgram(m_ytdlpPath);
  ytdlp.setArguments(ytArgs);
#ifdef _WIN32
  ytdlp.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif

  QString ytLog;
  const int ytExit = runProcess(
      ytdlp,
      QStringLiteral("yt_%1_%2").arg(task.videoId).arg(task.startSeconds),
      m_activeProcesses, m_mutex, m_logMutex, m_running, ytLog,
      [this](const QString &line) {
        emit logOutput(logHtml(line, "#555555"));
      });

  if (ytExit == -1)
    return false;

  if (ytExit != 0) {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(
        QStringLiteral("yt-dlp URL resolution failed for %1").arg(task.videoId),
        "#ff6666"));
    return false;
  }

  QString streamUrl;
  for (const QString &line : ytLog.split('\n')) {
    const QString t = line.trimmed();
    if (!t.isEmpty())
      streamUrl = t;
  }

  if (streamUrl.isEmpty() || !streamUrl.startsWith("http")) {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(
        logHtml(QStringLiteral("Could not resolve audio stream URL for %1")
                    .arg(task.videoId),
                "#ff6666"));
    return false;
  }

  const QStringList ffArgs = {"-y",
                              "-nostdin",

                              "-reconnect",
                              "1",
                              "-reconnect_streamed",
                              "1",
                              "-reconnect_delay_max",
                              "5",
                              "-i",
                              streamUrl,

                              "-ss",
                              QString::number(task.startSeconds),
                              "-t",
                              QString::number(m_clipDuration),
                              "-vn",
                              "-acodec",
                              "libmp3lame",
                              "-b:a",
                              "192k",
                              "-loglevel",
                              "warning",
                              "-hide_banner",
                              finalFile};

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(
        logHtml(QStringLiteral("[Pass 2/MP3] ffmpeg seek+trim+encode → %1")
                    .arg(QFileInfo(finalFile).fileName()),
                "#66ccff"));
    emit logOutput(logHtml(QStringLiteral("[Pass 2/MP3] %1 %2")
                               .arg(m_ffmpegPath, ffArgs.join(' ')),
                           "#444444"));
  }

  QProcess ffmpeg;
  ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
  ffmpeg.setStandardInputFile(QProcess::nullDevice());
  ffmpeg.setProgram(m_ffmpegPath);
  ffmpeg.setArguments(ffArgs);
#ifdef _WIN32
  ffmpeg.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif

  QString ffLog;
  const int ffExit = runProcess(
      ffmpeg,
      QStringLiteral("ff_%1_%2").arg(task.videoId).arg(task.startSeconds),
      m_activeProcesses, m_mutex, m_logMutex, m_running, ffLog,
      [this](const QString &line) {
        emit logOutput(logHtml(line, "#555555"));
      });

  if (ffExit == -1)
    return false;

  if (ffExit != 0) {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(QStringLiteral("ffmpeg MP3 encode failed for %1: %2")
                               .arg(task.videoId, ffLog.trimmed()),
                           "#ff6666"));
    QFile::remove(finalFile);
    return false;
  }

  {
    QMutexLocker lock(&m_logMutex);
    emit logOutput(logHtml(
        QStringLiteral("Saved: %1").arg(QFileInfo(finalFile).fileName()),
        "#90ee90"));
  }
  return true;
}

void ClipWorker::processTaskQueue() {
  struct ActiveTask {
    QFuture<bool> future;
    ClipTask task;
  };

  QList<ActiveTask> activeTasks;
  int taskIndex = 0;

  while (m_running.load() &&
         (taskIndex < m_taskQueue.size() || !activeTasks.isEmpty())) {

    while (activeTasks.size() < m_maxConcurrent &&
           taskIndex < m_taskQueue.size() && m_running.load()) {
      const ClipTask task = m_taskQueue[taskIndex++];
      activeTasks.append(
          {QtConcurrent::run([this, task]() { return downloadClip(task); }),
           task});
    }

    for (int i = activeTasks.size() - 1; i >= 0; --i) {
      if (!activeTasks[i].future.isFinished())
        continue;

      const bool ok = activeTasks[i].future.result();
      if (ok) {
        m_completedCount.fetch_add(1);
      } else {
        ClipTask failed = activeTasks[i].task;
        if (failed.retryCount < m_maxRetries) {
          ++failed.retryCount;
          {
            QMutexLocker lock(&m_logMutex);
            emit logOutput(
                logHtml(QStringLiteral("Retrying %1 (attempt %2/%3)...")
                            .arg(failed.videoId)
                            .arg(failed.retryCount + 1)
                            .arg(m_maxRetries + 1),
                        "orange"));
          }
          m_taskQueue.append(failed);
        } else {
          m_failedCount.fetch_add(1);
          m_failedTasks.append(failed);
        }
      }

      activeTasks.removeAt(i);
      emit progressUpdate(m_completedCount.load(), m_entries.size());
    }

    if (!activeTasks.isEmpty())
      QThread::msleep(50);
  }

  for (auto &active : activeTasks) {
    active.future.waitForFinished();
    if (active.future.result())
      m_completedCount.fetch_add(1);
    else
      m_failedCount.fetch_add(1);
  }
}

void ClipWorker::run() {
  auto qualityLabel = [this]() -> QString {
    switch (m_quality) {
    case Quality::Best:
      return "Best (HLS ≤1080p)";
    case Quality::Q720p:
      return "720p (HLS)";
    case Quality::Q480p:
      return "480p (HLS)";
    case Quality::Q360p:
      return "360p (HLS)";
    case Quality::AudioOnly:
      return "Audio Only";
    }
    return "Best (HLS)";
  };

  emit logOutput(logHtml(
      QStringLiteral(
          "Clip downloader started — Quality: %1  Format: %2  Threads: %3")
          .arg(qualityLabel(), getFormatExtension().toUpper())
          .arg(m_maxConcurrent),
      "#66ccff", true));

  emit logOutput(logHtml("Checking for yt-dlp updates...", "#aaaaaa"));
  {
    QProcess upd;
    upd.setProcessChannelMode(QProcess::MergedChannels);
    upd.setStandardInputFile(QProcess::nullDevice());
    upd.setProgram(m_ytdlpPath);
    upd.setArguments({"-U"});
#ifdef _WIN32
    upd.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif
    upd.start();
    while (!upd.waitForFinished(200)) {
      if (!m_running.load()) {
        killProcessTree(&upd);
        emit logOutput(logHtml("Update cancelled.", "orange"));
        emit finished(false, "Cancelled.");
        return;
      }
    }
    if (upd.exitCode() == 0) {
      const QString out =
          QString::fromLocal8Bit(upd.readAllStandardOutput()).trimmed();
      if (out.contains("up to date", Qt::CaseInsensitive))
        emit logOutput(logHtml("yt-dlp is up to date.", "#90ee90"));
      else if (!out.isEmpty())
        emit logOutput(logHtml("yt-dlp updated successfully.", "#90ee90"));
    } else {
      emit logOutput(logHtml(
          "Warning: yt-dlp update failed — continuing anyway.", "orange"));
    }
  }

  if (!m_running.load()) {
    emit finished(false, "Cancelled.");
    return;
  }

  QDir().mkpath(m_outputDir);
  QDir().mkpath(m_outputDir + "/clips");

  for (const auto &e : m_entries)
    m_taskQueue.append({e.videoId, e.startSeconds, 0});

  emit logOutput(logHtml(
      QStringLiteral(
          "Starting %1 clip(s) — %2 concurrent threads, 2-pass pipeline")
          .arg(m_taskQueue.size())
          .arg(m_maxConcurrent),
      "#66ccff"));

  processTaskQueue();

  const bool success = (m_failedCount.load() == 0);
  const QString msg = QStringLiteral("%1 — Downloaded: %2  Failed: %3")
                          .arg(!m_running.load() ? "Cancelled" : "Finished")
                          .arg(m_completedCount.load())
                          .arg(m_failedCount.load());

  emit logOutput(logHtml(msg, success ? "#90ee90" : "orange", true));
  emit finished(success, msg);
}

}