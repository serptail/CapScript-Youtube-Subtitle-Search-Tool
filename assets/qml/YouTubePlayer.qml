import QtQuick
import QtWebView

Item {
    id: root

    property string videoUrl: ""

    onVideoUrlChanged: {
        if (videoUrl.length > 0) {
            webView.url = videoUrl;
        }
    }

    function loadVideo(url) {
        videoUrl = url;
    }

    function stop() {
        webView.url = "about:blank";
    }

    Rectangle {
        anchors.fill: parent
        color: "#111111"

        WebView {
            id: webView
            anchors.fill: parent
            url: "about:blank"
        }
    }
}
