document.getElementById('year').textContent = new Date().getFullYear();

const GH_OWNER = 'serptail';
const GH_REPO = 'CapScript-Youtube-Subtitle-Search-Tool';
const GH_LATEST_RELEASE_API = `https://api.github.com/repos/${GH_OWNER}/${GH_REPO}/releases/latest`;
const GH_RELEASES_FALLBACK = `https://github.com/${GH_OWNER}/${GH_REPO}/releases/latest`;
const GH_MANIFEST_FALLBACK = `https://github.com/${GH_OWNER}/${GH_REPO}/blob/main/docs/releases/latest.json`;
const LOCAL_MANIFEST_FALLBACK = 'releases/latest.json';

function normalizeSha(value) {
    if (!value) return '';
    return value.toLowerCase().replace(/^sha256:/, '').trim();
}

function looksLikeSha256(value) {
    return /^[a-f0-9]{64}$/i.test(value || '');
}

function pickWindowsAsset(assets) {
    const rankedMatchers = [/\.exe$/i, /windows|win64|x64/i, /capscript/i];
    for (const matcher of rankedMatchers) {
        const match = assets.find(asset => matcher.test(asset.name || ''));
        if (match) return match;
    }
    return assets.find(asset => !/\.(sha256|sha512|txt|json)$/i.test(asset.name || '')) || null;
}

function pickChecksumAsset(assets) {
    return assets.find(asset => /(sha256|checksums?)/i.test(asset.name || '')) || null;
}

function pickManifestAsset(assets) {
    return assets.find(asset => /latest\.json$/i.test(asset.name || ''))
        || assets.find(asset => /manifest|\.json$/i.test(asset.name || ''))
        || null;
}

function extractShaFromText(text, preferredName) {
    if (!text) return '';
    const lines = text.split(/\r?\n/);

    if (preferredName) {
        const targetLine = lines.find(line => line.includes(preferredName));
        if (targetLine) {
            const namedMatch = targetLine.match(/[a-f0-9]{64}/i);
            if (namedMatch) return normalizeSha(namedMatch[0]);
        }
    }

    const firstMatch = text.match(/[a-f0-9]{64}/i);
    return firstMatch ? normalizeSha(firstMatch[0]) : '';
}

function setDownloadLinks(url) {
    if (!url) return;
    ['win-download-btn', 'hero-download-btn'].forEach(id => {
        const el = document.getElementById(id);
        if (el) {
            el.href = url;
            el.target = '_blank';
            el.rel = 'noopener noreferrer';
        }
    });
}

function setManifestLink(url) {
    const manifestEl = document.getElementById('latest-manifest-link');
    if (!manifestEl) return;
    manifestEl.href = url || GH_MANIFEST_FALLBACK;
    manifestEl.target = '_blank';
    manifestEl.rel = 'noopener noreferrer';
}

async function fetchJson(url) {
    const res = await fetch(url, {
        headers: { Accept: 'application/vnd.github+json' }
    });
    if (!res.ok) {
        throw new Error(`Request failed (${res.status}) for ${url}`);
    }
    return res.json();
}

async function fetchText(url) {
    const res = await fetch(url);
    if (!res.ok) {
        throw new Error(`Request failed (${res.status}) for ${url}`);
    }
    return res.text();
}

async function fetchLocalManifest() {
    const res = await fetch(LOCAL_MANIFEST_FALLBACK);
    if (!res.ok) throw new Error('Local manifest not found');
    return res.json();
}

async function hydrateDownloadsFromGitHub() {
    const checksumEl = document.getElementById('win-checksum');
    const filenameEl = document.getElementById('win-filename');

    try {
        const release = await fetchJson(GH_LATEST_RELEASE_API);
        const assets = Array.isArray(release.assets) ? release.assets : [];
        const winAsset = pickWindowsAsset(assets);
        const checksumAsset = pickChecksumAsset(assets);
        const manifestAsset = pickManifestAsset(assets);

        setDownloadLinks(winAsset?.browser_download_url || GH_RELEASES_FALLBACK);
        setManifestLink(manifestAsset?.browser_download_url || GH_MANIFEST_FALLBACK);

        if (winAsset?.name) {
            filenameEl.textContent = winAsset.name;
        }

        let checksum = '';

        if (winAsset?.digest) {
            checksum = normalizeSha(winAsset.digest);
        }

        if (!looksLikeSha256(checksum) && release.body) {
            checksum = extractShaFromText(release.body, winAsset?.name || '');
        }

        if (!looksLikeSha256(checksum) && checksumAsset?.browser_download_url) {
            try {
                const checksumText = await fetchText(checksumAsset.browser_download_url);
                checksum = extractShaFromText(checksumText, winAsset?.name || '');
            } catch (err) {
                console.warn('Unable to parse checksum asset:', err);
            }
        }

        if (!looksLikeSha256(checksum) && manifestAsset?.browser_download_url) {
            try {
                const manifest = await fetchJson(manifestAsset.browser_download_url);
                if (manifest.artifactName) filenameEl.textContent = manifest.artifactName;
                if (manifest.downloadUrl) setDownloadLinks(manifest.downloadUrl);
                checksum = normalizeSha(manifest.sha256 || '');
            } catch (err) {
                console.warn('Unable to parse manifest asset:', err);
            }
        }

        if (!looksLikeSha256(checksum)) {
            const localManifest = await fetchLocalManifest();
            if (localManifest.artifactName) filenameEl.textContent = localManifest.artifactName;
            if (localManifest.downloadUrl) setDownloadLinks(localManifest.downloadUrl);
            checksum = normalizeSha(localManifest.sha256 || '');
        }

        checksumEl.textContent = looksLikeSha256(checksum)
            ? checksum
            : 'Checksum unavailable for latest release.';
    } catch (err) {
        console.warn('Could not load latest release metadata:', err);
        setDownloadLinks(GH_RELEASES_FALLBACK);
        setManifestLink(GH_MANIFEST_FALLBACK);

        try {
            const localManifest = await fetchLocalManifest();
            if (localManifest.artifactName) filenameEl.textContent = localManifest.artifactName;
            if (localManifest.downloadUrl) setDownloadLinks(localManifest.downloadUrl);
            checksumEl.textContent = normalizeSha(localManifest.sha256 || '') || 'Unable to fetch latest checksum.';
        } catch (fallbackErr) {
            console.warn('Could not load local fallback manifest:', fallbackErr);
            checksumEl.textContent = 'Unable to fetch latest checksum.';
        }
    }
}

window.copyChecksum = function() {
    const checksumText = document.getElementById('win-checksum').textContent;
    if (checksumText && looksLikeSha256(checksumText)) {
        navigator.clipboard.writeText(checksumText).then(() => {
            const btn = document.querySelector('button[title="Copy to clipboard"]');
            const originalHtml = btn.innerHTML;
            btn.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="w-4 h-4 text-green-500"><path d="M20 6 9 17l-5-5"/></svg>';
            setTimeout(() => {
                btn.innerHTML = originalHtml;
            }, 2000);
        }).catch(err => {
            console.error('Failed to copy:', err);
        });
    }
};

document.addEventListener('DOMContentLoaded', () => {
    hydrateDownloadsFromGitHub();
});