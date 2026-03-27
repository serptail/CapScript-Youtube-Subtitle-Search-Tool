#include "TitleBar.h"
#include "../core/UrlLauncher.h"

#include <QIcon>
#include <QMouseEvent>
#include <QUrl>
#include <QWindow>

namespace CapScript {

TitleBar::TitleBar(QWidget *parent) : QWidget(parent) {
  setObjectName("titleBar");

  QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed);
  sp.setVerticalStretch(0);
  setSizePolicy(sp);

  QFontMetrics fm(font());
  int textHeight = fm.height();
  int verticalPadding = 12;
  int calculatedHeight = textHeight + verticalPadding;

  setMinimumHeight(qMax(36, calculatedHeight));
  setMaximumHeight(qMax(42, calculatedHeight + 6));

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(14, 6, 6, 6);
  layout->setSpacing(4);

  m_appIconLabel = new QLabel(this);
  m_appIconLabel->setPixmap(QIcon(":/icons/app_icon.ico").pixmap(24, 24));
  m_appIconLabel->setFixedSize(24, 28);
  m_appIconLabel->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
  m_appIconLabel->setContentsMargins(0, 4, 0, 0);
  layout->addWidget(m_appIconLabel);
  layout->addSpacing(8);

  m_titleLabel = new QLabel("CapScript Pro", this);
  m_titleLabel->setObjectName("titleLabel");
  layout->addWidget(m_titleLabel);
  layout->addSpacing(16);

  m_githubBtn = new QPushButton(this);
  m_githubBtn->setObjectName("titleExtraBtn");
  m_githubBtn->setIcon(QIcon(":/icons/github.svg"));
  m_githubBtn->setIconSize(QSize(18, 18));
  m_githubBtn->setFixedSize(28, 28);
  m_githubBtn->setToolTip("Open GitHub Repository");
  layout->addWidget(m_githubBtn);
  layout->addSpacing(4);

  m_donateBtn = new QPushButton(this);
  m_donateBtn->setObjectName("donateBtn");
  m_donateBtn->setIcon(QIcon(":/icons/donate.svg"));
  m_donateBtn->setIconSize(QSize(18, 18));
  m_donateBtn->setFixedSize(28, 28);
  m_donateBtn->setToolTip("Buy me a coffee!");

  m_donateBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0d0d0d; 
            border: none;
            color: #000000; 
        }
        QPushButton:hover {
            background-color: #FFC0CB; 
            color: #FFFFFF; 
        }
    )");

  layout->addWidget(m_donateBtn);

  layout->addStretch();

  m_minimizeBtn = new QPushButton(this);
  m_minimizeBtn->setObjectName("minimizeButton");
  m_minimizeBtn->setIcon(QIcon(":/icons/minimize.svg"));
  m_minimizeBtn->setIconSize(QSize(14, 14));
  m_minimizeBtn->setToolTip("Minimize");
  m_minimizeBtn->setFixedSize(36, 28);

  m_maximizeBtn = new QPushButton(this);
  m_maximizeBtn->setObjectName("maximizeButton");
  m_maximizeBtn->setIcon(QIcon(":/icons/maximize.svg"));
  m_maximizeBtn->setIconSize(QSize(14, 14));
  m_maximizeBtn->setToolTip("Maximize");
  m_maximizeBtn->setFixedSize(36, 28);

  m_closeBtn = new QPushButton(this);
  m_closeBtn->setObjectName("closeButton");
  m_closeBtn->setIcon(QIcon(":/icons/close.svg"));
  m_closeBtn->setIconSize(QSize(14, 14));
  m_closeBtn->setToolTip("Close");
  m_closeBtn->setFixedSize(36, 28);

  layout->addWidget(m_minimizeBtn);
  layout->addWidget(m_maximizeBtn);
  layout->addWidget(m_closeBtn);

  connect(m_minimizeBtn, &QPushButton::clicked, this,
          &TitleBar::minimizeClicked);
  connect(m_maximizeBtn, &QPushButton::clicked, this,
          &TitleBar::maximizeClicked);
  connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);

  connect(m_githubBtn, &QPushButton::clicked, this, []() {
    openExternalUrl(
        QUrl("https://github.com/serptail"));

  });
  connect(m_donateBtn, &QPushButton::clicked, this,
          []() { openExternalUrl(QUrl("https://ko-fi.com/serptail")); });
}

void TitleBar::setTitle(const QString &title) { m_titleLabel->setText(title); }

void TitleBar::mousePressEvent(QMouseEvent *e) {
  if (qobject_cast<QPushButton *>(childAt(e->position().toPoint()))) {
    QWidget::mousePressEvent(e);
    return;
  }

  if (e->button() == Qt::LeftButton) {
    m_dragging = true;
    m_dragStart =
        e->globalPosition().toPoint() - window()->frameGeometry().topLeft();
    e->accept();
  }
}

void TitleBar::mouseMoveEvent(QMouseEvent *e) {
  if (m_dragging && (e->buttons() & Qt::LeftButton)) {

    if (window()->isMaximized()) {
      window()->showNormal();

      m_dragStart = QPoint(width() / 2, height() / 2);
    }
    window()->move(e->globalPosition().toPoint() - m_dragStart);
    e->accept();
  }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *e) {
  m_dragging = false;
  e->accept();
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    emit maximizeClicked();
    e->accept();
  }
}

}
