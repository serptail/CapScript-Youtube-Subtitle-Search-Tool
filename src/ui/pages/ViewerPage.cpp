#include "ViewerPage.h"
#include "../../core/UrlLauncher.h"
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSplitter>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef CAPSCRIPT_WEBVIEW2
#include "../widgets/WebView2Widget.h"
#endif

#ifdef CAPSCRIPT_WEBVIEW
#include <QQuickItem>
#include <QQuickWidget>

#endif

namespace CapScript {

ViewerPage::ViewerPage(QWidget *parent) : QWidget(parent) { setupUi(); }

void ViewerPage::closeVideoPlayer() {
#ifdef CAPSCRIPT_WEBVIEW2
  if (m_webView2Widget) {
    m_webView2Widget->stop();
    m_webView2Widget->hide();
  }
#elif defined(CAPSCRIPT_WEBVIEW)
  if (m_quickWidget) {
    if (auto *root = m_quickWidget->rootObject())
      QMetaObject::invokeMethod(root, "stop");
    m_quickWidget->hide();
  }
#endif

  if (m_closeVideoBtn)
    m_closeVideoBtn->hide();
  if (m_videoPlaceholder)
    m_videoPlaceholder->show();

  m_videoVisible = false;
  m_currentVideoId.clear();
}

void ViewerPage::setupUi() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(24, 16, 24, 16);
  rootLayout->setSpacing(10);

  auto *videoContainer = new QWidget;
  videoContainer->setObjectName("videoContainer");
  auto *videoLayout = new QVBoxLayout(videoContainer);
  videoLayout->setContentsMargins(0, 0, 0, 0);
  videoLayout->setSpacing(0);

#if defined(CAPSCRIPT_WEBVIEW2) || defined(CAPSCRIPT_WEBVIEW)
  m_videoPlaceholder =
      new QLabel("\u25b6  Click a timestamp to play video");
#else
  m_videoPlaceholder = new QLabel(
      "\u25b6  Click a timestamp to open video in browser");
#endif
  m_videoPlaceholder->setObjectName("videoPlaceholder");
  m_videoPlaceholder->setAlignment(Qt::AlignCenter);
  m_videoPlaceholder->setWordWrap(true);
  m_videoPlaceholder->setMinimumHeight(80);

#ifdef CAPSCRIPT_WEBVIEW2
  m_webView2Widget = new WebView2Widget;
  m_webView2Widget->setMinimumHeight(240);
  m_webView2Widget->hide();

  m_closeVideoBtn = new QPushButton("\u2715", m_webView2Widget);
  m_closeVideoBtn->setFixedSize(28, 28);
  m_closeVideoBtn->setStyleSheet(
      "QPushButton { background: rgba(0,0,0,0.7); color: #f0f0f0; "
      "border: 1px solid #333; border-radius: 14px; font-size: 12pt; "
      "font-weight: 700; }"
      "QPushButton:hover { background: #FF0033; border-color: #FF0033; }");
  m_closeVideoBtn->setCursor(Qt::PointingHandCursor);
  m_closeVideoBtn->setToolTip("Close video player");
  m_closeVideoBtn->hide();

  connect(m_closeVideoBtn, &QPushButton::clicked, this,
          [this]() { closeVideoPlayer(); });

  videoLayout->addWidget(m_webView2Widget);
#elif defined(CAPSCRIPT_WEBVIEW)
  m_quickWidget = new QQuickWidget;
  m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_quickWidget->setSource(QUrl("qrc:/qml/YouTubePlayer.qml"));
  m_quickWidget->setMinimumHeight(240);
  m_quickWidget->hide();

  m_closeVideoBtn = new QPushButton("\u2715", m_quickWidget);
  m_closeVideoBtn->setFixedSize(28, 28);
  m_closeVideoBtn->setStyleSheet(
      "QPushButton { background: rgba(0,0,0,0.7); color: #f0f0f0; "
      "border: 1px solid #333; border-radius: 14px; font-size: 12pt; "
      "font-weight: 700; }"
      "QPushButton:hover { background: #FF0033; border-color: #FF0033; }");
  m_closeVideoBtn->setCursor(Qt::PointingHandCursor);
  m_closeVideoBtn->setToolTip("Close video player");
  m_closeVideoBtn->hide();

