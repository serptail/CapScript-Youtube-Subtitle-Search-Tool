#include "SearchPage.h"
#include "../../core/PythonBridge.h"
#include "../../core/Settings.h"
#include "../../core/ToolPaths.h"
#include "../../workers/SearchWorker.h"
#include "../widgets/FeedbackWidget.h"
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QUrl>
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

namespace {

static QString formatBytes(qint64 bytes) {
  if (bytes <= 0)
    return QStringLiteral("unknown size");

  const double kib = 1024.0;
  const double mib = kib * 1024.0;
  const double gib = mib * 1024.0;

  if (bytes >= gib)
    return QString::number(bytes / gib, 'f', 2) + " GB";
  if (bytes >= mib)
    return QString::number(bytes / mib, 'f', 2) + " MB";
  if (bytes >= kib)
    return QString::number(bytes / kib, 'f', 1) + " KB";
  return QString::number(bytes) + " B";
}

static QString ytDlpDownloadUrl() {
  return QStringLiteral(
      "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe");
}

static QString localYtDlpPath() {
  return ToolPaths::localYtdlp();
}

static QString normalizeYtDlpVersion(const QString &raw) {
  QString v = raw.trimmed();
  if (v.startsWith('v') || v.startsWith('V'))
    v.remove(0, 1);
  return v;
}

static int compareVersionStrings(const QString &lhsRaw, const QString &rhsRaw) {
  const QString lhs = normalizeYtDlpVersion(lhsRaw);
  const QString rhs = normalizeYtDlpVersion(rhsRaw);

  const QStringList lParts = lhs.split('.', Qt::SkipEmptyParts);
  const QStringList rParts = rhs.split('.', Qt::SkipEmptyParts);
  const int n = qMax(lParts.size(), rParts.size());

  for (int i = 0; i < n; ++i) {
    const int l = i < lParts.size() ? lParts[i].toInt() : 0;
    const int r = i < rParts.size() ? rParts[i].toInt() : 0;
    if (l < r)
      return -1;
    if (l > r)
      return 1;
  }

  return 0;
}

static QString latestYtDlpReleaseApiUrl() {
  return QStringLiteral("https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest");
}

static void persistLocalYtDlpVersion(QWidget *owner, const QString &ytdlpPath,
                                     QLabel *statusLabel = nullptr) {
  auto *proc = new QProcess(owner);
  proc->setProcessChannelMode(QProcess::MergedChannels);
  proc->setStandardInputFile(QProcess::nullDevice());
  proc->setProgram(ytdlpPath);
  proc->setArguments({"--version"});
#ifdef _WIN32
  proc->setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif
  QObject::connect(
      proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      owner,
      [proc, statusLabel](int exitCode, QProcess::ExitStatus) {
        const QString version =
            normalizeYtDlpVersion(QString::fromLocal8Bit(proc->readAll()).trimmed());
        if (exitCode == 0 && !version.isEmpty()) {
          Settings::setYtDlpVersionSaved(version);
          if (statusLabel)
            statusLabel->setText(QStringLiteral("yt-dlp downloaded (%1)").arg(version));
        }
        proc->deleteLater();
      });
  proc->start();
}

} // namespace

void SearchPage::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  positionFeedbackBtn();
}

