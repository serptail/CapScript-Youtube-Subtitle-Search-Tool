#pragma once
#ifndef CAPSCRIPT_LISTCREATORPAGE_H
#define CAPSCRIPT_LISTCREATORPAGE_H
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFutureWatcher>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

namespace CapScript {

class ListCreatorPage : public QWidget {
  Q_OBJECT

public:
  explicit ListCreatorPage(QWidget *parent = nullptr);

private slots:
  void onFetchClicked();
  void onCopySelected();
  void onExportTxt();
  void onHeaderCheckToggled(bool checked);
  void onVideoClicked(QListWidgetItem *item);
  void onModeChanged(int index);
  void onThumbnailLoaded(QNetworkReply *reply);

private:
  void setupUi();
  void setFetching(bool active);
  void loadThumbnail(const QString &videoId);
  void applyThumbnailProxySettings(const QString &proxyType,
                                   const QString &proxyUsername,
                                   const QString &proxyPassword,
                                   const QString &proxyUrl);
  QStringList buildThumbnailCandidates(const QString &videoId) const;
  void requestNextThumbnailCandidate();
  void showThumbnailPixmap(const QPixmap &pixmap);

  QLineEdit *m_channelInput = nullptr;
  QComboBox *m_modeCombo = nullptr;

  QWidget *m_datePanel = nullptr;
  QDateEdit *m_startDate = nullptr;
  QDateEdit *m_endDate = nullptr;

  QWidget *m_keywordPanel = nullptr;
  QLineEdit *m_keywordInput = nullptr;

  QLabel *m_thumbnailLabel = nullptr;

  QPushButton *m_fetchBtn = nullptr;
  QLabel *m_countLabel = nullptr;

  QCheckBox *m_headerCheck = nullptr;
  QListWidget *m_resultsList = nullptr;

  QPushButton *m_copyBtn = nullptr;
  QPushButton *m_exportBtn = nullptr;

  QNetworkAccessManager *m_netManager = nullptr;
  QNetworkReply *m_pendingThumbReply = nullptr;
  QFutureWatcher<QJsonArray> *m_fetchWatcher = nullptr;
  QString m_activeThumbVideoId;
  QStringList m_thumbCandidates;
  int m_thumbCandidateIndex = 0;
  QHash<QString, QPixmap> m_thumbnailCache;

  QString m_thumbProxyType;
  QString m_thumbProxyUsername;
  QString m_thumbProxyPassword;
  QString m_thumbProxyUrl;
};

}

#endif