  connect(m_closeVideoBtn, &QPushButton::clicked, this, [this]() {
    if (auto *root = m_quickWidget->rootObject())
      QMetaObject::invokeMethod(root, "stop");
    m_quickWidget->hide();
    m_closeVideoBtn->hide();
    m_videoPlaceholder->show();
    m_videoVisible = false;
  });

  videoLayout->addWidget(m_quickWidget);
#endif

  videoLayout->addWidget(m_videoPlaceholder);

  m_transcriptView = new QTextBrowser;
  m_transcriptView->setObjectName("viewerDisplay");
  m_transcriptView->setOpenExternalLinks(false);
  m_transcriptView->setOpenLinks(false);
  m_transcriptView->setPlaceholderText(
      "Transcript results will appear here after a search...");

  m_splitter = new QSplitter(Qt::Vertical);
  m_splitter->addWidget(videoContainer);
  m_splitter->addWidget(m_transcriptView);
  m_splitter->setStretchFactor(0, 2);
  m_splitter->setStretchFactor(1, 3);
  m_splitter->setHandleWidth(4);

  rootLayout->addWidget(m_splitter, 1);

  m_exportBtn = new QPushButton(m_transcriptView);
  m_exportBtn->setIcon(QIcon(":/icons/export.svg"));
  m_exportBtn->setIconSize(QSize(16, 16));
  m_exportBtn->setFixedSize(32, 32);
  m_exportBtn->setObjectName("ghostBtn");
  m_exportBtn->setCursor(Qt::PointingHandCursor);
  m_exportBtn->setToolTip("Export transcript");
  m_exportBtn->setStyleSheet(
      "QPushButton { border: none; background: rgba(30,30,30,0.70); "
      "border-radius: 4px; "
      "padding: 0px; min-width:32px; max-width:32px; min-height:32px; "
      "max-height:32px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.12); }");
  m_exportBtn->raise();

  m_importBtn = new QPushButton(m_transcriptView);
  m_importBtn->setIcon(QIcon(":/icons/import.svg"));
  m_importBtn->setIconSize(QSize(16, 16));
  m_importBtn->setFixedSize(32, 32);
  m_importBtn->setObjectName("ghostBtn");
  m_importBtn->setCursor(Qt::PointingHandCursor);
  m_importBtn->setToolTip("Import transcript");
  m_importBtn->setStyleSheet(
      "QPushButton { border: none; background: rgba(30,30,30,0.70); "
      "border-radius: 4px; "
      "padding: 0px; min-width:32px; max-width:32px; min-height:32px; "
      "max-height:32px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.12); }");
  m_importBtn->raise();

  m_transcriptView->installEventFilter(this);

  connect(m_transcriptView, &QTextBrowser::anchorClicked, this,
          &ViewerPage::onAnchorClicked);
  connect(m_exportBtn, &QPushButton::clicked, this,
          &ViewerPage::onExportTranscript);
  connect(m_importBtn, &QPushButton::clicked, this,
          &ViewerPage::onImportTranscript);
}

void ViewerPage::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
  closeVideoPlayer();
}

void ViewerPage::repositionLoadButton() {
  if (!m_exportBtn || !m_importBtn || !m_transcriptView)
    return;
  const int marginRight = 8;
  const int marginTop = 8;
  const int buttonSpacing = 4;

  int rightX = m_transcriptView->width() - marginRight;

  m_exportBtn->move(rightX - m_exportBtn->width(), marginTop);
  m_exportBtn->raise();

  rightX -= m_exportBtn->width() + buttonSpacing;
  m_importBtn->move(rightX - m_importBtn->width(), marginTop);
  m_importBtn->raise();

#ifdef CAPSCRIPT_WEBVIEW2
  if (m_closeVideoBtn && m_webView2Widget && m_webView2Widget->isVisible()) {
    m_closeVideoBtn->move(m_webView2Widget->width() - 36, 8);
    m_closeVideoBtn->raise();
  }
#elif defined(CAPSCRIPT_WEBVIEW)
  if (m_closeVideoBtn && m_quickWidget && m_quickWidget->isVisible()) {
    m_closeVideoBtn->move(m_quickWidget->width() - 36, 8);
    m_closeVideoBtn->raise();
  }
#endif
}

