#include "UrlLauncher.h"
#include <QDesktopServices>

namespace CapScript {

bool openExternalUrl(const QUrl &url) {
  if (!url.isValid())
    return false;

  if (QDesktopServices::openUrl(url))
    return true;

  return false;
}

}
