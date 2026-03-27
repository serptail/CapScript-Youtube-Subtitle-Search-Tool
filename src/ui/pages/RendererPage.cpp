#include "RendererPage.h"
#include "../../core/ToolPaths.h"
#include "../../workers/RenderWorker.h"
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace CapScript {

RendererPage::RendererPage(QWidget *parent) : QWidget(parent) { setupUi(); }

RendererPage::~RendererPage() {
  if (m_thread && m_thread->isRunning()) {
    if (m_worker)
      m_worker->requestStop();
    m_thread->quit();
    m_thread->wait(5000);
  }
}

void RendererPage::setupUi() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(20, 16, 20, 16);
  rootLayout->setSpacing(16);

  auto *configGroup = new QGroupBox("Render Configuration");
  auto *configGrid = new QGridLayout(configGroup);
  configGrid->setSpacing(10);
  configGrid->setColumnStretch(1, 1);

  int row = 0;
  configGrid->addWidget(new QLabel("Clips Folder:"), row, 0);
  auto *clipsRow = new QHBoxLayout;
  m_clipsFolderInput = new QLineEdit;
  m_clipsFolderInput->setPlaceholderText(
      "Path to folder containing .mp4/mkv/webm clips...");
  m_browseClipsBtn = new QPushButton;
  m_browseClipsBtn->setIcon(QIcon(":/icons/browse.svg"));
  m_browseClipsBtn->setIconSize(QSize(16, 16));
  m_browseClipsBtn->setFixedSize(28, 28);
  m_browseClipsBtn->setToolTip("Browse for clips folder");
  m_browseClipsBtn->setCursor(Qt::PointingHandCursor);
  m_browseClipsBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; border-radius: "
      "4px; "
      "padding: 0px; min-width:28px; max-width:28px; min-height:28px; "
      "max-height:28px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.10); }");
  clipsRow->addWidget(m_clipsFolderInput, 1);
  clipsRow->addWidget(m_browseClipsBtn);
  configGrid->addLayout(clipsRow, row, 1);
  row++;

  configGrid->addWidget(new QLabel("Output File:"), row, 0);
  auto *outRow = new QHBoxLayout;
  m_outputPathInput = new QLineEdit;
  m_outputPathInput->setPlaceholderText("output.mp4 (default location)");
  m_browseOutputBtn = new QPushButton;
  m_browseOutputBtn->setIcon(QIcon(":/icons/browse.svg"));
  m_browseOutputBtn->setIconSize(QSize(16, 16));
  m_browseOutputBtn->setFixedSize(28, 28);
  m_browseOutputBtn->setToolTip("Browse for output path");
  m_browseOutputBtn->setCursor(Qt::PointingHandCursor);
  m_browseOutputBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; border-radius: "
      "4px; "
      "padding: 0px; min-width:28px; max-width:28px; min-height:28px; "
      "max-height:28px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.10); }");
  outRow->addWidget(m_outputPathInput, 1);
  outRow->addWidget(m_browseOutputBtn);
  configGrid->addLayout(outRow, row, 1);

  rootLayout->addWidget(configGroup);

  auto *actionRow = new QHBoxLayout;
  m_renderBtn = new QPushButton("Render Video");
  m_renderBtn->setMinimumWidth(140);
  m_cancelBtn = new QPushButton("Cancel");
  m_cancelBtn->setObjectName("cancel_btn");
  m_cancelBtn->setVisible(false);
  actionRow->addWidget(m_renderBtn);
  actionRow->addWidget(m_cancelBtn);
  actionRow->addStretch();

  m_statusLabel = new QLabel;
  m_statusLabel->setStyleSheet("color: #888; font-size: 9pt;");
  actionRow->addWidget(m_statusLabel);
  rootLayout->addLayout(actionRow);

  m_progressBar = new QProgressBar;
  m_progressBar->setRange(0, 100);
  m_progressBar->setFixedHeight(4);
  m_progressBar->setTextVisible(false);
  m_progressBar->setVisible(false);
  rootLayout->addWidget(m_progressBar);

  m_logDisplay = new QTextEdit;
  m_logDisplay->setObjectName("logDisplay");
  m_logDisplay->setReadOnly(true);
  m_logDisplay->setPlaceholderText("Render output log...");
  rootLayout->addWidget(m_logDisplay, 1);

  connect(m_browseClipsBtn, &QPushButton::clicked, this,
          &RendererPage::onBrowseClips);
  connect(m_browseOutputBtn, &QPushButton::clicked, this,
          &RendererPage::onBrowseOutput);
  connect(m_renderBtn, &QPushButton::clicked, this,
          &RendererPage::onRenderClicked);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &RendererPage::onCancelRender);
}

