#pragma once
#ifndef CAPSCRIPT_MAINWINDOW_H
#define CAPSCRIPT_MAINWINDOW_H

#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace CapScript {

class TitleBar;
class SearchPage;
class ViewerPage;
class ClipDownloaderPage;
class RendererPage;
class ListCreatorPage;
class AboutPage;
class SupportPopup;
class AutoUpdater;
class MainWindow : public QWidget {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

protected:
  void closeEvent(QCloseEvent *e) override;
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;
  void changeEvent(QEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void showEvent(QShowEvent *e) override;

private slots:
  void onTabChanged(int index);
  void onSearchFinished(int count, const QStringList &results);
  void onManualCheckUpdates();
  void onUpdateAvailable(const QString &latestVersion,
                         const QString &releaseNotes);
  void onNoUpdateAvailable(const QString &message);
  void onUpdateError(const QString &message);
  void onDownloadProgress(int percent);
  void onDownloadFinished(const QString &zipPath, const QString &version);

private:
  void setupUi();
  void wireSignals();
  void saveGeometry();
  void restoreGeometryFromSettings();
  void applyWindowMask();

  TitleBar *m_titleBar = nullptr;

  QTabBar *m_tabBar = nullptr;
  QStackedWidget *m_pageStack = nullptr;

  SupportPopup *m_supportPopup = nullptr;
  bool m_supportShown = false;
  QTimer *m_supportTimer = nullptr;

  SearchPage *m_searchPage = nullptr;
  ViewerPage *m_viewerPage = nullptr;
  ClipDownloaderPage *m_clipDownloaderPage = nullptr;
  RendererPage *m_rendererPage = nullptr;
  ListCreatorPage *m_listCreatorPage = nullptr;
  AboutPage *m_aboutPage = nullptr;

  AutoUpdater *m_updater = nullptr;
  bool m_manualUpdateCheck = false;
  bool m_updateApplyOnExit = false;

  QWidget *m_centralContainer = nullptr;
  QVBoxLayout *m_rootLayout = nullptr;
};

}

#endif
