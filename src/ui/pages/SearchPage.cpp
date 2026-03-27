#include "SearchPage.h"
#include "../../core/PythonBridge.h"
#include "../../core/Settings.h"
#include "../../workers/SearchWorker.h"
#include "../widgets/FeedbackWidget.h"
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace CapScript {

SearchPage::SearchPage(QWidget *parent) : QWidget(parent) { setupUi(); }

SearchPage::~SearchPage() {
  if (m_thread && m_thread->isRunning()) {
    if (m_worker)
      m_worker->requestStop();
    m_thread->quit();
    m_thread->wait(3000);
  }
}

static QString extractChannelId(const QString &input) {
  QString trimmed = input.trimmed();

  if (trimmed.startsWith("UC") && trimmed.length() >= 24)
    return trimmed;

  static QRegularExpression channelRx(
      R"(youtube\.com\/channel\/(UC[\w-]{22,}))");
  auto m = channelRx.match(trimmed);
  if (m.hasMatch())
    return m.captured(1);

  static QRegularExpression handleRx(R"(youtube\.com\/@([\w.-]+))");
  auto hm = handleRx.match(trimmed);
  if (hm.hasMatch())
    return "@" + hm.captured(1);

  static QRegularExpression cRx(R"(youtube\.com\/c\/([\w.-]+))");
  auto cm = cRx.match(trimmed);
  if (cm.hasMatch())
    return cm.captured(1);

  if (trimmed.startsWith("@"))
    return trimmed;

  return trimmed;
}

void SearchPage::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  positionFeedbackBtn();
}

void SearchPage::showEvent(QShowEvent *e) {
  QWidget::showEvent(e);
  positionFeedbackBtn();
}

void SearchPage::positionFeedbackBtn() {
  if (!m_feedbackBtn)
    return;
  const int margin = 10;
  m_feedbackBtn->move(margin, height() - m_feedbackBtn->height() - margin);
  m_feedbackBtn->raise();
}