void RendererPage::setDefaultOutputDir(const QString &dir) {
  if (m_clipsFolderInput->text().isEmpty()) {
    m_clipsFolderInput->setText(QDir(dir).filePath("clips"));
  }
  if (m_outputPathInput->text().isEmpty()) {
    QString outName = "rendered_" +
                      QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") +
                      ".mp4";
    m_outputPathInput->setText(QDir(dir).filePath(outName));
  }
}

void RendererPage::onBrowseClips() {
  QString dir = QFileDialog::getExistingDirectory(this, "Select Clips Folder",
                                                  m_clipsFolderInput->text());
  if (!dir.isEmpty())
    m_clipsFolderInput->setText(dir);
}

void RendererPage::onBrowseOutput() {
  QString path = QFileDialog::getSaveFileName(this, "Save Rendered Video",
                                              m_outputPathInput->text(),
                                              "MP4 Video (*.mp4)");
  if (!path.isEmpty())
    m_outputPathInput->setText(path);
}

void RendererPage::onRenderClicked() {
  QString clipsFolder = m_clipsFolderInput->text().trimmed();
  if (clipsFolder.isEmpty()) {
    QMessageBox::warning(this, "Missing Input",
                         "Please specify the clips folder.");
    return;
  }

  if (!QDir(clipsFolder).exists()) {
    QMessageBox::warning(this, "Folder Not Found",
                         "The clips folder does not exist:\n" + clipsFolder);
    return;
  }

  QString outputPath = m_outputPathInput->text().trimmed();
  if (outputPath.isEmpty()) {
    outputPath = QDir(clipsFolder).filePath("rendered.mp4");
    m_outputPathInput->setText(outputPath);
  }

  QString ffmpeg = ToolPaths::ffmpeg();
  if (ffmpeg.isEmpty()) {
    QMessageBox::warning(this, "ffmpeg Not Found",
                         "ffmpeg was not found. Please install it or place it "
                         "in the bin/ folder.");
    return;
  }

  m_logDisplay->clear();

  m_worker = new RenderWorker(clipsFolder, outputPath, ffmpeg);
  m_thread = new QThread;
  m_worker->moveToThread(m_thread);

  connect(m_thread, &QThread::started, m_worker, &RenderWorker::run);
  connect(m_worker, &RenderWorker::logOutput, this, &RendererPage::onRenderLog);
  connect(m_worker, &RenderWorker::progressUpdate, this,
          &RendererPage::onRenderProgress);
  connect(m_worker, &RenderWorker::finished, this,
          &RendererPage::onRenderFinished);
  connect(m_worker, &RenderWorker::error, this, &RendererPage::onRenderError);

  connect(m_worker, &RenderWorker::finished, m_thread, &QThread::quit);
  connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

  setRendering(true);
  m_thread->start();
}

void RendererPage::onCancelRender() {
  if (m_worker) {
    m_worker->requestStop();
    m_statusLabel->setText("Cancelling...");
  }
}

void RendererPage::onRenderLog(const QString &html) {
  m_logDisplay->append(html);
}

void RendererPage::onRenderProgress(int percent) {
  m_progressBar->setValue(percent);
  m_statusLabel->setText(QString("Rendering... %1%").arg(percent));
}

void RendererPage::onRenderFinished(bool success, const QString &msg) {
  setRendering(false);
  m_statusLabel->setText(success ? "Render complete!" : msg);
  if (success) {
    m_progressBar->setValue(100);
    m_progressBar->setVisible(true);
  }

  m_worker = nullptr;
  m_thread = nullptr;
}

void RendererPage::onRenderError(const QString &msg) {
  setRendering(false);
  m_statusLabel->setText("Error");
  m_logDisplay->append("<span style='color:#f44336;'>Error: " + msg +
                       "</span>");

  m_worker = nullptr;
  m_thread = nullptr;
}

void RendererPage::setRendering(bool active) {
  m_renderBtn->setEnabled(!active);
  m_cancelBtn->setVisible(active);
  m_progressBar->setVisible(active);
  if (active) {
    m_progressBar->setValue(0);
    m_statusLabel->setText("Starting render...");
  }
}

}