void SearchPage::showEvent(QShowEvent *e) {
  QWidget::showEvent(e);
  positionFeedbackBtn();
  if (!m_startupUpdateChecked) {
    m_startupUpdateChecked = true;
    QTimer::singleShot(0, this, &SearchPage::checkForYtDlpUpdate);
  }
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

  auto *modeHeading = new QLabel("Search Mode");
  modeHeading->setStyleSheet(
      "font-size: 12px;"
      "font-weight: normal;"
      "color: #eeeeee;"
  );
  leftLayout->addWidget(modeHeading);

  auto *modeGroup = new QGroupBox();
  modeGroup->setObjectName("searchModeGroup");
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

  auto *matchRow = new QHBoxLayout;
  matchRow->addWidget(new QLabel("Match Mode:"));
  m_matchModeCombo = new QComboBox;
  m_matchModeCombo->addItem("Smart (recommended)", "smart");
  m_matchModeCombo->addItem("Exact Phrase", "exact_phrase");
  m_matchModeCombo->addItem("Contains (legacy)", "contains");
  m_matchModeCombo->setCurrentIndex(0);
  m_matchModeCombo->setToolTip(
      "Smart: exact word for single terms, phrase-aware for multiple words.\n"
      "Exact Phrase: ordered phrase match with punctuation tolerance.\n"
      "Contains: legacy substring behavior.");
  matchRow->addWidget(m_matchModeCombo, 1);
  modeLayout->addLayout(matchRow);

  leftLayout->addWidget(modeGroup);

  auto *outputHeading = new QLabel("Output");
  outputHeading->setStyleSheet(
      "font-size: 12px;"
      "font-weight: normal;"
      "color: #eeeeee;"
  );
  leftLayout->addWidget(outputHeading);

  auto *outputGroup = new QGroupBox();
  outputGroup->setObjectName("outputGroup");
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

  auto *proxyHeading = new QLabel("Proxy / Advanced");
  proxyHeading->setStyleSheet(
      "font-size: 12px;"
      "font-weight: normal;"
      "color: #eeeeee;"
  );
  leftLayout->addWidget(proxyHeading);

  auto *proxyGroup = new QGroupBox();
  proxyGroup->setObjectName("proxyGroup");
  auto *proxyGrid = new QGridLayout(proxyGroup);
  proxyGrid->setSpacing(8);
  proxyGrid->setColumnStretch(1, 1);

  row = 0;
  proxyGrid->addWidget(new QLabel("Cookies File:"), row, 0);
  auto *cookiesRow = new QHBoxLayout;
  m_cookiesFileInput = new QLineEdit;
  m_cookiesFileInput->setText(Settings::lastCookiesFile());
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

  proxyGrid->addWidget(new QLabel("Cookies Browser:"), row, 0);
  m_cookiesBrowserCombo = new QComboBox;
  m_cookiesBrowserCombo->addItem("None", "none");
  m_cookiesBrowserCombo->addItem("Chrome", "chrome");
  m_cookiesBrowserCombo->addItem("Edge", "edge");
  m_cookiesBrowserCombo->addItem("Firefox", "firefox");
  m_cookiesBrowserCombo->addItem("Safari", "safari");
  m_cookiesBrowserCombo->addItem("Brave", "brave");
  m_cookiesBrowserCombo->addItem("Opera", "opera");
  m_cookiesBrowserCombo->addItem("Vivaldi", "vivaldi");
  m_cookiesBrowserCombo->setEditable(false);
  m_cookiesBrowserCombo->setToolTip(
      "Use yt-dlp --cookies-from-browser <browser> to extract cookies directly\n"
      "from a local browser profile. Leave as None to disable.\n"
      "If Cookies File is set and exists, that file is used first.");
  {
    const QString savedBrowser = Settings::lastCookiesBrowser().trimmed();
    if (!savedBrowser.isEmpty()) {
      const int idx = m_cookiesBrowserCombo->findData(savedBrowser, Qt::MatchFixedString);
      if (idx >= 0)
        m_cookiesBrowserCombo->setCurrentIndex(idx);
      else
        m_cookiesBrowserCombo->setCurrentText(savedBrowser);
    }
  }
  proxyGrid->addWidget(m_cookiesBrowserCombo, row, 1);
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
    m_proxyUrlInput->setVisible(isGeneric || isWebshare);

    auto *grid =
        qobject_cast<QGridLayout *>(m_proxyUrlInput->parentWidget()->layout());
    if (grid) {
      for (int r = 0; r < grid->rowCount(); ++r) {
        auto *item = grid->itemAtPosition(r, 0);
        auto *item1 = grid->itemAtPosition(r, 1);
        if (item1 && item1->widget() == m_proxyUrlInput && item) {
          if (item->widget())
            item->widget()->setVisible(isGeneric || isWebshare);
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
  if (!path.isEmpty()) {
    m_cookiesFileInput->setText(path);
    Settings::setLastCookiesFile(path);
  }
}

void SearchPage::onSearchClicked() {
  ensureYtDlpAndStartSearch();
}

void SearchPage::startSearch() {
  m_cancelRequested = false;

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
  params["api_key"] = "";  // No API key needed for yt-dlp
  params["keyword"] = keyword;
  params["match_mode"] =
      (m_matchModeCombo ? m_matchModeCombo->currentData().toString() : "smart");
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
  QString cookiesBrowser =
      m_cookiesBrowserCombo
          ? m_cookiesBrowserCombo->currentData().toString().trimmed().toLower()
          : QStringLiteral("none");
  if ((cookiesBrowser.isEmpty() || cookiesBrowser == "none") &&
      m_cookiesBrowserCombo) {
    cookiesBrowser = m_cookiesBrowserCombo->currentText().trimmed().toLower();
  }

  if (!cookiesFile.isEmpty())
    params["cookies_file"] = cookiesFile;
  if (!cookiesBrowser.isEmpty() && cookiesBrowser != "none")
    params["cookies_from_browser"] = cookiesBrowser;

  Settings::setLastCookiesFile(cookiesFile);
  Settings::setLastCookiesBrowser(cookiesBrowser);

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
      QString pUrl = m_proxyUrlInput->text().trimmed();
      if (!pUser.isEmpty())
        params["proxy_username"] = pUser;
      if (!pPass.isEmpty())
        params["proxy_password"] = pPass;
      if (!pUrl.isEmpty())
        params["proxy_url"] = pUrl;
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

void SearchPage::ensureYtDlpAndStartSearch() {
  if (!localYtDlpPath().isEmpty()) {
    startSearch();
    return;
  }

  if (m_ytdlpDownloadInProgress)
    return;

  m_statusLabel->setText("yt-dlp is missing — preparing download...");

  if (!m_toolDownloader)
    m_toolDownloader = new QNetworkAccessManager(this);

  QNetworkRequest request{QUrl(ytDlpDownloadUrl())};
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  auto *reply = m_toolDownloader->head(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    qint64 size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    promptInstallYtDlp(size);
    reply->deleteLater();
  });
}

void SearchPage::promptInstallYtDlp(qint64 contentLengthBytes) {
  if (m_ytdlpDownloadInProgress)
    return;

  if (!m_toolDownloader)
    m_toolDownloader = new QNetworkAccessManager(this);

  const QString sizeText = contentLengthBytes > 0
                               ? QStringLiteral(" (%1)").arg(formatBytes(contentLengthBytes))
                               : QStringLiteral("");
  const auto answer = QMessageBox::question(
      this, "Download yt-dlp",
      QStringLiteral(
          "CapScript relies on the awesome tool yt-dlp, would you like to "
          "download it now%1?")
          .arg(sizeText),
      QMessageBox::Yes | QMessageBox::No);

  if (answer != QMessageBox::Yes)
    return;

  downloadYtDlp(false, QStringLiteral("Installing yt-dlp"));
}

void SearchPage::downloadYtDlp(bool backgroundUpdate, const QString &statusPrefix) {
  if (m_ytdlpDownloadInProgress)
    return;

  if (!m_toolDownloader)
    m_toolDownloader = new QNetworkAccessManager(this);

  const QString binDir = QCoreApplication::applicationDirPath() + "/bin";
  QDir().mkpath(binDir);
  const QString destFile = binDir + "/yt-dlp.exe";

  m_ytdlpDownloadInProgress = true;
  m_statusLabel->setText(backgroundUpdate ? "Updating yt-dlp..." : statusPrefix + "...");

  auto *progress = new QProgressDialog(this);
  if (!backgroundUpdate) {
    progress->setWindowTitle("Downloading yt-dlp");
    progress->setLabelText(QStringLiteral("Downloading yt-dlp..."));
    progress->setCancelButton(nullptr);
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->show();
  }

  QNetworkRequest request{QUrl(ytDlpDownloadUrl())};
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  auto *reply = m_toolDownloader->get(request);
  connect(reply, &QNetworkReply::downloadProgress, this,
          [this, progress, backgroundUpdate](qint64 received, qint64 total) {
            int percent = 0;
            if (total > 0)
              percent = static_cast<int>(received * 100 / total);
            if (!backgroundUpdate)
              progress->setValue(percent);
            if (!backgroundUpdate) {
              m_progressBar->setVisible(true);
              m_progressBar->setRange(0, 100);
              m_progressBar->setValue(percent);
            }
          });
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, progress, backgroundUpdate, destFile]() {
            const auto cleanup = [this, reply, progress]() {
              reply->deleteLater();
              progress->deleteLater();
              m_ytdlpDownloadInProgress = false;
            };

            if (reply->error() != QNetworkReply::NoError) {
              m_statusLabel->setText("yt-dlp download failed.");
              m_logDisplay->append("<span style='color:#f44336;'>Failed to download yt-dlp: " +
                                   reply->errorString() + "</span>");
              cleanup();
              return;
            }

            QFile outFile(destFile);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
              m_statusLabel->setText("yt-dlp download failed.");
              m_logDisplay->append("<span style='color:#f44336;'>Failed to write yt-dlp to bin/</span>");
              cleanup();
              return;
            }

            outFile.write(reply->readAll());
            outFile.close();

            if (backgroundUpdate)
              m_statusLabel->setText("Updated yt-dlp");
            else
              m_statusLabel->setText("yt-dlp downloaded");

            m_logDisplay->append(backgroundUpdate
                                     ? "<span style='color:#90ee90;'>Updated yt-dlp.</span>"
                                     : "<span style='color:#4caf50;'>yt-dlp downloaded successfully.</span>");

            m_progressBar->setValue(100);
            if (!backgroundUpdate)
              progress->setValue(100);

            persistLocalYtDlpVersion(this, destFile,
                                     backgroundUpdate ? nullptr : m_statusLabel);

            cleanup();
            if (!backgroundUpdate)
              startSearch();
          });
}

