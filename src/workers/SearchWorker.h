#pragma once
#ifndef CAPSCRIPT_SEARCHWORKER_H
#define CAPSCRIPT_SEARCHWORKER_H

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <atomic>

namespace CapScript {

class SearchWorker : public QObject {
  Q_OBJECT

public:
  explicit SearchWorker(const QJsonObject &params, QObject *parent = nullptr);

public slots:
  void run();
  void stop();
  void requestStop() { stop(); }

signals:
  void progressUpdate(int percent);
  void logOutput(const QString &htmlMessage);
  void finished(int matchCount, const QStringList &results);
  void error(const QString &message);

private:
  QJsonObject m_params;
  std::atomic<bool> m_running{true};
};

}

#endif