bool ViewerPage::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_transcriptView && event->type() == QEvent::Resize)
    repositionLoadButton();
  return QWidget::eventFilter(obj, event);
}

void ViewerPage::updateViewer(const QStringList &results) {
  closeVideoPlayer();

  m_currentResults = results;
  parseResultsToHtml(results);
#if defined(CAPSCRIPT_WEBVIEW2) || defined(CAPSCRIPT_WEBVIEW)
  m_videoPlaceholder->setText(
      QString(
          "\u25b6  %1 result block%2 found  |  Click a timestamp to play video")
          .arg(results.size())
          .arg(results.size() == 1 ? "" : "s"));
#else
  m_videoPlaceholder->setText(
      QString("\u25b6  %1 result block%2 found  |  Click a timestamp to open "
              "video in browser")
          .arg(results.size())
          .arg(results.size() == 1 ? "" : "s"));
#endif

  emit resultsLoaded(results, m_outputDir);
}

void ViewerPage::setOutputDir(const QString &dir) { m_outputDir = dir; }

void ViewerPage::onLoadTranscript() {
  QString path =
      QFileDialog::getOpenFileName(this, "Open Transcript File", m_outputDir,
                                   "Text Files (*.txt);;All Files (*)");
  if (path.isEmpty())
    return;

  closeVideoPlayer();

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  m_transcriptFile = path;
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

  m_currentResults = cleanBlocks;
  parseResultsToHtml(cleanBlocks);
#if defined(CAPSCRIPT_WEBVIEW2) || defined(CAPSCRIPT_WEBVIEW)
  m_videoPlaceholder->setText(
      QString("\u25b6  Loaded %1 video block%2 from file")
          .arg(cleanBlocks.size())
          .arg(cleanBlocks.size() == 1 ? "" : "s"));
#else
  m_videoPlaceholder->setText(
      QString("\u25b6  Loaded %1 video block%2 from file  |  Click a timestamp "
              "to open video in browser")
          .arg(cleanBlocks.size())
          .arg(cleanBlocks.size() == 1 ? "" : "s"));
#endif

  emit resultsLoaded(cleanBlocks, m_outputDir);
}

void ViewerPage::onExportTranscript() {
  if (m_currentResults.isEmpty()) {

    return;
  }

  QString path =
      QFileDialog::getSaveFileName(this, "Export Transcript", m_outputDir,
                                   "Text Files (*.txt);;All Files (*)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return;

  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);

  for (int i = 0; i < m_currentResults.size(); ++i) {
    out << m_currentResults[i];
    if (i < m_currentResults.size() - 1) {
      out << "\n"
          << QString(40, QChar(0x2550))
          << "\n";
    }
  }

  file.close();
}

void ViewerPage::onImportTranscript() { onLoadTranscript(); }

