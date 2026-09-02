#include "ListCreatorPage.h"
#include "../../core/PythonBridge.h"
#include "../../core/Settings.h"
#include <QApplication>
#include <QAuthenticator>
#include <QClipboard>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace CapScript {

ListCreatorPage::ListCreatorPage(QWidget *parent) : QWidget(parent) {
  m_netManager = new QNetworkAccessManager(this);
  connect(m_netManager, &QNetworkAccessManager::proxyAuthenticationRequired,
          this,
          [this](const QNetworkProxy &, QAuthenticator *authenticator) {
            if (!authenticator)
              return;
            if (!m_thumbProxyUsername.isEmpty()) {
              authenticator->setUser(m_thumbProxyUsername);
              authenticator->setPassword(m_thumbProxyPassword);
            }
          });
  connect(m_netManager, &QNetworkAccessManager::finished, this,
          &ListCreatorPage::onThumbnailLoaded);
  setupUi();
}

void ListCreatorPage::setupUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 16, 24, 16);
  root->setSpacing(14);

  auto *topRow = new QHBoxLayout;
  topRow->setSpacing(14);

  auto *configPanel = new QWidget;
  auto *configPanelLayout = new QVBoxLayout(configPanel);
  configPanelLayout->setContentsMargins(0, 0, 0, 0);
  configPanelLayout->setSpacing(6);

  auto *configHeading = new QLabel("Configuration");
  configHeading->setStyleSheet(
      "font-size: 12px;"
      "font-weight: normal;"
      "color: #eeeeee;"
  );
  configPanelLayout->addWidget(configHeading);

  auto *configGroup = new QGroupBox();
  configGroup->setObjectName("listConfigGroup");
  
  auto *configGrid = new QGridLayout(configGroup);
  configGrid->setContentsMargins(12, 22, 12, 10);
  configGrid->setSpacing(8);
  configGrid->setColumnStretch(1, 1);

  int row = 0;

  configGrid->addWidget(new QLabel("Channel:"), row, 0);
  m_channelInput = new QLineEdit;
  m_channelInput->setPlaceholderText("Channel URL or UCxxxxxxxx ID...");
  configGrid->addWidget(m_channelInput, row, 1);
  row++;

  configGrid->addWidget(new QLabel("Mode:"), row, 0);
  m_modeCombo = new QComboBox;
  m_modeCombo->addItem("Date Range");
  m_modeCombo->addItem("Keyword");
  configGrid->addWidget(m_modeCombo, row, 1);
  row++;

  m_datePanel = new QWidget;
  auto *dateGrid = new QGridLayout(m_datePanel);
  dateGrid->setContentsMargins(0, 0, 0, 0);
  dateGrid->setSpacing(8);
  dateGrid->setColumnStretch(1, 1);

  dateGrid->addWidget(new QLabel("From:"), 0, 0);
  m_startDate = new QDateEdit;
  m_startDate->setCalendarPopup(true);
  m_startDate->setDate(QDate::currentDate().addYears(-1));
  dateGrid->addWidget(m_startDate, 0, 1);

  dateGrid->addWidget(new QLabel("To:"), 1, 0);
  m_endDate = new QDateEdit;
  m_endDate->setCalendarPopup(true);
  m_endDate->setDate(QDate::currentDate());
  dateGrid->addWidget(m_endDate, 1, 1);

  m_keywordPanel = new QWidget;
  auto *kwLayout = new QHBoxLayout(m_keywordPanel);
  kwLayout->setContentsMargins(0, 0, 0, 0);
  kwLayout->addWidget(new QLabel("Keyword:"));
  m_keywordInput = new QLineEdit;
  m_keywordInput->setPlaceholderText("Keyword in video title...");
  kwLayout->addWidget(m_keywordInput, 1);

  auto *modeStack = new QStackedWidget;
  modeStack->addWidget(m_datePanel);
  modeStack->addWidget(m_keywordPanel);
  configGrid->addWidget(modeStack, row, 0, 1, 2);
  row++;

  auto *fetchRow = new QHBoxLayout;
  m_fetchBtn = new QPushButton("Fetch Videos");
  m_fetchBtn->setMinimumWidth(140);
  m_fetchBtn->setMinimumHeight(34);
  fetchRow->addWidget(m_fetchBtn);
  fetchRow->addStretch();

  m_countLabel = new QLabel;
  m_countLabel->setStyleSheet("color: #888; font-size: 9pt;");
  fetchRow->addWidget(m_countLabel);

  configGrid->addLayout(fetchRow, row, 0, 1, 2);

  configPanelLayout->addWidget(configGroup, 1);

  topRow->addWidget(configPanel, 1);

  auto *thumbPanel = new QWidget;
  auto *thumbPanelLayout = new QVBoxLayout(thumbPanel);
  thumbPanelLayout->setContentsMargins(0, 0, 0, 0);
  thumbPanelLayout->setSpacing(6);

  auto *thumbHeading = new QLabel("Thumbnail Preview");
  thumbHeading->setStyleSheet(
      "font-size: 12px;"
      "font-weight: normal;"
      "color: #eeeeee;"
  );
  thumbPanelLayout->addWidget(thumbHeading);

  auto *thumbGroup = new QGroupBox();
  auto *thumbLayout = new QVBoxLayout(thumbGroup);

  m_thumbnailLabel = new QLabel("Click a video\nto preview thumbnail");
  m_thumbnailLabel->setObjectName("thumbnailPreview");
  m_thumbnailLabel->setAlignment(Qt::AlignCenter);
  m_thumbnailLabel->setMinimumSize(240, 160);
  m_thumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_thumbnailLabel->setScaledContents(false);
  m_thumbnailLabel->setFocusPolicy(Qt::NoFocus);
  thumbLayout->addWidget(m_thumbnailLabel, 1);

  thumbPanelLayout->addWidget(thumbGroup, 1);

  topRow->addWidget(thumbPanel, 1);

  root->addLayout(topRow, 0);

  auto *listWrapper = new QVBoxLayout;
  listWrapper->setSpacing(4);

  auto *headerRow = new QHBoxLayout;
  m_headerCheck = new QCheckBox("Select All");
  m_headerCheck->setChecked(false);
  headerRow->addWidget(m_headerCheck);
  headerRow->addStretch();
  listWrapper->addLayout(headerRow);

  m_resultsTable = new QTableWidget(0, 3);
  m_resultsTable->setObjectName("resultsTable");
  m_resultsTable->setHorizontalHeaderLabels({"", "Title", "Video ID"});
  m_resultsTable->setSelectionMode(QAbstractItemView::NoSelection);
  m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_resultsTable->setMinimumHeight(120);
  m_resultsTable->setShowGrid(false);
  m_resultsTable->verticalHeader()->setVisible(false);
  m_resultsTable->horizontalHeader()->setStretchLastSection(false);
  m_resultsTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Fixed);
  m_resultsTable->setColumnWidth(0, 30);
  m_resultsTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Stretch);
  m_resultsTable->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  listWrapper->addWidget(m_resultsTable, 1);

  root->addLayout(listWrapper, 1);

  auto *footerRow = new QHBoxLayout;
  footerRow->setSpacing(10);

  m_copyBtn = new QPushButton("Copy Selected IDs");
  m_copyBtn->setObjectName("secondaryBtn");
  m_copyBtn->setMinimumWidth(140);
  footerRow->addWidget(m_copyBtn);

  m_exportBtn = new QPushButton("Export to .TXT");
  m_exportBtn->setObjectName("secondaryBtn");
  m_exportBtn->setMinimumWidth(140);
  footerRow->addWidget(m_exportBtn);

  footerRow->addStretch();
  root->addLayout(footerRow, 0);

  connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ListCreatorPage::onModeChanged);
  connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          modeStack, &QStackedWidget::setCurrentIndex);
  connect(m_fetchBtn, &QPushButton::clicked, this,
          &ListCreatorPage::onFetchClicked);
  connect(m_headerCheck, &QCheckBox::toggled, this,
          &ListCreatorPage::onHeaderCheckToggled);
  connect(m_copyBtn, &QPushButton::clicked, this,
          &ListCreatorPage::onCopySelected);
  connect(m_exportBtn, &QPushButton::clicked, this,
          &ListCreatorPage::onExportTxt);
  connect(m_resultsTable, &QTableWidget::cellClicked, this,
          &ListCreatorPage::onVideoClicked);
}

