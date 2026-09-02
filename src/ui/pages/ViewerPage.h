#pragma once
#ifndef CAPSCRIPT_VIEWERPAGE_H
#define CAPSCRIPT_VIEWERPAGE_H

#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStringList>
#include <QTextBrowser>
#include <QWidget>

#ifdef CAPSCRIPT_WEBVIEW2
namespace CapScript {
class WebView2Widget;
}
#endif

#ifdef CAPSCRIPT_WEBVIEW
class QQuickWidget;
#endif

namespace CapScript {

class ViewerPage : public QWidget {
  Q_OBJECT

public:
  explicit ViewerPage(QWidget *parent = nullptr);

public slots:
  void updateViewer(const QStringList &results);
  void setOutputDir(const QString &dir);

  QString outputDir() const { return m_outputDir; }
  QStringList currentResults() const { return m_currentResults; }
  QString transcriptFilePath() const { return m_transcriptFile; }

signals:
  
  void resultsLoaded(const QStringList &results, const QString &outputDir);

private slots:
  void onAnchorClicked(const QUrl &url);
  void onLoadTranscript();
  void onExportTranscript();
  void onImportTranscript();
  void playVideoAtTimestamp(const QString &videoId, int seconds);

protected:
  void hideEvent(QHideEvent *event) override;

private:
  void closeVideoPlayer();
  void setupUi();
  void parseResultsToHtml(const QStringList &results);
  void repositionLoadButton();
  bool eventFilter(QObject *obj, QEvent *event) override;

  QSplitter *m_splitter = nullptr;
  QLabel *m_videoPlaceholder = nullptr;
  QTextBrowser *m_transcriptView = nullptr;
  QPushButton *m_exportBtn = nullptr;
  QPushButton *m_importBtn = nullptr;
  QPushButton *m_closeVideoBtn = nullptr;

#ifdef CAPSCRIPT_WEBVIEW
  QQuickWidget *m_quickWidget = nullptr;
#endif

#ifdef CAPSCRIPT_WEBVIEW2
  WebView2Widget *m_webView2Widget = nullptr;
#endif

  QString m_outputDir;
  QString m_transcriptFile;
  QStringList m_currentResults;
  QString m_currentVideoId;
  bool m_videoVisible = false;
};

}

#endif
