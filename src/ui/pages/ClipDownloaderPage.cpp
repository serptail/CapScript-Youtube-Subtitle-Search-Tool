#include "ClipDownloaderPage.h"
#include "../../core/ToolPaths.h"
#include "../../workers/ClipWorker.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace CapScript {

ClipDownloaderPage::ClipDownloaderPage(QWidget *parent) : QWidget(parent) {
  setupUi();
}

ClipDownloaderPage::~ClipDownloaderPage() {
  if (m_thread && m_thread->isRunning()) {
    if (m_worker)
      m_worker->requestStop();
    m_thread->quit();
    m_thread->wait(3000);
  }
}

void ClipDownloaderPage::setupUi() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(24, 16, 24, 16);
  rootLayout->setSpacing(12);

  auto *sourceRow = new QHBoxLayout;
  m_sourceLabel = new QLabel("No transcript loaded");
  m_sourceLabel->setStyleSheet(
      "color: #FF0033; font-weight: 600; font-size: 10pt; "
      "background-color: #1e1015; border: 1px solid #FF0033; "
      "border-radius: 6px; padding: 8px 14px;");
  m_sourceLabel->setWordWrap(true);
  sourceRow->addWidget(m_sourceLabel, 1);

  m_loadFileBtn = new QPushButton("  Load Transcript  ");
  m_loadFileBtn->setObjectName("secondaryBtn");
  m_loadFileBtn->setMinimumWidth(140);
  sourceRow->addWidget(m_loadFileBtn);
  rootLayout->addLayout(sourceRow);

  auto *contentRow = new QHBoxLayout;

  auto *tsGroup = new QGroupBox("Timestamps");
  auto *tsLayout = new QVBoxLayout(tsGroup);

  auto *tsBtnRow = new QHBoxLayout;
  m_selectAllBtn = new QPushButton("Select All");
  m_selectAllBtn->setObjectName("ghostBtn");
  m_deselectAllBtn = new QPushButton("Deselect All");
  m_deselectAllBtn->setObjectName("ghostBtn");
  tsBtnRow->addWidget(m_selectAllBtn);
  tsBtnRow->addWidget(m_deselectAllBtn);
  tsBtnRow->addStretch();
  tsLayout->addLayout(tsBtnRow);

  m_timestampTree = new QTreeWidget;
  m_timestampTree->setObjectName("timestampTree");
  m_timestampTree->setHeaderHidden(true);
  m_timestampTree->setRootIsDecorated(true);
  m_timestampTree->setIndentation(24);
  m_timestampTree->setSelectionMode(QAbstractItemView::NoSelection);
  tsLayout->addWidget(m_timestampTree, 1);

  contentRow->addWidget(tsGroup, 3);

  auto *optPanel = new QWidget;
  auto *optLayout = new QVBoxLayout(optPanel);
  optLayout->setContentsMargins(8, 0, 0, 0);
  optLayout->setSpacing(12);

  auto *optGroup = new QGroupBox("Options");
  auto *optGrid = new QGridLayout(optGroup);
  optGrid->setSpacing(8);
  optGrid->setColumnStretch(1, 1);

  int row = 0;
  optGrid->addWidget(new QLabel("Clip Duration (sec):"), row, 0);
  m_clipDurationSpin = new QSpinBox;
  m_clipDurationSpin->setRange(3, 300);
  m_clipDurationSpin->setValue(5);
  optGrid->addWidget(m_clipDurationSpin, row, 1);
  row++;

  optGrid->addWidget(new QLabel("Format:"), row, 0);
  m_formatCombo = new QComboBox;
  m_formatCombo->addItems({"mp4", "mkv", "webm", "mp3 (audio only)"});
  optGrid->addWidget(m_formatCombo, row, 1);
  row++;

  optGrid->addWidget(new QLabel("Quality:"), row, 0);
  m_qualityCombo = new QComboBox;

  m_qualityCombo->addItems({"best", "720p", "480p", "360p"});
  optGrid->addWidget(m_qualityCombo, row, 1);
  row++;

  optGrid->addWidget(new QLabel("Output Folder:"), row, 0);
  auto *outRow = new QHBoxLayout;
  m_outputDirInput = new QLineEdit;
  m_outputDirInput->setPlaceholderText("clips/");
  m_browseOutputBtn = new QPushButton;
  m_browseOutputBtn->setIcon(QIcon(":/icons/browse.svg"));
  m_browseOutputBtn->setIconSize(QSize(16, 16));
  m_browseOutputBtn->setFixedSize(28, 28);
  m_browseOutputBtn->setToolTip("Browse for output folder");
  m_browseOutputBtn->setCursor(Qt::PointingHandCursor);
  m_browseOutputBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; border-radius: "
      "4px; "
      "padding: 0px; min-width:28px; max-width:28px; min-height:28px; "
      "max-height:28px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.10); }");
  outRow->addWidget(m_outputDirInput, 1);
  outRow->addWidget(m_browseOutputBtn);
  optGrid->addLayout(outRow, row, 1);

  optLayout->addWidget(optGroup);

  auto *actionRow = new QHBoxLayout;
  m_downloadBtn = new QPushButton("Download Clips");
  m_downloadBtn->setMinimumWidth(140);
  m_downloadBtn->setMinimumHeight(36);
  m_downloadBtn->setEnabled(false);

  m_cancelBtn = new QPushButton("Cancel");
  m_cancelBtn->setObjectName("cancel_btn");
  m_cancelBtn->setVisible(false);

  actionRow->addWidget(m_downloadBtn);
  actionRow->addWidget(m_cancelBtn);
  optLayout->addLayout(actionRow);

  m_progressBar = new QProgressBar;
  m_progressBar->setFixedHeight(4);
  m_progressBar->setTextVisible(false);
  m_progressBar->setVisible(false);
  optLayout->addWidget(m_progressBar);

  m_statusLabel = new QLabel;
  m_statusLabel->setStyleSheet("color: #888; font-size: 9pt;");
  optLayout->addWidget(m_statusLabel);

  optLayout->addStretch();
  contentRow->addWidget(optPanel, 2);

  rootLayout->addLayout(contentRow, 1);

  m_logDisplay = new QTextEdit;
  m_logDisplay->setObjectName("logDisplay");
  m_logDisplay->setReadOnly(true);
  m_logDisplay->setPlaceholderText("Clip download log...");
  m_logDisplay->setMaximumHeight(150);
  rootLayout->addWidget(m_logDisplay);

  connect(m_loadFileBtn, &QPushButton::clicked, this,
          &ClipDownloaderPage::onLoadTranscript);
  connect(m_browseOutputBtn, &QPushButton::clicked, this,
          &ClipDownloaderPage::onBrowseOutput);
  connect(m_downloadBtn, &QPushButton::clicked, this,
          &ClipDownloaderPage::onDownloadClicked);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &ClipDownloaderPage::onCancelClicked);
  connect(m_selectAllBtn, &QPushButton::clicked, this,
          &ClipDownloaderPage::onSelectAll);
  connect(m_deselectAllBtn, &QPushButton::clicked, this,
          &ClipDownloaderPage::onDeselectAll);
  connect(m_timestampTree, &QTreeWidget::itemChanged, this,
          &ClipDownloaderPage::onTreeItemChanged);
  connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ClipDownloaderPage::onFormatChanged);

  updateQualityState();
}