static void showTableStatusRow(QTableWidget *table, const QString &message) {
  table->clearContents();
  table->clearSpans();
  table->setRowCount(1);
  auto *item = new QTableWidgetItem(message);
  item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  item->setTextAlignment(Qt::AlignCenter);
  table->setItem(0, 0, item);
  table->setSpan(0, 0, 1, 3);
}

static QString extractChannelId(const QString &input) {
  QString trimmed = input.trimmed();

  if (trimmed.startsWith("UC") && trimmed.length() >= 24)
    return trimmed;

  static QRegularExpression channelRe(
      R"(youtube\.com\/channel\/(UC[\w-]{22,}))");
  auto m1 = channelRe.match(trimmed);
  if (m1.hasMatch())
    return m1.captured(1);

  static QRegularExpression handleRe(R"(youtube\.com\/@([\w.-]+))");
  auto m2 = handleRe.match(trimmed);
  if (m2.hasMatch())
    return "@" + m2.captured(1);

  static QRegularExpression cRe(R"(youtube\.com\/c\/([\w.-]+))");
  auto m3 = cRe.match(trimmed);
  if (m3.hasMatch())
    return m3.captured(1);

  return trimmed;
}

void ListCreatorPage::onModeChanged(int index) { Q_UNUSED(index); }

