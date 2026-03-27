#pragma once
#ifndef CAPSCRIPT_ABOUTPAGE_H
#define CAPSCRIPT_ABOUTPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;
class QProgressBar;

namespace CapScript {

class AboutPage : public QWidget {
  Q_OBJECT

public:
  explicit AboutPage(QWidget *parent = nullptr);

  void setUpdateStatus(const QString &status);
  void setUpdateProgressVisible(bool visible);
  void setUpdateProgress(int percent);
  void setBusy(bool busy);

signals:
  void checkForUpdatesRequested();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void setupUi();

  QLabel *m_statusLabel = nullptr;
  QPushButton *m_checkUpdatesBtn = nullptr;
  QLabel *m_updateProgressLabel = nullptr;
  QProgressBar *m_updateProgressBar = nullptr;
};

}

#endif
