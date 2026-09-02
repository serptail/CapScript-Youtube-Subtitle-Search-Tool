#pragma once
#ifndef CAPSCRIPT_RENDERERPAGE_H
#define CAPSCRIPT_RENDERERPAGE_H

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTextEdit>
#include <QThread>
#include <QWidget>

namespace CapScript {

class RenderWorker;
class RendererPage : public QWidget {
  Q_OBJECT

public:
  explicit RendererPage(QWidget *parent = nullptr);
  ~RendererPage() override;

public slots:
  void setDefaultOutputDir(const QString &dir);

private slots:
  void onBrowseClips();
  void onBrowseOutput();
  void onRenderClicked();
  void onCancelRender();
  void onRenderLog(const QString &html);
  void onRenderProgress(int percent);
  void onRenderFinished(bool success, const QString &msg);
  void onRenderError(const QString &msg);
  void onQualitySliderChanged(int value);

private:
  void setupUi();
  void setRendering(bool active);
  void applyRenderOptionsToWorker();

  QLineEdit *m_clipsFolderInput = nullptr;
  QPushButton *m_browseClipsBtn = nullptr;
  QLineEdit *m_outputPathInput = nullptr;
  QPushButton *m_browseOutputBtn = nullptr;

  QComboBox *m_resolutionCombo = nullptr;
  QComboBox *m_formatCombo = nullptr;
  QSpinBox *m_fpsSpin = nullptr;
  QSlider *m_qualitySlider = nullptr;
  QLabel *m_qualityValueLabel = nullptr;
  QCheckBox *m_hwAccelCheck = nullptr;
  QCheckBox *m_normalizeAudioCheck = nullptr;

  QPushButton *m_renderBtn = nullptr;
  QPushButton *m_cancelBtn = nullptr;
  QProgressBar *m_progressBar = nullptr;
  QLabel *m_statusLabel = nullptr;
  QTextEdit *m_logDisplay = nullptr;

  RenderWorker *m_worker = nullptr;
  QThread *m_thread = nullptr;
};

}

#endif