void ListCreatorPage::onFetchClicked() {
  if (m_fetchWatcher && m_fetchWatcher->isRunning()) {
    return;
  }

  const QString channelRaw = m_channelInput->text().trimmed();
  if (channelRaw.isEmpty()) {
    QMessageBox::warning(this, "Missing Channel",
                         "Please enter a Channel URL or ID.");
    return;
  }

  const bool isDateMode = (m_modeCombo->currentIndex() == 0);
  const QString keyword = m_keywordInput->text().trimmed();
  if (!isDateMode && keyword.isEmpty()) {
    QMessageBox::warning(this, "Missing Keyword",
                         "Please enter a keyword to search.");
    return;
  }

  setFetching(true);
  showTableStatusRow(m_resultsTable, "Resolving channel...");

  const QString extractedChannel = extractChannelId(channelRaw);

  const QString cookiesFile = Settings::lastCookiesFile().trimmed();

  QString proxyType = "none";
  QString proxyUsername;
  QString proxyPassword;
  QString proxyUrl;
  {
    const QString proxyJson = PythonBridge::instance().loadProxySettings();
    const QJsonDocument doc = QJsonDocument::fromJson(proxyJson.toUtf8());
    if (doc.isObject()) {
      const QJsonObject obj = doc.object();
      proxyType = obj.value("type").toString("none");
      proxyUsername = obj.value("username").toString();
      proxyPassword = obj.value("password").toString();
      proxyUrl = obj.value("url").toString();
    }
  }

  if (proxyType == "none")
    proxyType.clear();

  applyThumbnailProxySettings(proxyType, proxyUsername, proxyPassword, proxyUrl);

  const QString startIso =
      m_startDate->date().startOfDay().toUTC().toString(Qt::ISODate);
  const QString endIso =
      m_endDate->date().addDays(1).startOfDay().toUTC().toString(Qt::ISODate);

  showTableStatusRow(m_resultsTable, "Resolving channel and fetching videos...");

  auto future = QtConcurrent::run([=]() -> QJsonArray {
    QStringList channelCandidates;
    auto addCandidate = [&channelCandidates](const QString &candidate) {
      const QString normalized = candidate.trimmed();
      if (normalized.isEmpty())
        return;
      if (!channelCandidates.contains(normalized))
        channelCandidates.append(normalized);
    };

    if (!extractedChannel.startsWith("UC")) {
      const QString resolved = PythonBridge::instance().resolveChannelId(
          "", extractedChannel, cookiesFile, QString(), proxyType,
          proxyUsername, proxyPassword, proxyUrl);
      if (!resolved.isEmpty()) {
        addCandidate(resolved);
      }
    }

    // Always include the extracted token and original user input as fallbacks.
    addCandidate(extractedChannel);
    addCandidate(channelRaw);

    if (channelCandidates.isEmpty()) {
      return QJsonArray();
    }

    auto fetchForChannel = [&](const QString &channelToken,
                               const QString &pType,
                               const QString &pUser,
                               const QString &pPass,
                               const QString &pUrl) {
      if (isDateMode) {
        return PythonBridge::instance().fetchVideosByChannelDate(
            "", channelToken, startIso, endIso, cookiesFile, QString(),
            pType, pUser, pPass, pUrl);
      }
      return PythonBridge::instance().searchVideosByKeyword(
          "", channelToken, keyword, startIso, endIso, cookiesFile, QString(),
          pType, pUser, pPass, pUrl);
    };

    for (const QString &candidate : channelCandidates) {
      QJsonArray videos =
          fetchForChannel(candidate, proxyType, proxyUsername, proxyPassword,
                          proxyUrl);
      if (!videos.isEmpty()) {
        return videos;
      }

      // If a proxy is configured but returns no results, retry direct once.
      if (!proxyType.isEmpty()) {
        videos = fetchForChannel(candidate, "", "", "", "");
        if (!videos.isEmpty()) {
          return videos;
        }
      }
    }
    return QJsonArray();
  });

  m_fetchWatcher = new QFutureWatcher<QJsonArray>(this);
  connect(m_fetchWatcher, &QFutureWatcher<QJsonArray>::finished, this,
          [this]() {
            if (!m_fetchWatcher) {
              return;
            }

            const QJsonArray videos = m_fetchWatcher->result();
            m_fetchWatcher->deleteLater();
            m_fetchWatcher = nullptr;

            if (videos.isEmpty()) {
              showTableStatusRow(
                  m_resultsTable,
                  "No videos found. Try a wider date range, a different "
                  "keyword, or check the channel ID.");
              m_countLabel->setText("0 videos");
              setFetching(false);
              return;
            }

            m_resultsTable->clearContents();
            m_resultsTable->clearSpans();
            m_resultsTable->setRowCount(videos.size());

            int row = 0;
            for (const auto &v : videos) {
              QJsonObject obj = v.toObject();
              QString id = obj["id"].toString();
              QString title = obj["title"].toString();

              auto *checkItem = new QTableWidgetItem;
              checkItem->setFlags(Qt::ItemIsUserCheckable |
                                  Qt::ItemIsEnabled);
              checkItem->setCheckState(Qt::Unchecked);
              checkItem->setData(Qt::UserRole, id);
        
              checkItem->setBackground(QColor("#111111"));
              m_resultsTable->setItem(row, 0, checkItem);

              auto *titleItem = new QTableWidgetItem(title);
              titleItem->setFlags(titleItem->flags() &
                                  ~Qt::ItemIsEditable);
              titleItem->setData(Qt::UserRole, id);
              titleItem->setBackground(QColor("#111111"));
              m_resultsTable->setItem(row, 1, titleItem);

              auto *idItem = new QTableWidgetItem(id);
              idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
              idItem->setForeground(QColor("#909090"));
              idItem->setBackground(QColor("#111111"));
              m_resultsTable->setItem(row, 2, idItem);

              row++;
            }

            m_countLabel->setText(QString("%1 videos").arg(videos.size()));
            m_headerCheck->setChecked(false);
            setFetching(false);
          });
  m_fetchWatcher->setFuture(future);
}