void SearchPage::setupUi() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(24, 12, 24, 8);
  rootLayout->setSpacing(10);

  m_splitter = new QSplitter(Qt::Horizontal);

  auto *leftPanel = new QWidget;
  auto *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 16, 0);
  leftLayout->setSpacing(6);

  auto *apiGroup = new QGroupBox("API Key");
  auto *apiLayout = new QHBoxLayout(apiGroup);
  apiLayout->setContentsMargins(10, 10, 10, 10);
  apiLayout->setSpacing(6);

  m_apiKeyInput = new QLineEdit;
  m_apiKeyInput->setPlaceholderText("Enter YouTube Data API v3 key...");
  m_apiKeyInput->setEchoMode(QLineEdit::Password);

  QAction *pasteAction = m_apiKeyInput->addAction(QIcon(":/icons/paste.svg"),
                                                  QLineEdit::TrailingPosition);
  pasteAction->setToolTip("Paste from clipboard");
  connect(pasteAction, &QAction::triggered, this, &SearchPage::onPasteApiKey);

  m_apiToggleBtn = new QPushButton;
  m_apiToggleBtn->setObjectName("ghostBtn");
  m_apiToggleBtn->setIcon(QIcon(":/icons/show.svg"));
  m_apiToggleBtn->setIconSize(QSize(16, 16));
  m_apiToggleBtn->setFixedSize(30, 28);
  m_apiToggleBtn->setToolTip("Show/hide API key");
  m_apiToggleBtn->setCursor(Qt::PointingHandCursor);

  m_apiInfoBtn = new QPushButton;
  m_apiInfoBtn->setObjectName("ghostBtn");
  m_apiInfoBtn->setIcon(QIcon(":/icons/tooltip.svg"));
  m_apiInfoBtn->setIconSize(QSize(16, 16));
  m_apiInfoBtn->setFixedSize(30, 28);
  m_apiInfoBtn->setCursor(Qt::PointingHandCursor);
  m_apiInfoBtn->setToolTip("YouTube Data API v3 Key\n\n"
                           "Get your free API key from Google Cloud:\n"
                           "1. Go to Google Cloud Console\n"
                           "2. Create a new project\n"
                           "3. Enable YouTube Data API v3\n"
                           "4. Create an API key credential\n"
                           "5. Paste it here\n\n"
                           "https://console.cloud.google.com");

  apiLayout->addWidget(m_apiKeyInput, 1);
  apiLayout->addWidget(m_apiToggleBtn);
  apiLayout->addWidget(m_apiInfoBtn);

  leftLayout->addWidget(apiGroup);

  QString savedKey = PythonBridge::instance().loadApiKey();
  if (!savedKey.isEmpty())
    m_apiKeyInput->setText(savedKey);

  auto *modeGroup = new QGroupBox("Search Mode");
  auto *modeLayout = new QVBoxLayout(modeGroup);
  modeLayout->setSpacing(8);

  auto *radioRow = new QHBoxLayout;
  radioRow->addStretch();
  m_videoRadio = new QRadioButton("Video");
  m_channelRadio = new QRadioButton("Channel");
  m_videoRadio->setChecked(true);

  auto *radioGroup = new QButtonGroup(this);
  radioGroup->addButton(m_videoRadio);
  radioGroup->addButton(m_channelRadio);

  radioRow->addWidget(m_videoRadio);
  radioRow->addSpacing(20);
  radioRow->addWidget(m_channelRadio);
  radioRow->addStretch();
  modeLayout->addLayout(radioRow);

  m_modeStack = new QStackedWidget;
  m_modeStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  auto *videoPanel = new QWidget;
  videoPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto *videoGrid = new QGridLayout(videoPanel);
  videoGrid->setSpacing(8);
  videoGrid->setColumnStretch(1, 1);
  videoGrid->setContentsMargins(0, 4, 0, 0);

  int row = 0;
  videoGrid->addWidget(new QLabel("Video IDs / URLs:"), row, 0);
  m_videoIdsInput = new QLineEdit;
  m_videoIdsInput->setPlaceholderText(
      "IDs, URLs (comma-separated), or path to file...");
  videoGrid->addWidget(m_videoIdsInput, row, 1);
  row++;

  videoGrid->addWidget(new QLabel("Search Term:"), row, 0);
  m_videoSearchTerm = new QLineEdit;
  m_videoSearchTerm->setPlaceholderText(
      "Word or phrase to find in transcripts...");
  videoGrid->addWidget(m_videoSearchTerm, row, 1);
  row++;

  videoGrid->addWidget(new QLabel("Language:"), row, 0);
  m_videoLangCombo = new QComboBox;
  m_videoLangCombo->addItems(
      {"en", "es", "fr", "de", "it", "pt", "ja", "ko", "zh", "ar", "ru", "hi"});
  m_videoLangCombo->setEditable(true);
  m_videoLangCombo->setCurrentText("en");
  videoGrid->addWidget(m_videoLangCombo, row, 1);

  m_modeStack->addWidget(videoPanel);

  auto *channelPanel = new QWidget;
  channelPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto *channelGrid = new QGridLayout(channelPanel);
  channelGrid->setSpacing(8);
  channelGrid->setColumnStretch(1, 1);
  channelGrid->setContentsMargins(0, 4, 0, 0);

  row = 0;
  channelGrid->addWidget(new QLabel("Channel URL / ID:"), row, 0);
  m_channelInput = new QLineEdit;
  m_channelInput->setPlaceholderText("Channel URL or UCxxxxxxxx ID...");
  channelGrid->addWidget(m_channelInput, row, 1);
  row++;

  channelGrid->addWidget(new QLabel("Search Term:"), row, 0);
  m_channelSearchTerm = new QLineEdit;
  m_channelSearchTerm->setPlaceholderText(
      "Word or phrase to find in transcripts...");
  channelGrid->addWidget(m_channelSearchTerm, row, 1);
  row++;

  channelGrid->addWidget(new QLabel("Language:"), row, 0);
  m_channelLangCombo = new QComboBox;
  m_channelLangCombo->addItems(
      {"en", "es", "fr", "de", "it", "pt", "ja", "ko", "zh", "ar", "ru", "hi"});
  m_channelLangCombo->setEditable(true);
  m_channelLangCombo->setCurrentText("en");
  channelGrid->addWidget(m_channelLangCombo, row, 1);
  row++;

  auto *maxResRow = new QHBoxLayout;
  channelGrid->addWidget(new QLabel("Max Results:"), row, 0);
  m_maxResultsSpin = new QSpinBox;
  m_maxResultsSpin->setRange(1, 10000);
  m_maxResultsSpin->setValue(10);

  auto *maxResTooltip = new QPushButton;
  maxResTooltip->setObjectName("ghostBtn");
  maxResTooltip->setIcon(QIcon(":/icons/tooltip.svg"));
  maxResTooltip->setIconSize(QSize(16, 16));
  maxResTooltip->setFixedSize(24, 24);
  maxResTooltip->setToolTip("Fetches the first N most recent videos\n"
                            "from the channel and searches each\n"
                            "for the desired term.");
  maxResTooltip->setCursor(Qt::WhatsThisCursor);

  maxResRow->addWidget(m_maxResultsSpin, 1);
  maxResRow->addWidget(maxResTooltip, 1, Qt::AlignTop);
  channelGrid->addLayout(maxResRow, row, 1);

  m_modeStack->addWidget(channelPanel);

  modeLayout->addWidget(m_modeStack);
  leftLayout->addWidget(modeGroup);

  auto *outputGroup = new QGroupBox("Output");
  auto *outputGrid = new QGridLayout(outputGroup);
  outputGrid->setSpacing(8);
  outputGrid->setColumnStretch(1, 1);

  row = 0;
  outputGrid->addWidget(new QLabel("Folder:"), row, 0);
  auto *folderRow = new QHBoxLayout;
  m_outputDirInput = new QLineEdit;
  m_outputDirInput->setText(Settings::lastOutputDir());
  m_outputDirInput->setPlaceholderText("transcripts");
  m_browseBtn = new QPushButton;
  m_browseBtn->setIcon(QIcon(":/icons/browse.svg"));
  m_browseBtn->setIconSize(QSize(16, 16));
  m_browseBtn->setFixedSize(28, 28);
  m_browseBtn->setToolTip("Browse for folder");
  m_browseBtn->setCursor(Qt::PointingHandCursor);
  m_browseBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; border-radius: "
      "4px; "
      "padding: 0px; min-width:28px; max-width:28px; min-height:28px; "
      "max-height:28px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.10); }");
  folderRow->addWidget(m_outputDirInput, 1);
  folderRow->addWidget(m_browseBtn);
  outputGrid->addLayout(folderRow, row, 1);
  row++;

  outputGrid->addWidget(new QLabel("Filename:"), row, 0);
  m_outputFileInput = new QLineEdit;
  m_outputFileInput->setPlaceholderText(
      "results.txt (auto-generated if empty)");
  outputGrid->addWidget(m_outputFileInput, row, 1);

  leftLayout->addWidget(outputGroup);

  auto *proxyGroup = new QGroupBox("Proxy / Advanced");
  auto *proxyGrid = new QGridLayout(proxyGroup);
  proxyGrid->setSpacing(8);
  proxyGrid->setColumnStretch(1, 1);

  row = 0;
  proxyGrid->addWidget(new QLabel("Cookies File:"), row, 0);
  auto *cookiesRow = new QHBoxLayout;
  m_cookiesFileInput = new QLineEdit;
  m_cookiesFileInput->setPlaceholderText(
      "Optional: cookies.txt to bypass IP blocks...");
  m_cookiesBrowseBtn = new QPushButton;
  m_cookiesBrowseBtn->setIcon(QIcon(":/icons/browse.svg"));
  m_cookiesBrowseBtn->setIconSize(QSize(16, 16));
  m_cookiesBrowseBtn->setFixedSize(28, 28);
  m_cookiesBrowseBtn->setCursor(Qt::PointingHandCursor);
  m_cookiesBrowseBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; border-radius: "
      "4px; "
      "padding: 0px; min-width:28px; max-width:28px; min-height:28px; "
      "max-height:28px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.10); }");
  m_cookiesBrowseBtn->setToolTip(
      "Export YouTube cookies with browser extension\n"
      "'Get cookies.txt LOCALLY', then select the file here.\n"
      "Required if YouTube blocks your IP address.");
  cookiesRow->addWidget(m_cookiesFileInput, 1);
  cookiesRow->addWidget(m_cookiesBrowseBtn);
  proxyGrid->addLayout(cookiesRow, row, 1);
  row++;

  proxyGrid->addWidget(new QLabel("Proxy Type:"), row, 0);
  m_proxyTypeCombo = new QComboBox;
  m_proxyTypeCombo->addItems(
      {"None", "Generic (HTTP/SOCKS)", "Webshare (Rotating)"});
  proxyGrid->addWidget(m_proxyTypeCombo, row, 1);
  row++;

  proxyGrid->addWidget(new QLabel("Proxy URL:"), row, 0);
  m_proxyUrlInput = new QLineEdit;
  m_proxyUrlInput->setPlaceholderText(
      "http://user:pass@host:port  or  socks5://host:port");
  m_proxyUrlInput->setToolTip(
      "Proxy URL including credentials.\n"
      "HTTP proxy:   http://user:pass@host:port\n"
      "SOCKS5 proxy: socks5://host:port  (recommended — supports HTTPS "
      "natively)\n"
      "SOCKS5 proxy: socks5://user:pass@host:port\n\n"
      "Note: plain HTTP proxies often reject HTTPS tunneling (400 error).\n"
      "Use a SOCKS5 proxy for the most reliable results.");
  proxyGrid->addWidget(m_proxyUrlInput, row, 1);
  row++;

  proxyGrid->addWidget(new QLabel("Username:"), row, 0);
  m_proxyUsernameInput = new QLineEdit;
  m_proxyUsernameInput->setPlaceholderText("Webshare proxy username...");
  proxyGrid->addWidget(m_proxyUsernameInput, row, 1);
  row++;

  proxyGrid->addWidget(new QLabel("Password:"), row, 0);
  m_proxyPasswordInput = new QLineEdit;
  m_proxyPasswordInput->setPlaceholderText("Webshare proxy password...");
  m_proxyPasswordInput->setEchoMode(QLineEdit::Password);
  proxyGrid->addWidget(m_proxyPasswordInput, row, 1);

  leftLayout->addWidget(proxyGroup);

  auto updateProxyFields = [this]() {
    int idx = m_proxyTypeCombo->currentIndex();
    bool isGeneric = (idx == 1);
    bool isWebshare = (idx == 2);
    m_proxyUrlInput->setVisible(isGeneric);

    auto *grid =
        qobject_cast<QGridLayout *>(m_proxyUrlInput->parentWidget()->layout());
    if (grid) {
      for (int r = 0; r < grid->rowCount(); ++r) {
        auto *item = grid->itemAtPosition(r, 0);
        auto *item1 = grid->itemAtPosition(r, 1);
        if (item1 && item1->widget() == m_proxyUrlInput && item) {
          if (item->widget())
            item->widget()->setVisible(isGeneric);
        }
        if (item1 && item1->widget() == m_proxyUsernameInput && item) {
          if (item->widget())
            item->widget()->setVisible(isWebshare);
        }
        if (item1 && item1->widget() == m_proxyPasswordInput && item) {
          if (item->widget())
            item->widget()->setVisible(isWebshare);
        }
      }
    }
    m_proxyUsernameInput->setVisible(isWebshare);
    m_proxyPasswordInput->setVisible(isWebshare);
  };
  connect(m_proxyTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, updateProxyFields);
  updateProxyFields();

  {
    QString proxyJson = PythonBridge::instance().loadProxySettings();
    QJsonDocument pDoc = QJsonDocument::fromJson(proxyJson.toUtf8());
    QJsonObject pObj = pDoc.object();
    QString pType = pObj.value("type").toString("none");
    if (pType == "generic")
      m_proxyTypeCombo->setCurrentIndex(1);
    else if (pType == "webshare")
      m_proxyTypeCombo->setCurrentIndex(2);
    else
      m_proxyTypeCombo->setCurrentIndex(0);
    m_proxyUrlInput->setText(pObj.value("url").toString());
    m_proxyUsernameInput->setText(pObj.value("username").toString());
    m_proxyPasswordInput->setText(pObj.value("password").toString());
  }

  auto *actionWidget = new QWidget;
  auto *actionLayout = new QVBoxLayout(actionWidget);
  actionLayout->setContentsMargins(0, 6, 0, 0);
  actionLayout->setSpacing(8);

  auto *btnRow = new QHBoxLayout;
  btnRow->addStretch();

  m_searchBtn = new QPushButton("Start Search");
  m_searchBtn->setMinimumWidth(160);
  m_searchBtn->setMinimumHeight(36);
  btnRow->addWidget(m_searchBtn);

  m_cancelBtn = new QPushButton("Cancel");
  m_cancelBtn->setObjectName("cancel_btn");
  m_cancelBtn->setMinimumWidth(100);
  m_cancelBtn->setVisible(false);
  btnRow->addWidget(m_cancelBtn);

  btnRow->addStretch();
  actionLayout->addLayout(btnRow);

  m_statusLabel = new QLabel;
  m_statusLabel->setAlignment(Qt::AlignCenter);
  m_statusLabel->setStyleSheet("color: #888; font-size: 9pt;");
  actionLayout->addWidget(m_statusLabel);

  m_progressBar = new QProgressBar;
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  m_progressBar->setVisible(false);
  m_progressBar->setTextVisible(false);
  m_progressBar->setFixedHeight(4);
  actionLayout->addWidget(m_progressBar);

  leftLayout->addWidget(actionWidget);
  leftLayout->addStretch();

  m_feedbackPanel = new FeedbackPanel(this);

  auto *rightPanel = new QWidget;
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(16, 0, 0, 0);

  m_logDisplay = new QTextEdit;
  m_logDisplay->setObjectName("logDisplay");
  m_logDisplay->setReadOnly(true);
  m_logDisplay->setPlaceholderText("Search output will appear here...");

  rightLayout->addWidget(m_logDisplay);

  m_splitter->addWidget(leftPanel);
  m_splitter->addWidget(rightPanel);
  m_splitter->setStretchFactor(0, 1);
  m_splitter->setStretchFactor(1, 1);
  m_splitter->setHandleWidth(4);
  m_splitter->setStyleSheet(R"(
        QSplitter::handle:horizontal {
            background-color: #555555;
            margin: 0px; 
            border-radius: 2px;
        }
        QSplitter::handle:horizontal:hover {
            background-color: red;
        }
    )");

  rootLayout->addWidget(m_splitter, 1);

  m_logDisplay->installEventFilter(this);
  m_logDisplay->viewport()->installEventFilter(this);
  m_logDisplay->setMouseTracking(true);

  connect(m_searchBtn, &QPushButton::clicked, this,
          &SearchPage::onSearchClicked);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &SearchPage::onCancelClicked);
  connect(m_browseBtn, &QPushButton::clicked, this,
          &SearchPage::onBrowseOutput);
  if (m_pasteBtn)
    connect(m_pasteBtn, &QPushButton::clicked, this,
            &SearchPage::onPasteApiKey);
  connect(m_apiToggleBtn, &QPushButton::clicked, this,
          &SearchPage::onToggleApiVisibility);
  connect(m_videoRadio, &QRadioButton::toggled, this,
          &SearchPage::onModeChanged);
  connect(m_channelRadio, &QRadioButton::toggled, this,
          &SearchPage::onModeChanged);
  connect(m_cookiesBrowseBtn, &QPushButton::clicked, this,
          &SearchPage::onBrowseCookies);

  m_feedbackBtn =
      new QPushButton(this);
  m_feedbackBtn->setIcon(QIcon(":/icons/feedback.svg"));
  m_feedbackBtn->setIconSize(QSize(18, 18));
  m_feedbackBtn->setFixedSize(32, 32);
  m_feedbackBtn->setToolTip("Send Feedback");
  m_feedbackBtn->setStyleSheet(
      "QPushButton { border: none; background: transparent; }"
      "QPushButton:hover { background: rgba(255,255,255,0.1); border-radius: "
      "4px; }");
  m_feedbackBtn->setCursor(Qt::PointingHandCursor);
  m_feedbackBtn->raise();
  positionFeedbackBtn();

  connect(m_feedbackBtn, &QPushButton::clicked, m_feedbackPanel,
          &FeedbackPanel::togglePanel);
}

