#pragma once
#ifndef CAPSCRIPT_CLIPDOWNLOADERPAGE_H
#define CAPSCRIPT_CLIPDOWNLOADERPAGE_H

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTextEdit>
#include <QThread>
#include <QTreeWidget>
#include <QWidget>

class QNetworkAccessManager;

namespace CapScript {

class ClipWorker;

class ClipDownloaderPage : public QWidget {
  Q_OBJECT

public:
  explicit ClipDownloaderPage(QWidget *parent = nullptr);
  ~ClipDownloaderPage() override;

public slots:
  
  void loadFromViewer(const QStringList &results, const QString &outputDir);

private slots:
  void onLoadTranscript();
  void onBrowseOutput();
  void onDownloadClicked();
  void onCancelClicked();
  void onSelectAll();
  void onDeselectAll();
  void onTreeItemChanged(QTreeWidgetItem *item, int column);
  void onFormatChanged(int index);

  void onClipLog(const QString &html);
  void onClipError(const QString &html);
  void onClipFinished(bool success, const QString &msg);

private:
  struct TimestampInfo {
    QString videoId;
    int startSeconds;
    QString label;
    bool selected = true;
  };

  void setupUi();
  void parseResults(const QStringList &results);
  void setDownloading(bool active);
  void updateQualityState();

  void downloadMissingTools(bool needYtdlp, bool needFfmpeg, bool needDeno);
  void downloadDenoOnly();
  void finishDenoDownload(bool success);

  void startClipDownload(const QList<TimestampInfo> &selected,
                         const QString &ytdlp, const QString &ffmpeg);

  QLabel *m_sourceLabel = nullptr;
  QPushButton *m_loadFileBtn = nullptr;

  QTreeWidget *m_timestampTree = nullptr;

  QSpinBox *m_clipDurationSpin = nullptr;
  QComboBox *m_formatCombo = nullptr;
  QComboBox *m_qualityCombo = nullptr;
  QLineEdit *m_outputDirInput = nullptr;
  QPushButton *m_browseOutputBtn = nullptr;

  QPushButton *m_downloadBtn = nullptr;
  QPushButton *m_cancelBtn = nullptr;
  QPushButton *m_selectAllBtn = nullptr;
  QPushButton *m_deselectAllBtn = nullptr;
  QProgressBar *m_progressBar = nullptr;
  QLabel *m_statusLabel = nullptr;

  QTextEdit *m_logDisplay = nullptr;

  QList<TimestampInfo> m_timestamps;
  QString m_outputDir;
  QString m_sourceFile;
  bool m_updatingChecks = false;
  bool m_denoSkipRequested = false;

  ClipWorker *m_worker = nullptr;
  QThread *m_thread = nullptr;

  QNetworkAccessManager *m_toolDownloader = nullptr;
};

}

#endif