void ListCreatorPage::onHeaderCheckToggled(bool checked) {
  for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
    auto *item = m_resultsTable->item(row, 0);
    if (item && (item->flags() & Qt::ItemIsUserCheckable))
      item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
  }
}

void ListCreatorPage::onVideoClicked(int row, int column) {
  Q_UNUSED(column);
  auto *item = m_resultsTable->item(row, 0);
  if (!item)
    return;
  QString videoId = item->data(Qt::UserRole).toString();
  if (!videoId.isEmpty())
    loadThumbnail(videoId);
}

void ListCreatorPage::onCopySelected() {
  QStringList ids;
  for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
    auto *item = m_resultsTable->item(row, 0);
    if (item && item->checkState() == Qt::Checked) {
      QString id = item->data(Qt::UserRole).toString();
      if (!id.isEmpty())
        ids << id;
    }
  }
  if (ids.isEmpty()) {
    QMessageBox::information(this, "Copy IDs", "No videos are checked.");
    return;
  }
  QApplication::clipboard()->setText(ids.join(","));
  QMessageBox::information(
      this, "Copied",
      QString("Copied %1 video ID(s) to clipboard.").arg(ids.size()));
}

void ListCreatorPage::onExportTxt() {
  QStringList ids;
  for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
    auto *item = m_resultsTable->item(row, 0);
    if (item && item->checkState() == Qt::Checked) {
      QString id = item->data(Qt::UserRole).toString();
      if (!id.isEmpty())
        ids << id;
    }
  }
  if (ids.isEmpty()) {
    QMessageBox::information(this, "Export", "No videos are checked.");
    return;
  }

  QString path = QFileDialog::getSaveFileName(
      this, "Export Video IDs", "video_ids.txt", "Text Files (*.txt)");
  if (path.isEmpty())
    return;

  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    f.write(ids.join(",").toUtf8());
    f.close();
    QMessageBox::information(
        this, "Exported",
        QString("Exported %1 IDs to %2").arg(ids.size()).arg(path));
  } else {
    QMessageBox::warning(this, "Error", "Could not write to file.");
  }
}

