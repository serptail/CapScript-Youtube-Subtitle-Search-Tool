#pragma once
#ifndef CAPSCRIPT_RENDERWORKER_H
#define CAPSCRIPT_RENDERWORKER_H

#include <QObject>
#include <QProcess>
#include <atomic>

namespace CapScript {

class RenderWorker : public QObject {
  Q_OBJECT

public:
  explicit RenderWorker(const QString &clipsFolder, const QString &outputPath,
                        const QString &ffmpegPath, QObject *parent = nullptr);

public slots:
  void run();
  void stop();
  void requestStop() { stop(); }

signals:
  void logOutput(const QString &htmlMessage);
  void progressUpdate(int percent);
  void finished(bool success, const QString &message);
  void error(const QString &htmlMessage);

private:
  enum class ClipFormat { Mp4, Mkv, Webm };

  std::optional<ClipFormat> detectFormat(QStringList &outFiles);

  QString ffprobePath() const;

  double probeTotalDuration(const QStringList &absPaths) const;

  void handleOutputLine(const QString &line, double totalSecs, int &lastPct);

  QString m_clipsFolder;
  QString m_outputPath;
  QString m_ffmpegPath;
  std::atomic<bool> m_running{true};
  QProcess *m_process = nullptr;
};

}

#endif