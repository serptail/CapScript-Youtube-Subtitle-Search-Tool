#include "ListCreatorPage.h"
#include "../../core/PythonBridge.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace CapScript {

ListCreatorPage::ListCreatorPage(QWidget *parent) : QWidget(parent) {
  m_netManager = new QNetworkAccessManager(this);
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

  auto *configGroup = new QGroupBox("Configuration");
  auto *configGrid = new QGridLayout(configGroup);
  configGrid->setSpacing(8);
  configGrid->setColumnStretch(1, 1);

  int row = 0;

  configGrid->addWidget(new QLabel("API Key:"), row, 0);
  m_apiKeyInput = new QLineEdit;
  m_apiKeyInput->setPlaceholderText("API key (auto-loaded from Search page)");
  m_apiKeyInput->setEchoMode(QLineEdit::Password);
  configGrid->addWidget(m_apiKeyInput, row, 1);
  row++;

  QString savedKey = PythonBridge::instance().loadApiKey();
  if (!savedKey.isEmpty())
    m_apiKeyInput->setText(savedKey);

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

  topRow->addWidget(configGroup, 1);

  auto *thumbGroup = new QGroupBox("Thumbnail Preview");
  auto *thumbLayout = new QVBoxLayout(thumbGroup);

  m_thumbnailLabel = new QLabel("Click a video\nto preview thumbnail");
  m_thumbnailLabel->setObjectName("thumbnailPreview");
  m_thumbnailLabel->setAlignment(Qt::AlignCenter);
  m_thumbnailLabel->setMinimumSize(240, 160);
  m_thumbnailLabel->setScaledContents(false);
  thumbLayout->addWidget(m_thumbnailLabel, 1);

  topRow->addWidget(thumbGroup, 1);

  root->addLayout(topRow, 0);

  auto *listWrapper = new QVBoxLayout;
  listWrapper->setSpacing(4);

  auto *headerRow = new QHBoxLayout;
  m_headerCheck = new QCheckBox("Select All");
  m_headerCheck->setChecked(false);
  headerRow->addWidget(m_headerCheck);
  headerRow->addStretch();
  listWrapper->addLayout(headerRow);

  m_resultsList = new QListWidget;
  m_resultsList->setSelectionMode(QAbstractItemView::NoSelection);
  m_resultsList->setMinimumHeight(120);
  listWrapper->addWidget(m_resultsList, 1);

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
  connect(m_resultsList, &QListWidget::itemClicked, this,
          &ListCreatorPage::onVideoClicked);
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
  QString apiKey = m_apiKeyInput->text().trimmed();
  if (apiKey.isEmpty()) {
    QMessageBox::warning(this, "Missing API Key",
                         "Please enter your YouTube Data API key.");
    return;
  }

  QString channelRaw = m_channelInput->text().trimmed();
  if (channelRaw.isEmpty()) {
    QMessageBox::warning(this, "Missing Channel",
                         "Please enter a Channel URL or ID.");
    return;
  }

  setFetching(true);
  m_resultsList->clear();
  m_resultsList->addItem("Resolving channel...");
  QApplication::processEvents();

  QString channel = extractChannelId(channelRaw);
  if (!channel.startsWith("UC")) {

    channel = PythonBridge::instance().resolveChannelId(apiKey, channel);
    if (channel.isEmpty()) {
      m_resultsList->clear();
      m_resultsList->addItem(
          "Could not resolve channel. Check the URL/handle.");
      m_countLabel->setText("0 videos");
      setFetching(false);
      return;
    }
  }

  m_resultsList->clear();
  m_resultsList->addItem("Fetching videos...");
  QApplication::processEvents();

  QJsonArray videos;

  bool isDateMode = (m_modeCombo->currentIndex() == 0);
  if (isDateMode) {
    QString startIso =
        m_startDate->date().startOfDay().toUTC().toString(Qt::ISODate);
    QString endIso =
        m_endDate->date().addDays(1).startOfDay().toUTC().toString(Qt::ISODate);
    videos = PythonBridge::instance().fetchVideosByChannelDate(
        apiKey, channel, startIso, endIso);
  } else {
    QString keyword = m_keywordInput->text().trimmed();
    if (keyword.isEmpty()) {
      QMessageBox::warning(this, "Missing Keyword",
                           "Please enter a keyword to search.");
      setFetching(false);
      return;
    }
    QString startIso =
        m_startDate->date().startOfDay().toUTC().toString(Qt::ISODate);
    QString endIso =
        m_endDate->date().addDays(1).startOfDay().toUTC().toString(Qt::ISODate);
    videos = PythonBridge::instance().searchVideosByKeyword(
        apiKey, channel, keyword, startIso, endIso);
  }

  m_resultsList->clear();

  if (videos.isEmpty()) {
    m_resultsList->addItem("No videos found.");
    m_countLabel->setText("0 videos");
    setFetching(false);
    return;
  }

  for (const auto &v : videos) {
    QJsonObject obj = v.toObject();
    QString id = obj["id"].toString();
    QString title = obj["title"].toString();

    auto *item = new QListWidgetItem(title);
    item->setData(Qt::UserRole, id);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    m_resultsList->addItem(item);
  }

  m_countLabel->setText(QString("%1 videos").arg(videos.size()));
  m_headerCheck->setChecked(false);
  setFetching(false);
}

void ListCreatorPage::onHeaderCheckToggled(bool checked) {
  for (int i = 0; i < m_resultsList->count(); ++i) {
    auto *item = m_resultsList->item(i);
    if (item->flags() & Qt::ItemIsUserCheckable)
      item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
  }
}

void ListCreatorPage::onVideoClicked(QListWidgetItem *item) {
  if (!item)
    return;
  QString videoId = item->data(Qt::UserRole).toString();
  if (!videoId.isEmpty())
    loadThumbnail(videoId);
}

void ListCreatorPage::onCopySelected() {
  QStringList ids;
  for (int i = 0; i < m_resultsList->count(); ++i) {
    auto *item = m_resultsList->item(i);
    if (item->checkState() == Qt::Checked) {
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
  for (int i = 0; i < m_resultsList->count(); ++i) {
    auto *item = m_resultsList->item(i);
    if (item->checkState() == Qt::Checked) {
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
  m_thumbnailLabel->setText("Loading...");
  m_thumbnailLabel->setPixmap(QPixmap());

  QUrl url(QString("https://img.youtube.com/vi/%1/mqdefault.jpg").arg(videoId));
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("Mozilla/5.0"));
  m_netManager->get(req);
}

void ListCreatorPage::onThumbnailLoaded(QNetworkReply *reply) {
  if (reply->error() == QNetworkReply::NoError) {
    QByteArray data = reply->readAll();
    QPixmap pix;
    if (pix.loadFromData(data)) {
      QPixmap scaled = pix.scaled(m_thumbnailLabel->size(), Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
      m_thumbnailLabel->setPixmap(scaled);
      m_thumbnailLabel->setText("");
    } else {
      m_thumbnailLabel->setText("Error loading image");
    }
  } else {
    m_thumbnailLabel->setText("Network error");
  }
  reply->deleteLater();
}

void ListCreatorPage::setFetching(bool active) {
  m_fetchBtn->setEnabled(!active);
  m_fetchBtn->setText(active ? "Fetching..." : "Fetch Videos");
}

}