QString SearchPage::outputDir() const {
  QString dir = m_outputDirInput->text().trimmed();
  return dir.isEmpty() ? QStringLiteral("transcripts") : dir;
}

QStringList SearchPage::lastResults() const { return m_lastResults; }

void SearchPage::onPasteApiKey() {
  QString clip = QApplication::clipboard()->text().trimmed();
  if (!clip.isEmpty())
    m_apiKeyInput->setText(clip);
}

void SearchPage::onToggleApiVisibility() {
  m_apiVisible = !m_apiVisible;
  m_apiKeyInput->setEchoMode(m_apiVisible ? QLineEdit::Normal
                                          : QLineEdit::Password);
  m_apiToggleBtn->setIcon(
      QIcon(m_apiVisible ? ":/icons/hide.svg" : ":/icons/show.svg"));
}

void SearchPage::onModeChanged() {
  m_modeStack->setCurrentIndex(m_channelRadio->isChecked() ? 1 : 0);
}

void SearchPage::onBrowseOutput() {
  QString dir = QFileDialog::getExistingDirectory(this, "Select Output Folder",
                                                  m_outputDirInput->text());
  if (!dir.isEmpty()) {
    m_outputDirInput->setText(dir);
    Settings::setLastOutputDir(dir);
  }
}

void SearchPage::onBrowseCookies() {
  QString path =
      QFileDialog::getOpenFileName(this, "Select Cookies File", QString(),
                                   "Text Files (*.txt);;All Files (*)");
  if (!path.isEmpty())
    m_cookiesFileInput->setText(path);
}