void ViewerPage::parseResultsToHtml(const QStringList &results) {
  QString html;
  html += "<style>"
          "body { font-family: 'Segoe UI', sans-serif; color: #f0f0f0; }"
          "a { color: #FF0033; text-decoration: none; font-weight: 600; }"
          "a:hover { text-decoration: underline; }"
          ".video-block { margin-bottom: 18px; padding-bottom: 12px; "
          "  border-bottom: 1px solid #2a2a2a; }"
          ".video-title { font-size: 12pt; font-weight: 700; color: #f0f0f0; "
          "  margin: 0 0 4px 0; }"
          ".video-meta { color: #888; font-size: 9pt; margin: 2px 0; }"
          ".timestamp-row { margin: 3px 0; }"
          ".ts-link { color: #FF0033; font-weight: 600; cursor: pointer; }"
          ".ts-text { color: #ccc; }"
          "</style>";

  static QRegularExpression titleRx(R"(Video\s*Title:\s*(.+))");
  static QRegularExpression idRx(R"(Video\s*ID:\s*([a-zA-Z0-9_-]{11}))");
  static QRegularExpression channelRx(R"(Channel:\s*(.+))");
  static QRegularExpression dateRx(R"(Date:\s*(.+))");
  static QRegularExpression viewsRx(R"(Views:\s*(.+))");
  static QRegularExpression timestampRx(QStringLiteral(
      "[\\x{2573}\\x{D7}xX]\\s*(\\d{1,2}:\\d{2}:\\d{2})\\s*-\\s*(.*)"));

  for (const QString &block : results) {
    QStringList lines = block.split('\n');
    QString videoTitle, videoId, channel, date, views;
    QVector<QPair<QString, QString>> timestamps;

    for (const QString &rawLine : lines) {
      QString line = rawLine.trimmed();
      if (line.isEmpty())
        continue;

      if (line.contains(QChar(0x2550)))
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

      auto cm = channelRx.match(line);
      if (cm.hasMatch()) {
        channel = cm.captured(1).trimmed();
        continue;
      }

      auto dm = dateRx.match(line);
      if (dm.hasMatch()) {
        date = dm.captured(1).trimmed();
        continue;
      }

      auto vm = viewsRx.match(line);
      if (vm.hasMatch()) {
        views = vm.captured(1).trimmed();
        continue;
      }

      if (line.startsWith("Timestamps"))
        continue;

      auto tsm = timestampRx.match(line);
      if (tsm.hasMatch()) {
        timestamps.append(
            {tsm.captured(1).trimmed(), tsm.captured(2).trimmed()});
      }
    }

    if (videoId.isEmpty() && timestamps.isEmpty())
      continue;

    html += "<div class='video-block'>";

    if (!videoTitle.isEmpty()) {
      html += QString("<div class='video-title'>%1</div>")
                  .arg(videoTitle.toHtmlEscaped());
    }
    if (!channel.isEmpty() || !date.isEmpty() || !views.isEmpty()) {
      QStringList meta;
      if (!channel.isEmpty())
        meta << channel.toHtmlEscaped();
      if (!date.isEmpty())
        meta << date.toHtmlEscaped();
      if (!views.isEmpty())
        meta << (views + " views");
      html += QString("<div class='video-meta'>%1</div>").arg(meta.join(" | "));
    }

    for (const auto &ts : timestamps) {
      QStringList parts = ts.first.split(':');
      int seconds = 0;
      if (parts.size() == 3)
        seconds =
            parts[0].toInt() * 3600 + parts[1].toInt() * 60 + parts[2].toInt();

      QString ytUrl = QString("https://www.youtube.com/watch?v=%1&t=%2")
                          .arg(videoId)
                          .arg(seconds);
      html += QString("<div class='timestamp-row'>"
                      "<a href='%1' class='ts-link'>[%2]</a> "
                      "<span class='ts-text'>%3</span></div>")
                  .arg(ytUrl, ts.first, ts.second.toHtmlEscaped());
    }

    html += "</div>";
  }

  if (html.isEmpty() || results.isEmpty()) {
    html += "<p style='color: #888; text-align: center; margin-top: 40px;'>"
            "No transcript data to display</p>";
  }

  m_transcriptView->setHtml(html);
}

void ViewerPage::onAnchorClicked(const QUrl &url) {
  if (url.scheme() == "https" || url.scheme() == "http") {

    QString urlStr = url.toString();
    static QRegularExpression ytRx(
        R"(youtube\.com/watch\?v=([a-zA-Z0-9_-]{11})(?:.*[&?]t=(\d+))?)");
    auto match = ytRx.match(urlStr);

    if (match.hasMatch()) {
      QString videoId = match.captured(1);
      int seconds = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
      playVideoAtTimestamp(videoId, seconds);
    } else {
      openExternalUrl(url);
    }
  }
}