void SearchPage::checkForYtDlpUpdate() {
  const QString currentYtdlp = localYtDlpPath();
  if (currentYtdlp.isEmpty()) {
    m_statusLabel->setText("yt-dlp is missing");
    return;
  }

  m_statusLabel->setText("Checking for yt-dlp updates...");

  auto *versionProc = new QProcess(this);
  versionProc->setProcessChannelMode(QProcess::MergedChannels);
  versionProc->setStandardInputFile(QProcess::nullDevice());
  versionProc->setProgram(currentYtdlp);
  versionProc->setArguments({"--version"});
#ifdef _WIN32
  versionProc->setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; });
#endif
  connect(versionProc,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this, versionProc](int exitCode, QProcess::ExitStatus) {
            const QString localVersion = normalizeYtDlpVersion(
                QString::fromLocal8Bit(versionProc->readAll()).trimmed());
            versionProc->deleteLater();

            if (exitCode != 0 || localVersion.isEmpty()) {
              m_statusLabel->setText("Could not read local yt-dlp version");
              return;
            }

            QString savedVersion = Settings::ytDlpVersionSaved();
            if (savedVersion.isEmpty()) {
              savedVersion = localVersion;
              Settings::setYtDlpVersionSaved(savedVersion);
            } else if (compareVersionStrings(savedVersion, localVersion) != 0) {
              savedVersion = localVersion;
              Settings::setYtDlpVersionSaved(savedVersion);
            }

            if (!m_toolDownloader)
              m_toolDownloader = new QNetworkAccessManager(this);

            QNetworkRequest req{QUrl(latestYtDlpReleaseApiUrl())};
            req.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("CapScriptPro/1.0"));
            req.setRawHeader("Accept", "application/vnd.github+json");
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

            auto *reply = m_toolDownloader->get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, savedVersion]() {
              const auto done = [reply]() { reply->deleteLater(); };

              if (reply->error() != QNetworkReply::NoError) {
                m_statusLabel->setText("Could not check yt-dlp latest version");
                done();
                return;
              }

              const QByteArray body = reply->readAll();
              QJsonParseError parseError{};
              const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
              if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                m_statusLabel->setText("Invalid yt-dlp release response");
                done();
                return;
              }

              const QJsonObject obj = doc.object();
              QString latestVersion = obj.value("tag_name").toString().trimmed();
              if (latestVersion.isEmpty())
                latestVersion = obj.value("name").toString().trimmed();
              latestVersion = normalizeYtDlpVersion(latestVersion);

              if (latestVersion.isEmpty()) {
                m_statusLabel->setText("Could not parse latest yt-dlp version");
                done();
                return;
              }

              Settings::setYtDlpLastCheckedLatest(latestVersion);

              if (compareVersionStrings(savedVersion, latestVersion) < 0) {
                m_statusLabel->setText(
                    QStringLiteral("Updating yt-dlp (%1 -> %2)")
                        .arg(savedVersion, latestVersion));
                downloadYtDlp(true, QStringLiteral("Updating yt-dlp"));
              } else {
                m_statusLabel->setText(
                    QStringLiteral("yt-dlp is up to date (%1)").arg(savedVersion));
              }

              done();
            });
          });
  versionProc->start();
}