void ListCreatorPage::loadThumbnail(const QString &videoId) {
  if (videoId.isEmpty())
    return;

  if (m_thumbnailCache.contains(videoId)) {
    showThumbnailPixmap(m_thumbnailCache.value(videoId));
    return;
  }

  if (m_pendingThumbReply) {
    m_pendingThumbReply->abort();
    m_pendingThumbReply->deleteLater();
    m_pendingThumbReply = nullptr;
  }

  m_activeThumbVideoId = videoId;
  m_thumbCandidates = buildThumbnailCandidates(videoId);
  m_thumbCandidateIndex = 0;

  m_thumbnailLabel->setText("Loading...");
  m_thumbnailLabel->setPixmap(QPixmap());

  requestNextThumbnailCandidate();
}

void ListCreatorPage::applyThumbnailProxySettings(const QString &proxyType,
                                                  const QString &proxyUsername,
                                                  const QString &proxyPassword,
                                                  const QString &proxyUrl) {
  m_thumbProxyType = proxyType.trimmed().toLower();
  m_thumbProxyUsername = proxyUsername;
  m_thumbProxyPassword = proxyPassword;
  m_thumbProxyUrl = proxyUrl.trimmed();

  if (m_thumbProxyType.isEmpty() || m_thumbProxyType == "none") {
    m_netManager->setProxy(QNetworkProxy::NoProxy);
    return;
  }

  QString normalizedUrl = m_thumbProxyUrl;
  if (normalizedUrl.isEmpty() && m_thumbProxyType == "webshare")
    normalizedUrl = "http://proxy.webshare.io:80";
  if (!normalizedUrl.contains("://"))
    normalizedUrl.prepend("http://");

  const QUrl parsed(normalizedUrl);
  if (!parsed.isValid() || parsed.host().isEmpty()) {
    m_netManager->setProxy(QNetworkProxy::NoProxy);
    return;
  }

  QNetworkProxy::ProxyType qtProxyType = QNetworkProxy::HttpProxy;
  const QString scheme = parsed.scheme().trimmed().toLower();
  if (scheme.startsWith("socks"))
    qtProxyType = QNetworkProxy::Socks5Proxy;

  QNetworkProxy proxy(qtProxyType, parsed.host(),
                      static_cast<quint16>(parsed.port(80)));

  QString user = parsed.userName(QUrl::FullyDecoded);
  QString pass = parsed.password(QUrl::FullyDecoded);
  if (user.isEmpty())
    user = m_thumbProxyUsername;
  if (pass.isEmpty())
    pass = m_thumbProxyPassword;
  proxy.setUser(user);
  proxy.setPassword(pass);

  m_thumbProxyUsername = user;
  m_thumbProxyPassword = pass;

  m_netManager->setProxy(proxy);
}

