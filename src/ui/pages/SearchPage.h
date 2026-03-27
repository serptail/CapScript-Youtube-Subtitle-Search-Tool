#pragma once
#ifndef CAPSCRIPT_SEARCHPAGE_H
#define CAPSCRIPT_SEARCHPAGE_H

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QThread>
#include <QWidget>

namespace CapScript {

class SearchWorker;

class SearchPage : public QWidget {
  Q_OBJECT

public:
  explicit SearchPage(QWidget *parent = nullptr);
  ~SearchPage() override;

  QString outputDir() const;
  QStringList lastResults() const;

signals:
  void searchFinished(int count, const QStringList &results);

private slots:
  void onSearchClicked();
  void onCancelClicked();
  void onBrowseOutput();
  void onPasteApiKey();
  void onToggleApiVisibility();
  void onModeChanged();
  void onBrowseCookies();

  void onWorkerProgress(int percent);
  void onWorkerLog(const QString &html);
  void onWorkerFinished(int count, const QStringList &results);
  void onWorkerError(const QString &msg);

protected:
  void showEvent(QShowEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

private:
  void positionFeedbackBtn();
  void setupUi();
  void setSearching(bool active);
  void toggleLogExpanded();
  bool eventFilter(QObject *watched, QEvent *event) override;

  QLineEdit *m_apiKeyInput = nullptr;
  QPushButton *m_pasteBtn = nullptr;
  QPushButton *m_apiToggleBtn = nullptr;
  QPushButton *m_apiInfoBtn = nullptr;
  bool m_apiVisible = false;

  QRadioButton *m_videoRadio = nullptr;
  QRadioButton *m_channelRadio = nullptr;
  QStackedWidget *m_modeStack = nullptr;

  QLineEdit *m_videoIdsInput = nullptr;
  QLineEdit *m_videoSearchTerm = nullptr;
  QComboBox *m_videoLangCombo = nullptr;

  QLineEdit *m_channelInput = nullptr;
  QLineEdit *m_channelSearchTerm = nullptr;
  QComboBox *m_channelLangCombo = nullptr;
  QSpinBox *m_maxResultsSpin = nullptr;

  QLineEdit *m_outputDirInput = nullptr;
  QPushButton *m_browseBtn = nullptr;
  QLineEdit *m_outputFileInput = nullptr;
  QLineEdit *m_cookiesFileInput = nullptr;
  QPushButton *m_cookiesBrowseBtn = nullptr;

  QComboBox *m_proxyTypeCombo = nullptr;
  QLineEdit *m_proxyUrlInput = nullptr;
  QLineEdit *m_proxyUsernameInput = nullptr;
  QLineEdit *m_proxyPasswordInput = nullptr;

  QPushButton *m_searchBtn = nullptr;
  QPushButton *m_cancelBtn = nullptr;
  QProgressBar *m_progressBar = nullptr;
  QLabel *m_statusLabel = nullptr;

  QTextEdit *m_logDisplay = nullptr;
  QSplitter *m_splitter = nullptr;
  bool m_logExpanded = false;
  QList<int> m_savedSizes;

  SearchWorker *m_worker = nullptr;
  QThread *m_thread = nullptr;
  QStringList m_lastResults;

  QPushButton *m_feedbackBtn = nullptr;
  class FeedbackPanel *m_feedbackPanel = nullptr;
};

}

#endif