void ClipDownloaderPage::updateQualityState() {
  QString formatText = m_formatCombo->currentText();
  bool isAudioOnly = formatText.contains("mp3", Qt::CaseInsensitive) ||
                     formatText.contains("audio only", Qt::CaseInsensitive);

  m_qualityCombo->setEnabled(!isAudioOnly);

  if (isAudioOnly) {
    m_qualityCombo->setToolTip(
        "Quality selection is not available for audio-only formats");
    m_qualityCombo->setStyleSheet(
        "QComboBox { color: #666; background-color: #2a2a2a; }");
  } else {
    m_qualityCombo->setToolTip("");
    m_qualityCombo->setStyleSheet("");
  }
}

void ClipDownloaderPage::onFormatChanged(int index) {
  Q_UNUSED(index);
  updateQualityState();
}

void ClipDownloaderPage::loadFromViewer(const QStringList &results,
                                        const QString &outputDir) {
  m_outputDir = outputDir;
  m_outputDirInput->setText(QDir(outputDir).filePath("clips"));
  m_sourceFile = "(from Viewer tab)";
  m_sourceLabel->setText("Source: Most recent search results");
  parseResults(results);
}

void ClipDownloaderPage::onLoadTranscript() {
  QString path =
      QFileDialog::getOpenFileName(this, "Open Transcript File", m_outputDir,
                                   "Text Files (*.txt);;All Files (*)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  m_sourceFile = path;
  m_sourceLabel->setText("Source: " + path);

  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  QString content = in.readAll();
  file.close();

  static QRegularExpression blockSep(
      QStringLiteral("[\\x{2550}]{10,}"),
      QRegularExpression::UseUnicodePropertiesOption);

  QStringList blocks = content.split(blockSep, Qt::SkipEmptyParts);
  QStringList cleanBlocks;
  for (const QString &b : blocks) {
    QString trimmed = b.trimmed();
    if (!trimmed.isEmpty())
      cleanBlocks << trimmed;
  }

  parseResults(cleanBlocks);
}

void ClipDownloaderPage::parseResults(const QStringList &results) {
  m_timestamps.clear();
  m_timestampTree->clear();
  m_updatingChecks = true;

  static QRegularExpression titleRx(R"(Video\s*Title:\s*(.+))");
  static QRegularExpression idRx(R"(Video\s*ID:\s*([a-zA-Z0-9_-]{11}))");
  static QRegularExpression timestampRx(QStringLiteral(
      "[\\x{2573}\\x{D7}xX]\\s*(\\d{1,2}:\\d{2}:\\d{2})\\s*-\\s*(.*)"));

  int totalTs = 0;

  for (const QString &block : results) {
    QStringList lines = block.split('\n');
    QString videoTitle, videoId;
    QVector<QPair<QString, QString>> tsEntries;

    for (const QString &rawLine : lines) {
      QString line = rawLine.trimmed();
      if (line.isEmpty() || line.contains(QChar(0x2550)))
        continue;

      auto tm = titleRx.match(line);
      if (tm.hasMatch()) {
        videoTitle = tm.captured(1).trimmed();
        continue;
      }

      auto im = idRx.match(line);
      if (im.hasMatch()) {
        videoId = im.captured(1).trimmed();
        continue;
      }

      if (line.startsWith("Timestamps") || line.startsWith("Channel:") ||
          line.startsWith("Date:") || line.startsWith("Views:"))
        continue;

      auto tsm = timestampRx.match(line);
      if (tsm.hasMatch()) {
        tsEntries.append(
            {tsm.captured(1).trimmed(), tsm.captured(2).trimmed()});
      }
    }

    if (videoId.isEmpty() || tsEntries.isEmpty())
      continue;

    QString parentLabel = videoTitle.isEmpty()
                              ? videoId
                              : QString("%1 (%2)").arg(videoTitle, videoId);

    auto *videoItem = new QTreeWidgetItem(m_timestampTree);
    videoItem->setText(0, parentLabel);
    videoItem->setFlags(videoItem->flags() | Qt::ItemIsUserCheckable);
    videoItem->setCheckState(0, Qt::Checked);

    for (const auto &ts : tsEntries) {
      QStringList parts = ts.first.split(':');
      int seconds = 0;
      if (parts.size() == 3)
        seconds =
            parts[0].toInt() * 3600 + parts[1].toInt() * 60 + parts[2].toInt();

      QString childLabel = QString("[%1] %2").arg(ts.first, ts.second);

      auto *tsItem = new QTreeWidgetItem(videoItem);
      tsItem->setText(0, childLabel);
      tsItem->setFlags(tsItem->flags() | Qt::ItemIsUserCheckable);
      tsItem->setCheckState(0, Qt::Checked);
      tsItem->setData(0, Qt::UserRole, videoId);
      tsItem->setData(0, Qt::UserRole + 1, seconds);

      m_timestamps.append({videoId, seconds, childLabel, true});
      totalTs++;
    }

    videoItem->setExpanded(true);
  }

  m_updatingChecks = false;
  m_timestampTree->expandAll();
  m_downloadBtn->setEnabled(totalTs > 0);
  m_statusLabel->setText(
      QString("%1 timestamp%2 loaded across %3 video%4")
          .arg(totalTs)
          .arg(totalTs == 1 ? "" : "s")
          .arg(m_timestampTree->topLevelItemCount())
          .arg(m_timestampTree->topLevelItemCount() == 1 ? "" : "s"));
}

void ClipDownloaderPage::onTreeItemChanged(QTreeWidgetItem *item, int column) {
  if (m_updatingChecks || column != 0)
    return;
  m_updatingChecks = true;

  if (item->childCount() > 0) {
    Qt::CheckState state = item->checkState(0);
    for (int i = 0; i < item->childCount(); ++i) {
      item->child(i)->setCheckState(0, state);
    }
  }

  else if (item->parent()) {
    auto *parent = item->parent();
    int checked = 0, unchecked = 0;
    for (int i = 0; i < parent->childCount(); ++i) {
      if (parent->child(i)->checkState(0) == Qt::Checked)
        checked++;
      else
        unchecked++;
    }
    if (checked == parent->childCount())
      parent->setCheckState(0, Qt::Checked);
    else if (unchecked == parent->childCount())
      parent->setCheckState(0, Qt::Unchecked);
    else
      parent->setCheckState(0, Qt::PartiallyChecked);
  }

  m_updatingChecks = false;
  m_timestampTree->viewport()->repaint();
}

void ClipDownloaderPage::onBrowseOutput() {
  QString dir = QFileDialog::getExistingDirectory(this, "Select Output Folder",
                                                  m_outputDirInput->text());
  if (!dir.isEmpty())
    m_outputDirInput->setText(dir);
}

void ClipDownloaderPage::onSelectAll() {
  m_updatingChecks = true;
  for (int i = 0; i < m_timestampTree->topLevelItemCount(); ++i) {
    auto *item = m_timestampTree->topLevelItem(i);
    item->setCheckState(0, Qt::Checked);
    for (int j = 0; j < item->childCount(); ++j)
      item->child(j)->setCheckState(0, Qt::Checked);
  }
  m_updatingChecks = false;
  m_timestampTree->viewport()->repaint();
  m_statusLabel->setText(QString("All %1 selected").arg(m_timestamps.size()));
}

void ClipDownloaderPage::onDeselectAll() {
  m_updatingChecks = true;
  for (int i = 0; i < m_timestampTree->topLevelItemCount(); ++i) {
    auto *item = m_timestampTree->topLevelItem(i);
    item->setCheckState(0, Qt::Unchecked);
    for (int j = 0; j < item->childCount(); ++j)
      item->child(j)->setCheckState(0, Qt::Unchecked);
  }
  m_updatingChecks = false;
  m_timestampTree->viewport()->repaint();
  m_statusLabel->setText("All deselected");
}

static QString findDenoExecutable() {

  QString binDir = QCoreApplication::applicationDirPath() + "/bin";
  QString denoExe = binDir + "/deno.exe";
  if (QFile::exists(denoExe))
    return denoExe;

  QString userProfile = qEnvironmentVariable("USERPROFILE");
  if (!userProfile.isEmpty()) {
    QString denoUserPath = userProfile + "/.deno/bin/deno.exe";
    if (QFile::exists(denoUserPath))
      return denoUserPath;
  }

  QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
  if (!localAppData.isEmpty()) {
    QString denoLocalPath = localAppData + "/deno/deno.exe";
    if (QFile::exists(denoLocalPath))
      return denoLocalPath;
  }

  QProcess proc;
  proc.start("deno", {"--version"});
  if (proc.waitForStarted() && proc.waitForFinished(5000) &&
      proc.exitCode() == 0) {

    QProcess whereProc;
    whereProc.start("where", {"deno"});
    if (whereProc.waitForStarted() && whereProc.waitForFinished(5000) &&
        whereProc.exitCode() == 0) {
      QString path =
          QString::fromUtf8(whereProc.readAllStandardOutput()).trimmed();
      if (!path.isEmpty()) {

        QStringList paths = path.split('\n', Qt::SkipEmptyParts);
        if (!paths.isEmpty()) {
          QString firstPath = paths.first().trimmed();
          if (QFile::exists(firstPath))
            return firstPath;
        }
      }
    }

    return "deno";
  }

  return QString();
}

void ClipDownloaderPage::onDownloadClicked() {

  QList<TimestampInfo> selected;
  for (int i = 0; i < m_timestampTree->topLevelItemCount(); ++i) {
    auto *parentItem = m_timestampTree->topLevelItem(i);
    for (int j = 0; j < parentItem->childCount(); ++j) {
      auto *child = parentItem->child(j);
      if (child->checkState(0) == Qt::Checked) {
        QString videoId = child->data(0, Qt::UserRole).toString();
        int seconds = child->data(0, Qt::UserRole + 1).toInt();
        selected.append({videoId, seconds, child->text(0), true});
      }
    }
  }

  if (selected.isEmpty()) {
    QMessageBox::information(this, "No Clips",
                             "No timestamps selected for download.");
    return;
  }

  QString ytdlp = ToolPaths::ytdlp();
  QString ffmpeg = ToolPaths::ffmpeg();
  QString deno = findDenoExecutable();

  bool needYtdlp = ytdlp.isEmpty();
  bool needFfmpeg = ffmpeg.isEmpty();
  bool needDeno = deno.isEmpty();

  if (needYtdlp || needFfmpeg) {

    QStringList missing;
    if (needYtdlp)
      missing << "yt-dlp";
    if (needFfmpeg)
      missing << "ffmpeg";
    if (needDeno)
      missing << "deno";

    auto result = QMessageBox::question(
        this, "Missing Tools",
        QString(
            "The following tools were not found:\n\n\u2022 %1\n\n"
            "Would you like to download them automatically?\n"
            "They will be placed in the bin/ folder next to the application.")
            .arg(missing.join("\n\u2022 ")),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::No)
      return;

    downloadMissingTools(needYtdlp, needFfmpeg, needDeno);
    return;
  }

  if (needDeno && !m_denoSkipRequested) {
    auto result = QMessageBox::question(
        this, "Optional Dependency",
        QString("Deno runtime was not found.\n\n"
                "Deno is optional and used for advanced scripting features.\n"
                "Would you like to install it now?\n\n"
                "Click 'No' to skip - clip downloading will work without it."),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
      downloadDenoOnly();
      return;
    }

    m_denoSkipRequested = true;
    m_logDisplay->append("<i>Note: Deno not installed. Some advanced features "
                         "may be unavailable.</i>");
  }

  startClipDownload(selected, ytdlp, ffmpeg);
}

void ClipDownloaderPage::startClipDownload(const QList<TimestampInfo> &selected,
                                           const QString &ytdlp,
                                           const QString &ffmpeg) {
  QString clipDir = m_outputDirInput->text().trimmed();
  if (clipDir.isEmpty())
    clipDir = "clips";
  QDir().mkpath(clipDir);

  QList<ClipWorker::TimestampEntry> entries;
  for (const auto &ts : selected) {
    entries.append({ts.videoId, ts.startSeconds});
  }

  m_logDisplay->clear();
  m_logDisplay->append(QString("Using yt-dlp: %1").arg(ytdlp));
  m_logDisplay->append(QString("Using ffmpeg: %1").arg(ffmpeg));

  QString deno = findDenoExecutable();
  if (!deno.isEmpty()) {
    m_logDisplay->append(QString("Using deno: %1").arg(deno));
  }

  ClipWorker::Quality quality = ClipWorker::Quality::Best;
  QString qualityText = m_qualityCombo->currentText();
  if (qualityText == "720p")
    quality = ClipWorker::Quality::Q720p;
  else if (qualityText == "480p")
    quality = ClipWorker::Quality::Q480p;
  else if (qualityText == "360p")
    quality = ClipWorker::Quality::Q360p;

  ClipWorker::Format format = ClipWorker::Format::MP4;
  QString formatText = m_formatCombo->currentText();
  if (formatText == "mkv")
    format = ClipWorker::Format::MKV;
  else if (formatText == "webm")
    format = ClipWorker::Format::WebM;
  else if (formatText.contains("mp3"))
    format = ClipWorker::Format::MP3;

  int maxConcurrent = 3;
  int maxRetries = 3;
  m_worker =
      new ClipWorker(entries, clipDir, m_clipDurationSpin->value(), ytdlp,
                     ffmpeg, quality, format, maxConcurrent, maxRetries);
  m_thread = new QThread;
  m_worker->moveToThread(m_thread);

  connect(m_thread, &QThread::started, m_worker, &ClipWorker::run);
  connect(m_worker, &ClipWorker::logOutput, this,
          &ClipDownloaderPage::onClipLog);
  connect(m_worker, &ClipWorker::error, this, &ClipDownloaderPage::onClipError);
  connect(m_worker, &ClipWorker::finished, this,
          &ClipDownloaderPage::onClipFinished);
  connect(m_worker, &ClipWorker::finished, m_thread, &QThread::quit);
  connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

  setDownloading(true);
  m_thread->start();
}

void ClipDownloaderPage::onCancelClicked() {
  if (m_worker) {
    m_worker->requestStop();
    m_statusLabel->setText("Cancelling...");
  }
}

void ClipDownloaderPage::onClipLog(const QString &html) {
  m_logDisplay->append(html);
}

void ClipDownloaderPage::onClipError(const QString &html) {
  m_logDisplay->append("<span style='color:#f44336;'>" + html + "</span>");
}

void ClipDownloaderPage::onClipFinished(bool success, const QString &msg) {
  setDownloading(false);
  m_statusLabel->setText(success ? msg : "Download failed.");
  m_worker = nullptr;
  m_thread = nullptr;
}

void ClipDownloaderPage::setDownloading(bool active) {
  m_downloadBtn->setEnabled(!active);
  m_cancelBtn->setVisible(active);
  m_progressBar->setVisible(active);
  if (active) {
    m_progressBar->setRange(0, 0);
    m_statusLabel->setText("Downloading clips...");
  }
}

void ClipDownloaderPage::downloadDenoOnly() {
  if (!m_toolDownloader)
    m_toolDownloader = new QNetworkAccessManager(this);

  QString binDir = QCoreApplication::applicationDirPath() + "/bin";
  QDir().mkpath(binDir);

  m_logDisplay->clear();
  m_logDisplay->append("<b>Installing Deno (optional)...</b>");
  m_downloadBtn->setEnabled(false);
  m_progressBar->setVisible(true);
  m_progressBar->setRange(0, 0);

  const QString denoUrl = "https://github.com/denoland/deno/releases/latest/"
                          "download/deno-x86_64-pc-windows-msvc.zip";
  const QString denoExeName = "deno.exe";

  m_statusLabel->setText("Installing Deno...");
  m_logDisplay->append("Method 1: Trying winget install DenoLand.Deno...");

  auto *wingetProc = new QProcess(this);
  wingetProc->setProgram("winget");
  wingetProc->setArguments({"install", "DenoLand.Deno",
                            "--accept-source-agreements",
                            "--accept-package-agreements"});

  connect(
      wingetProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      this, [=](int wingetExitCode, QProcess::ExitStatus) {
        if (wingetExitCode == 0) {
          m_logDisplay->append("<span style='color:#4CAF50;'>✓ Deno installed "
                               "via winget</span>");
          finishDenoDownload(true);
          wingetProc->deleteLater();
          return;
        }

        m_logDisplay->append(
            "Winget not available or failed. Trying PowerShell installer...");

        auto *psProc = new QProcess(this);
        psProc->setProgram("powershell");
        psProc->setArguments({"-NoProfile", "-ExecutionPolicy", "Bypass",
                              "-Command",
                              "irm https://deno.land/install.ps1 | iex"});

        connect(
            psProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [=](int psExitCode, QProcess::ExitStatus) {
              if (psExitCode == 0) {
                m_logDisplay->append("<span style='color:#4CAF50;'>✓ Deno "
                                     "installed via PowerShell script</span>");
                finishDenoDownload(true);
                psProc->deleteLater();
                wingetProc->deleteLater();
                return;
              }

              m_logDisplay->append("PowerShell installer failed. Downloading "
                                   "Deno binary directly...");

              QNetworkRequest request{QUrl(denoUrl)};
              request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                   QNetworkRequest::NoLessSafeRedirectPolicy);

              auto *reply = m_toolDownloader->get(request);
              connect(reply, &QNetworkReply::downloadProgress, this,
                      [this](qint64 recv, qint64 total) {
                        if (total > 0) {
                          m_progressBar->setRange(0, 100);
                          m_progressBar->setValue(
                              static_cast<int>(recv * 100 / total));
                        }
                      });
              connect(reply, &QNetworkReply::finished, this, [=]() {
                if (reply->error() == QNetworkReply::NoError) {
                  QString tempZip = QDir::tempPath() + "/capscript_deno.zip";
                  {
                    QFile f(tempZip);
                    if (f.open(QIODevice::WriteOnly)) {
                      f.write(reply->readAll());
                      f.close();
                    }
                  }

                  QString tempDir =
                      QDir::tempPath() + "/capscript_deno_extract";
                  QDir(tempDir).removeRecursively();
                  QDir().mkpath(tempDir);

                  auto *extractProc = new QProcess(this);
                  extractProc->setProgram("powershell");
                  extractProc->setArguments(
                      {"-NoProfile", "-Command",
                       QString("Expand-Archive -Path '%1' -DestinationPath "
                               "'%2' -Force")
                           .arg(tempZip, tempDir)});

                  connect(extractProc,
                          QOverload<int, QProcess::ExitStatus>::of(
                              &QProcess::finished),
                          this, [=](int extractExitCode, QProcess::ExitStatus) {
                            if (extractExitCode == 0) {
                              QString srcExe = tempDir + "/" + denoExeName;
                              QString destExe = binDir + "/" + denoExeName;
                              QFile::remove(destExe);
                              if (QFile::copy(srcExe, destExe)) {
                                m_logDisplay->append(
                                    "<span style='color:#4CAF50;'>✓ Deno "
                                    "binary downloaded to bin/</span>");
                                finishDenoDownload(true);
                              } else {
                                m_logDisplay->append(
                                    "<span style='color:#f44336;'>✗ Failed to "
                                    "copy Deno to bin/</span>");
                                finishDenoDownload(false);
                              }
                            } else {
                              m_logDisplay->append(
                                  "<span style='color:#f44336;'>✗ Failed to "
                                  "extract Deno archive</span>");
                              finishDenoDownload(false);
                            }

                            QFile::remove(tempZip);
                            QDir(tempDir).removeRecursively();
                            extractProc->deleteLater();
                            psProc->deleteLater();
                            wingetProc->deleteLater();
                            reply->deleteLater();
                          });
                  extractProc->start();
                } else {
                  m_logDisplay->append("<span style='color:#f44336;'>✗ Failed "
                                       "to download Deno: " +
                                       reply->errorString() + "</span>");
                  finishDenoDownload(false);
                  reply->deleteLater();
                  psProc->deleteLater();
                  wingetProc->deleteLater();
                }
              });
            });
        psProc->start();
      });
  wingetProc->start();
}