void SearchPage::onSearchClicked() {

  QString apiKey = m_apiKeyInput->text().trimmed();
  if (apiKey.isEmpty()) {
    QMessageBox::warning(this, "Missing API Key",
                         "Please enter your YouTube Data API key.");
    m_apiKeyInput->setFocus();
    return;
  }

  PythonBridge::instance().saveApiKey(apiKey);

  bool isChannelMode = m_channelRadio->isChecked();
  QString keyword;
  QString language;

  if (isChannelMode) {
    QString channel = m_channelInput->text().trimmed();
    if (channel.isEmpty()) {
      QMessageBox::warning(this, "Missing Channel",
                           "Please enter a channel URL or ID.");
      m_channelInput->setFocus();
      return;
    }
    keyword = m_channelSearchTerm->text().trimmed();
    language = m_channelLangCombo->currentText();
  } else {
    QString videoIds = m_videoIdsInput->text().trimmed();
    if (videoIds.isEmpty()) {
      QMessageBox::warning(this, "Missing Video IDs",
                           "Please enter video IDs, URLs, or path to file.");
      m_videoIdsInput->setFocus();
      return;
    }
    keyword = m_videoSearchTerm->text().trimmed();
    language = m_videoLangCombo->currentText();
  }

  if (keyword.isEmpty()) {
    QMessageBox::warning(this, "Missing Search Term",
                         "Please enter a word or phrase to search for.");
    return;
  }

  QString outDir = outputDir();
  QDir().mkpath(outDir);

  QJsonObject params;
  params["api_key"] = apiKey;
  params["keyword"] = keyword;
  params["language"] = language;
  params["output_dir"] = outDir;

  QString outputFilename = m_outputFileInput->text().trimmed();
  if (!outputFilename.isEmpty())
    params["output_filename"] = outputFilename;

  if (isChannelMode) {
    params["search_type"] = "channel";

    QString channelId = extractChannelId(m_channelInput->text());
    params["channel_id"] = channelId;
    params["max_results"] = m_maxResultsSpin->value();
  } else {
    params["search_type"] = "video";
    params["video_ids_input"] =
        m_videoIdsInput->text().trimmed();
  }

  QString cookiesFile = m_cookiesFileInput->text().trimmed();
  if (!cookiesFile.isEmpty())
    params["cookies_file"] = cookiesFile;

  {
    int proxyIdx = m_proxyTypeCombo->currentIndex();
    QString proxyType = "none";
    if (proxyIdx == 1) {
      proxyType = "generic";
      QString pUrl = m_proxyUrlInput->text().trimmed();
      if (!pUrl.isEmpty())
        params["proxy_url"] = pUrl;
    } else if (proxyIdx == 2) {
      proxyType = "webshare";
      QString pUser = m_proxyUsernameInput->text().trimmed();
      QString pPass = m_proxyPasswordInput->text().trimmed();
      if (!pUser.isEmpty())
        params["proxy_username"] = pUser;
      if (!pPass.isEmpty())
        params["proxy_password"] = pPass;
    }
    if (proxyType != "none")
      params["proxy_type"] = proxyType;

    PythonBridge::instance().saveProxySettings(
        proxyType, m_proxyUsernameInput->text().trimmed(),
        m_proxyPasswordInput->text().trimmed(),
        m_proxyUrlInput->text().trimmed());
  }

  m_logDisplay->clear();
  m_lastResults.clear();

  m_worker = new SearchWorker(params);
  m_thread = new QThread;
  m_worker->moveToThread(m_thread);

  connect(m_thread, &QThread::started, m_worker, &SearchWorker::run);
  connect(m_worker, &SearchWorker::progressUpdate, this,
          &SearchPage::onWorkerProgress);
  connect(m_worker, &SearchWorker::logOutput, this, &SearchPage::onWorkerLog);
  connect(m_worker, &SearchWorker::finished, this,
          &SearchPage::onWorkerFinished);
  connect(m_worker, &SearchWorker::error, this, &SearchPage::onWorkerError);

  connect(m_worker, &SearchWorker::finished, m_thread, &QThread::quit);
  connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

  setSearching(true);
  m_thread->start();
}

