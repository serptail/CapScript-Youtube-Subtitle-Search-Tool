#pragma once
#ifndef CAPSCRIPT_WEBVIEW2WIDGET_H
#define CAPSCRIPT_WEBVIEW2WIDGET_H

#include <QString>
#include <QWidget>

struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;

namespace CapScript {

class WebView2Widget : public QWidget {
  Q_OBJECT

public:
  explicit WebView2Widget(QWidget *parent = nullptr);
  ~WebView2Widget() override;

  void loadUrl(const QString &url);
  void loadHtml(const QString &html);
  void executeJavaScript(const QString &js);
  void stop();
  bool isReady() const { return m_webView != nullptr; }
  bool isInitializationFailed() const { return m_initFailed; }

  void onEnvironmentCreated(long result, ICoreWebView2Environment *environment);
  void onControllerCreated(long result, ICoreWebView2Controller *controller);

signals:
  void webViewReady();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void changeEvent(QEvent *event) override;
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;

private:
  void initializeWebView2();
  void updateControllerBounds();
  void syncControllerVisibility();

  ICoreWebView2Environment *m_environment = nullptr;
  ICoreWebView2Controller *m_controller = nullptr;
  ICoreWebView2 *m_webView = nullptr;

  void *m_loaderModule = nullptr;
  bool m_comInitialized = false;
  bool m_comReady = false;
  bool m_initFailed = false;
  bool m_shouldBeVisible = false;
  QString m_pendingUrl;
  QString m_pendingHtml;
};

}

#endif