QStringList ListCreatorPage::buildThumbnailCandidates(const QString &videoId) const {
  QStringList urls;
  auto add = [&urls](const QString &url) {
    if (!urls.contains(url))
      urls << url;
  };

  add(QString("https://i.ytimg.com/vi/%1/maxresdefault.jpg").arg(videoId));
  add(QString("https://i.ytimg.com/vi_webp/%1/maxresdefault.webp").arg(videoId));
  add(QString("https://i.ytimg.com/vi/%1/sddefault.jpg").arg(videoId));
  add(QString("https://i.ytimg.com/vi/%1/hqdefault.jpg").arg(videoId));
  add(QString("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(videoId));
  add(QString("https://img.youtube.com/vi/%1/mqdefault.jpg").arg(videoId));

  return urls;
}

void ListCreatorPage::requestNextThumbnailCandidate() {
  if (m_activeThumbVideoId.isEmpty()) {
    m_thumbnailLabel->setText("No thumbnail selected");
    return;
  }

  if (m_thumbCandidateIndex >= m_thumbCandidates.size()) {
    m_thumbnailLabel->setText("Thumbnail unavailable");
    return;
  }

  const QString url = m_thumbCandidates.at(m_thumbCandidateIndex++);
  QNetworkRequest req{QUrl(url)};
  req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("Mozilla/5.0"));
  req.setRawHeader("Accept",
                   "image/avif,image/webp,image/apng,image/*,*/*;q=0.8");
  req.setRawHeader("Referer",
                   QByteArray("https://www.youtube.com/watch?v=") +
                       m_activeThumbVideoId.toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  req.setTransferTimeout(8000);
#endif
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);

  m_pendingThumbReply = m_netManager->get(req);
  m_pendingThumbReply->setProperty("video_id", m_activeThumbVideoId);
}

void ListCreatorPage::showThumbnailPixmap(const QPixmap &pixmap) {
  if (pixmap.isNull())
    return;

  QPixmap scaled = pixmap.scaled(m_thumbnailLabel->size(), Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
  m_thumbnailLabel->setPixmap(scaled);
  m_thumbnailLabel->setText("");
}

void ListCreatorPage::onThumbnailLoaded(QNetworkReply *reply) {
  if (!reply)
    return;

  if (reply != m_pendingThumbReply) {
    reply->deleteLater();
    return;
  }

  m_pendingThumbReply = nullptr;
  const QString replyVideoId = reply->property("video_id").toString();
  if (replyVideoId.isEmpty() || replyVideoId != m_activeThumbVideoId) {
    reply->deleteLater();
    return;
  }

  const bool ok = (reply->error() == QNetworkReply::NoError);
  const QByteArray data = reply->readAll();
  reply->deleteLater();

  QPixmap pix;
  const bool loaded = ok && pix.loadFromData(data);
  const bool tiny = loaded && pix.width() <= 120 && pix.height() <= 90;

  if (loaded && !tiny) {
    m_thumbnailCache.insert(replyVideoId, pix);
    showThumbnailPixmap(pix);
    return;
  }

  if (loaded && tiny && m_thumbCandidateIndex >= m_thumbCandidates.size()) {
    m_thumbnailCache.insert(replyVideoId, pix);
    showThumbnailPixmap(pix);
    return;
  }

  requestNextThumbnailCandidate();
}

void ListCreatorPage::setFetching(bool active) {
  m_fetchBtn->setEnabled(!active);
  m_fetchBtn->setText(active ? "Fetching..." : "Fetch Videos");
}

}