void SearchPage::onCancelClicked() {
  if (m_worker) {
    m_worker->requestStop();

    disconnect(m_worker, nullptr, this, nullptr);
  }

  setSearching(false);
  m_statusLabel->setText("Search cancelled.");
  m_logDisplay->append(
      "<p style='margin:0;padding:2px 0;font-family:monospace;'>"
      "<span style='color:#ff9800;font-weight:900;'>[WARN]</span>"
      "&nbsp;<span style='color:#ffcc80;'><b>Search cancelled by "
      "user.</b></span></p>");

  m_worker = nullptr;
  m_thread = nullptr;
}

void SearchPage::onWorkerProgress(int percent) {
  m_progressBar->setValue(percent);
  m_statusLabel->setText(QString("Searching... %1%").arg(percent));
}

void SearchPage::onWorkerLog(const QString &html) {
  m_logDisplay->append(html);
}

void SearchPage::onWorkerFinished(int count, const QStringList &results) {
  m_lastResults = results;
  setSearching(false);
  m_progressBar->setValue(100);

  if (count > 0) {
    m_statusLabel->setText(
        QString("<span style='color:#4caf50;'>Done — %1 match%2 found</span>")
            .arg(count)
            .arg(count == 1 ? "" : "es"));
  } else {
    m_statusLabel->setText("Search complete — no matches found.");
  }

  emit searchFinished(count, results);

  m_worker = nullptr;
  m_thread = nullptr;
}