void SearchPage::onCancelClicked() {
  if (!m_worker)
    return;

  m_cancelRequested = true;
  m_cancelBtn->setEnabled(false);
  m_statusLabel->setText("Cancelling...");
  m_worker->requestStop();
}

void SearchPage::onWorkerProgress(int percent) {
  m_progressBar->setValue(percent);
  if (m_cancelRequested)
    m_statusLabel->setText(QString("Cancelling... %1%").arg(percent));
  else
    m_statusLabel->setText(QString("Searching... %1%").arg(percent));
}

void SearchPage::onWorkerLog(const QString &html) {
  m_logDisplay->append(html);
}

void SearchPage::onWorkerFinished(int count, const QStringList &results) {
  m_lastResults = results;
  setSearching(false);
  m_progressBar->setValue(100);

  if (m_cancelRequested) {
    if (count > 0) {
      m_statusLabel->setText(
          QString("<span style='color:#ff9800;'>Search cancelled — %1 partial "
                  "match%2 captured</span>")
              .arg(count)
              .arg(count == 1 ? "" : "es"));
      emit searchFinished(count, results);
    } else {
      m_statusLabel->setText("Search cancelled.");
    }
  } else {
    if (count > 0) {
      m_statusLabel->setText(
          QString("<span style='color:#4caf50;'>Done — %1 match%2 found</span>")
              .arg(count)
              .arg(count == 1 ? "" : "es"));
    } else {
      m_statusLabel->setText("Search complete — no matches found.");
    }

    emit searchFinished(count, results);
  }

  m_cancelRequested = false;

  m_worker = nullptr;
  m_thread = nullptr;
}

void SearchPage::onWorkerError(const QString &msg) {
  setSearching(false);
  if (m_cancelRequested) {
    m_statusLabel->setText("Search cancelled.");
  } else {
    m_statusLabel->setText("<span style='color:#f44336;'>Error occurred</span>");
    m_logDisplay->append("<span style='color:#f44336;'>Error: " + msg +
                         "</span>");
  }

  m_cancelRequested = false;

  m_worker = nullptr;
  m_thread = nullptr;
}

void SearchPage::setSearching(bool active) {
  m_searchBtn->setEnabled(!active);
  m_cancelBtn->setVisible(active);
  m_cancelBtn->setEnabled(active);
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