#pragma once
#ifndef CAPSCRIPT_CLIPWORKER_H
#define CAPSCRIPT_CLIPWORKER_H

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <atomic>

namespace CapScript {

class ClipWorker : public QObject {
  Q_OBJECT

public:
  struct TimestampEntry {
    QString videoId;
    int startSeconds;
  };

  enum class Quality { Best, Q720p, Q480p, Q360p, AudioOnly };

  enum class Format { MP4, MKV, WebM, MP3 };

  ClipWorker(const QList<TimestampEntry> &entries, const QString &outputDir,
             int clipDuration, const QString &ytdlpPath,
             const QString &ffmpegPath, Quality quality = Quality::Best,
             Format format = Format::MP4, int maxConcurrent = 3,
             int maxRetries = 3, QObject *parent = nullptr);

public slots:
  void run();
  void stop();
  void requestStop() { stop(); }

signals:
  void logOutput(const QString &htmlMessage);
  void error(const QString &htmlMessage);
  void finished(bool success, const QString &message);
  void progressUpdate(int completed, int total);

private:
  struct ClipTask {
    QString videoId;
    int startSeconds;
    int retryCount = 0;
  };

  bool downloadClip(const ClipTask &task);
  bool downloadClipMp3(const ClipTask &task);
  void processTaskQueue();

  QString buildFormatSelector() const;
  QString getFormatExtension() const;
  QString getMergeFormat() const;
  QString buildYtdlpPostprocessorArgs() const;
  QStringList buildFfmpegTrimArgs(const QString &inputFile,
                                  const QString &outputFile,
                                  int startOffsetSec = 0) const;

  QList<TimestampEntry> m_entries;
  QString m_outputDir;
  int m_clipDuration;
  QString m_ytdlpPath;
  QString m_ffmpegPath;
  Quality m_quality;
  Format m_format;
  int m_maxConcurrent;
  int m_maxRetries;

  std::atomic<bool> m_running{true};
  std::atomic<int> m_completedCount{0};
  std::atomic<int> m_failedCount{0};
  QMutex m_mutex;
  QMutex m_logMutex;
  QMap<QString, QProcess *> m_activeProcesses;
  QList<ClipTask> m_taskQueue;
  QList<ClipTask> m_failedTasks;
};

}

#endif