void SearchPage::onWorkerError(const QString &msg) {
  setSearching(false);
  m_statusLabel->setText("<span style='color:#f44336;'>Error occurred</span>");
  m_logDisplay->append("<span style='color:#f44336;'>Error: " + msg +
                       "</span>");

  m_worker = nullptr;
  m_thread = nullptr;
}

void SearchPage::setSearching(bool active) {
  m_searchBtn->setEnabled(!active);
  m_cancelBtn->setVisible(active);
  m_progressBar->setVisible(active);
  if (active) {
    m_progressBar->setValue(0);
    m_statusLabel->setText("Starting search...");
  }
}

bool SearchPage::eventFilter(QObject *watched, QEvent *event) {

  if (watched == m_logDisplay) {
    if (event->type() == QEvent::Enter) {
      m_logDisplay->setProperty("hovered", true);
      m_logDisplay->style()->unpolish(m_logDisplay);
      m_logDisplay->style()->polish(m_logDisplay);
    } else if (event->type() == QEvent::Leave) {
      if (!m_logExpanded) {
        m_logDisplay->setProperty("hovered", false);
        m_logDisplay->style()->unpolish(m_logDisplay);
        m_logDisplay->style()->polish(m_logDisplay);
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void SearchPage::toggleLogExpanded() {
  if (!m_splitter)
    return;

  QList<int> startSizes = m_splitter->sizes();
  int total = startSizes[0] + startSizes[1];
  if (total <= 0)
    total = m_splitter->width();

  QList<int> endSizes;
  if (!m_logExpanded) {

    m_savedSizes = startSizes;
    endSizes = {0, total};
  } else {

    endSizes = m_savedSizes.isEmpty() ? QList<int>{total / 2, total / 2}
                                      : m_savedSizes;
  }

  m_logExpanded = !m_logExpanded;
  m_logDisplay->setProperty("expanded", m_logExpanded);
  m_logDisplay->setProperty("hovered", m_logExpanded);
  m_logDisplay->style()->unpolish(m_logDisplay);
  m_logDisplay->style()->polish(m_logDisplay);

  auto *anim = new QVariantAnimation(this);
  anim->setDuration(250);
  anim->setStartValue(0.0);
  anim->setEndValue(1.0);
  anim->setEasingCurve(QEasingCurve::InOutCubic);

  connect(
      anim, &QVariantAnimation::valueChanged, this, [=](const QVariant &val) {
        double t = val.toDouble();
        int s0 =
            startSizes[0] + static_cast<int>(t * (endSizes[0] - startSizes[0]));
        int s1 =
            startSizes[1] + static_cast<int>(t * (endSizes[1] - startSizes[1]));
        m_splitter->setSizes({s0, s1});
      });
  connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);
  anim->start();
}

}