void ViewerPage::playVideoAtTimestamp(const QString &videoId, int seconds) {
#ifdef CAPSCRIPT_WEBVIEW2
  QString watchUrl = QString("https://www.youtube.com/watch?v=%1&t=%2")
                         .arg(videoId)
                         .arg(seconds);
  const QString webviewMissingText =
      "Edge WebView2 Engine was not found on this Windows system.\n"
      "Opening in your browser instead.\n"
      "More info: "
      "https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool";

  if (!m_webView2Widget || m_webView2Widget->isInitializationFailed()) {
    if (m_webView2Widget)
      m_webView2Widget->hide();
    if (m_closeVideoBtn)
      m_closeVideoBtn->hide();
    if (m_videoPlaceholder) {
      m_videoPlaceholder->setText(webviewMissingText);
      m_videoPlaceholder->show();
    }
    m_videoVisible = false;
    openExternalUrl(QUrl(watchUrl));
    return;
  }

  if (m_currentVideoId == videoId && m_videoVisible &&
      m_webView2Widget->isReady()) {

    QString js =
        QString(
            "document.querySelector('iframe').contentWindow.postMessage(JSON."
            "stringify({event: 'command', func: 'seekTo', args: [%1, true]}), "
            "'*');"
            "document.querySelector('iframe').contentWindow.postMessage(JSON."
            "stringify({event: 'command', func: 'playVideo', args: []}), '*');")
            .arg(seconds);
    m_webView2Widget->executeJavaScript(js);
    return;
  }

  m_currentVideoId = videoId;

  QString embedUrl =
      QString("https://www.youtube.com/embed/"
              "%1?enablejsapi=1&autoplay=1&start=%2&rel=0&modestbranding=1")
          .arg(videoId)
          .arg(seconds);

  QString html =
      QString(
          "<!DOCTYPE html>"
          "<html>"
          "<head>"
          "<meta name=\"referrer\" content=\"strict-origin-when-cross-origin\">"
          "<style>"
          "  body, html { margin: 0; padding: 0; width: 100%; height: 100%; "
          "overflow: hidden; background-color: transparent; border-radius: "
          "8px; }"
          "  iframe { width: 100%; height: 100%; border: none; border-radius: "
          "8px; }"
          "</style>"
          "</head>"
          "<body>"
          "  <iframe src=\"%1\" "
          "          frameborder=\"0\" "
          "          allow=\"accelerometer; autoplay; clipboard-write; "
          "encrypted-media; gyroscope; picture-in-picture\" "
          "          referrerpolicy=\"strict-origin-when-cross-origin\" "
          "          allowfullscreen>"
          "  </iframe>"
          "</body>"
          "</html>")
          .arg(embedUrl);

  m_videoPlaceholder->hide();
  m_webView2Widget->show();
  m_closeVideoBtn->show();
  m_videoVisible = true;

  int total = m_splitter->height();
  m_splitter->setSizes({total * 2 / 5, total * 3 / 5});

  m_webView2Widget->loadHtml(html);

  QTimer::singleShot(50, this, [this]() {
    if (m_closeVideoBtn && m_webView2Widget) {
      m_closeVideoBtn->move(m_webView2Widget->width() - 36, 8);
      m_closeVideoBtn->raise();
    }
  });

  if (m_webView2Widget && m_webView2Widget->isInitializationFailed()) {
    if (m_webView2Widget)
      m_webView2Widget->hide();
    if (m_closeVideoBtn)
      m_closeVideoBtn->hide();
    if (m_videoPlaceholder) {
      m_videoPlaceholder->setText(webviewMissingText);
      m_videoPlaceholder->show();
    }
    m_videoVisible = false;

    openExternalUrl(QUrl(watchUrl));
    return;
  }
#elif defined(CAPSCRIPT_WEBVIEW)

  QString embedUrl = QString("https://www.youtube.com/embed/"
                             "%1?autoplay=1&start=%2&rel=0&modestbranding=1")
                         .arg(videoId)
                         .arg(seconds);

  m_videoPlaceholder->hide();
  m_quickWidget->show();
  m_closeVideoBtn->show();
  m_videoVisible = true;

  int total = m_splitter->height();
  m_splitter->setSizes({total * 2 / 5, total * 3 / 5});

  if (auto *root = m_quickWidget->rootObject())
    root->setProperty("videoUrl", embedUrl);

  QTimer::singleShot(50, this, [this]() {
    if (m_closeVideoBtn && m_quickWidget) {
      m_closeVideoBtn->move(m_quickWidget->width() - 36, 8);
      m_closeVideoBtn->raise();
    }
  });
#else

  QString ytUrl = QString("https://www.youtube.com/watch?v=%1&t=%2")
                      .arg(videoId)
                      .arg(seconds);
  openExternalUrl(QUrl(ytUrl));
#endif
}

}
