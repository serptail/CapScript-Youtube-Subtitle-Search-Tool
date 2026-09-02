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
#include <QtMath>

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

  auto *configHeading = new QLabel("Render Configuration");
  configHeading->setStyleSheet(
      "font-size: 12px;"
      "font-weight: normal;"
      "color: #eeeeee;"
  );
  rootLayout->addWidget(configHeading);

  auto *configGroup = new QGroupBox();
  configGroup->setObjectName("renderConfigGroup");

  auto *configGrid = new QGridLayout(configGroup);
  configGrid->setSpacing(10);
  configGrid->setColumnStretch(1, 1);

  rootLayout->addWidget(configGroup);

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
  row++;

  configGrid->addWidget(new QLabel("Resolution:"), row, 0);
  m_resolutionCombo = new QComboBox;
  m_resolutionCombo->addItem("Source", "source");
  m_resolutionCombo->addItem("2160p (4K)", "2160");
  m_resolutionCombo->addItem("1440p", "1440");
  m_resolutionCombo->addItem("1080p", "1080");
  m_resolutionCombo->addItem("720p", "720");
  m_resolutionCombo->addItem("480p", "480");
  configGrid->addWidget(m_resolutionCombo, row, 1);
  row++;

  configGrid->addWidget(new QLabel("Output Format:"), row, 0);
  m_formatCombo = new QComboBox;
  m_formatCombo->addItem("MP4 (H.264)", "mp4");
  m_formatCombo->addItem("MOV (ProRes)", "mov");
  m_formatCombo->addItem("MKV (H.265)", "mkv");
  m_formatCombo->addItem("WebM (VP9)", "webm");
  configGrid->addWidget(m_formatCombo, row, 1);
  row++;

  configGrid->addWidget(new QLabel("Frame Rate:"), row, 0);
  m_fpsSpin = new QSpinBox;
  m_fpsSpin->setRange(15, 120);
  m_fpsSpin->setValue(30);
  m_fpsSpin->setSuffix(" fps");
  configGrid->addWidget(m_fpsSpin, row, 1);
  row++;

  configGrid->addWidget(new QLabel("Quality:"), row, 0);
  auto *qualityRow = new QHBoxLayout;
  m_qualitySlider = new QSlider(Qt::Horizontal);
  m_qualitySlider->setObjectName("qualitySlider");
  m_qualitySlider->setRange(0, 100);
  m_qualitySlider->setValue(70);
  m_qualitySlider->setTickPosition(QSlider::TicksBelow);
  m_qualitySlider->setTickInterval(10);
  m_qualityValueLabel = new QLabel("CRF 20 (High)");
  m_qualityValueLabel->setMinimumWidth(90);
  m_qualityValueLabel->setStyleSheet("color: #909090; font-size: 8.5pt;");
  qualityRow->addWidget(m_qualitySlider, 1);
  qualityRow->addWidget(m_qualityValueLabel);
  configGrid->addLayout(qualityRow, row, 1);
  row++;

  configGrid->addWidget(new QLabel("Options:"), row, 0);
  auto *optionsRow = new QHBoxLayout;
  m_hwAccelCheck = new QCheckBox("Hardware acceleration");
  m_hwAccelCheck->setChecked(true);
  m_normalizeAudioCheck = new QCheckBox("Normalize audio");
  optionsRow->addWidget(m_hwAccelCheck);
  optionsRow->addWidget(m_normalizeAudioCheck);
  optionsRow->addStretch();
  configGrid->addLayout(optionsRow, row, 1);

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
  connect(m_qualitySlider, &QSlider::valueChanged, this,
          &RendererPage::onQualitySliderChanged);
  onQualitySliderChanged(m_qualitySlider->value());
}

void RendererPage::onQualitySliderChanged(int value) {

  int crf = 32 - qRound((value / 100.0) * 18.0);
  QString label = value >= 80   ? "Very High"
                  : value >= 55 ? "High"
                  : value >= 30 ? "Medium"
                                : "Low";
  m_qualityValueLabel->setText(QString("CRF %1 (%2)").arg(crf).arg(label));
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
  applyRenderOptionsToWorker();

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

void RendererPage::applyRenderOptionsToWorker() {
  if (!m_worker)
    return;

  const int value = m_qualitySlider->value();
  const int crf = 32 - qRound((value / 100.0) * 18.0);

  m_worker->setProperty("resolution",
                        m_resolutionCombo->currentData().toString());
  m_worker->setProperty("outputFormat", m_formatCombo->currentData().toString());
  m_worker->setProperty("frameRate", m_fpsSpin->value());
  m_worker->setProperty("crf", crf);
  m_worker->setProperty("hwAccel", m_hwAccelCheck->isChecked());
  m_worker->setProperty("normalizeAudio", m_normalizeAudioCheck->isChecked());
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