void ClipDownloaderPage::finishDenoDownload(bool success) {
  m_progressBar->setVisible(false);
  m_downloadBtn->setEnabled(true);

  if (success && !findDenoExecutable().isEmpty()) {
    m_statusLabel->setText("Deno installed. Click Download Clips to proceed.");
    m_logDisplay->append("<b>Deno is ready!</b>");
  } else {
    m_statusLabel->setText(
        "Deno installation skipped. Clip downloading available.");
    m_logDisplay->append("<i>You can continue without Deno - clip downloads "
                         "will work normally.</i>");
  }
}

void ClipDownloaderPage::downloadMissingTools(bool needYtdlp, bool needFfmpeg,
                                              bool needDeno) {
  if (!m_toolDownloader)
    m_toolDownloader = new QNetworkAccessManager(this);

  QString binDir = QCoreApplication::applicationDirPath() + "/bin";
  QDir().mkpath(binDir);

  m_logDisplay->clear();
  m_logDisplay->append("<b>Starting tool download...</b>");
  m_downloadBtn->setEnabled(false);
  m_progressBar->setVisible(true);
  m_progressBar->setRange(0, 0);

  const QString ytdlpUrl =
      "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
  const QString ytdlpFileName = "yt-dlp.exe";

  const QString ffmpegUrl =
      "https://github.com/yt-dlp/FFmpeg-Builds/releases/download/latest/"
      "ffmpeg-master-latest-win64-gpl.zip";
  const QString ffmpegExeName = "ffmpeg.exe";

  auto doYtdlp = [=](std::function<void()> next) {
    if (!needYtdlp) {
      next();
      return;
    }

    m_statusLabel->setText("Downloading yt-dlp...");
    m_logDisplay->append("Downloading yt-dlp from GitHub...");

    QNetworkRequest request{QUrl(ytdlpUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_toolDownloader->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 recv, qint64 total) {
              if (total > 0) {
                m_progressBar->setRange(0, 100);
                m_progressBar->setValue(static_cast<int>(recv * 100 / total));
              }
            });
    connect(reply, &QNetworkReply::finished, this, [=]() {
      if (reply->error() == QNetworkReply::NoError) {
        QFile f(binDir + "/" + ytdlpFileName);
        if (f.open(QIODevice::WriteOnly)) {
          f.write(reply->readAll());
          f.close();
          m_logDisplay->append("<span style='color:#4CAF50;'>✓ yt-dlp "
                               "downloaded successfully</span>");
        } else {
          m_logDisplay->append("<span style='color:#f44336;'>✗ Failed to write "
                               "yt-dlp to disk</span>");
        }
      } else {
        m_logDisplay->append(
            "<span style='color:#f44336;'>✗ Failed to download yt-dlp: " +
            reply->errorString() + "</span>");
      }
      reply->deleteLater();
      next();
    });
  };

  auto doFfmpeg = [=](std::function<void()> next) {
    if (!needFfmpeg) {
      next();
      return;
    }

    m_statusLabel->setText("Downloading FFmpeg from yt-dlp/FFmpeg-Builds...");
    m_logDisplay->append(
        "Downloading FFmpeg build (ffmpeg-master-latest-win64-gpl.zip)...");
    m_progressBar->setRange(0, 0);

    QNetworkRequest request{QUrl(ffmpegUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_toolDownloader->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 recv, qint64 total) {
              if (total > 0) {
                m_progressBar->setRange(0, 100);
                m_progressBar->setValue(static_cast<int>(recv * 100 / total));
              }
            });
    connect(reply, &QNetworkReply::finished, this, [=]() {
      if (reply->error() != QNetworkReply::NoError) {
        m_logDisplay->append(
            "<span style='color:#f44336;'>✗ Failed to download FFmpeg: " +
            reply->errorString() + "</span>");
        reply->deleteLater();
        next();
        return;
      }

      QString tempZip = QDir::tempPath() + "/capscript_ffmpeg_archive.zip";
      {
        QFile f(tempZip);
        if (f.open(QIODevice::WriteOnly)) {
          f.write(reply->readAll());
          f.close();
        }
      }
      reply->deleteLater();

      m_statusLabel->setText("Extracting FFmpeg...");
      m_logDisplay->append("Extracting FFmpeg archive...");
      m_progressBar->setRange(0, 0);

      QString tempDir = QDir::tempPath() + "/capscript_ffmpeg_extract";
      QDir(tempDir).removeRecursively();
      QDir().mkpath(tempDir);

      auto *proc = new QProcess(this);
      proc->setProgram("powershell");
      proc->setArguments(
          {"-NoProfile", "-Command",
           QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
               .arg(tempZip, tempDir)});

      connect(
          proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [=](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
              bool found = false;
              QDirIterator it(tempDir, {ffmpegExeName}, QDir::Files,
                              QDirIterator::Subdirectories);
              while (it.hasNext()) {
                it.next();
                QString filePath = it.filePath();
                if (filePath.contains("/bin/") ||
                    filePath.contains("\\bin\\")) {
                  QString dest = binDir + "/" + ffmpegExeName;
                  QFile::remove(dest);
                  if (QFile::copy(filePath, dest)) {
                    m_logDisplay->append(
                        "<span style='color:#4CAF50;'>✓ FFmpeg downloaded and "
                        "extracted successfully</span>");
                    found = true;
                    break;
                  }
                }
              }

              if (!found) {
                QDirIterator it2(tempDir, {ffmpegExeName}, QDir::Files,
                                 QDirIterator::Subdirectories);
                if (it2.hasNext()) {
                  it2.next();
                  QString dest = binDir + "/" + ffmpegExeName;
                  QFile::remove(dest);
                  if (QFile::copy(it2.filePath(), dest)) {
                    m_logDisplay->append(
                        "<span style='color:#4CAF50;'>✓ FFmpeg downloaded and "
                        "extracted successfully</span>");
                    found = true;
                  }
                }
              }

              if (!found) {
                m_logDisplay->append("<span style='color:#f44336;'>✗ Could not "
                                     "find ffmpeg.exe in archive</span>");
              }
            } else {
              m_logDisplay->append("<span style='color:#f44336;'>✗ Failed to "
                                   "extract FFmpeg (exit code " +
                                   QString::number(exitCode) + ")</span>");
            }

            QFile::remove(tempZip);
            QDir(tempDir).removeRecursively();
            proc->deleteLater();
            next();
          });
      proc->start();
    });
  };

  doYtdlp([=]() {
    doFfmpeg([=]() {
      bool hasYtdlp = !ToolPaths::ytdlp().isEmpty();
      bool hasFfmpeg = !ToolPaths::ffmpeg().isEmpty();

      if (hasYtdlp && hasFfmpeg) {

        if (needDeno) {
          m_logDisplay->append(
              "<b>Required tools installed. Now installing Deno...</b>");

          m_statusLabel->setText("Installing Deno...");
          m_logDisplay->append("Installing Deno (optional dependency)...");

          auto *wingetProc = new QProcess(this);
          wingetProc->setProgram("winget");
          wingetProc->setArguments({"install", "DenoLand.Deno",
                                    "--accept-source-agreements",
                                    "--accept-package-agreements"});

          connect(
              wingetProc,
              QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
              this, [=](int wingetExitCode, QProcess::ExitStatus) {
                if (wingetExitCode == 0) {
                  m_logDisplay->append("<span style='color:#4CAF50;'>✓ Deno "
                                       "installed via winget</span>");
                  m_progressBar->setVisible(false);
                  m_downloadBtn->setEnabled(true);
                  m_statusLabel->setText(
                      "All tools installed! Click Download Clips to proceed.");
                  m_logDisplay->append(
                      "<b>All tools installed successfully.</b>");
                  wingetProc->deleteLater();
                  return;
                }

                m_logDisplay->append("Winget not available or failed. Trying "
                                     "PowerShell installer...");
                auto *psProc = new QProcess(this);
                psProc->setProgram("powershell");
                psProc->setArguments(
                    {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                     "irm https://deno.land/install.ps1 | iex"});

                connect(
                    psProc,
                    QOverload<int, QProcess::ExitStatus>::of(
                        &QProcess::finished),
                    this, [=](int psExitCode, QProcess::ExitStatus) {
                      if (psExitCode == 0) {
                        m_logDisplay->append(
                            "<span style='color:#4CAF50;'>✓ Deno installed via "
                            "PowerShell script</span>");
                      } else {
                        m_logDisplay->append(
                            "<span style='color:#f0ad4e;'>⚠ Deno installation "
                            "failed, but required tools are ready.</span>");
                      }
                      m_progressBar->setVisible(false);
                      m_downloadBtn->setEnabled(true);
                      m_statusLabel->setText("Required tools ready! Click "
                                             "Download Clips to proceed.");
                      m_logDisplay->append("<b>Setup complete.</b>");
                      psProc->deleteLater();
                      wingetProc->deleteLater();
                    });
                psProc->start();
              });
          wingetProc->start();
        } else {

          m_progressBar->setVisible(false);
          m_downloadBtn->setEnabled(true);
          m_statusLabel->setText(
              "Required tools ready! Click Download Clips to proceed.");
          m_logDisplay->append("<b>Required tools installed successfully.</b>");
        }
      } else {
        m_progressBar->setVisible(false);
        m_downloadBtn->setEnabled(true);
        QStringList still;
        if (!hasYtdlp)
          still << "yt-dlp";
        if (!hasFfmpeg)
          still << "ffmpeg";
        m_statusLabel->setText("Still missing: " + still.join(", "));
        m_logDisplay->append("<span style='color:#f44336;'>Some required tools "
                             "could not be installed.</span>");
      }
    });
  });
}

}
