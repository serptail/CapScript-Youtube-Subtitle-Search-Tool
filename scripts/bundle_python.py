import sys
import os
import urllib.request
import zipfile
import shutil
import subprocess
import glob
import hashlib


def read_file_hash(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def install_requirements(site_packages, req_file, marker_name, marker_scope):
    os.makedirs(site_packages, exist_ok=True)

    req_hash = read_file_hash(req_file)
    marker_payload = f"{marker_scope}|{req_hash}\n"
    marker_file = os.path.join(site_packages, marker_name)

    if os.path.exists(marker_file):
        try:
            with open(marker_file, "r", encoding="utf-8") as f:
                if f.read() == marker_payload:
                    print("Requirements already installed.")
                    return
        except OSError:
            pass

    print(f"Installing requirements from {req_file} into {site_packages}...")
    subprocess.check_call(
        [
            sys.executable,
            "-m",
            "pip",
            "install",
            "--upgrade",
            "--prefer-binary",
            "-r",
            req_file,
            "--target",
            site_packages,
        ]
    )

    with open(marker_file, "w", encoding="utf-8") as f:
        f.write(marker_payload)


def bundle_windows_runtime(py_version, target_dir, req_file):
    parts = py_version.split(".")
    if len(parts) < 3:
        py_version += ".0"
        parts = py_version.split(".")

    major_minor = f"{parts[0]}{parts[1]}"
    embed_url = f"https://www.python.org/ftp/python/{py_version}/python-{py_version}-embed-amd64.zip"
    embed_zip = os.path.join(target_dir, f"python-{py_version}-embed-amd64.zip")

    os.makedirs(target_dir, exist_ok=True)

    if not os.path.exists(os.path.join(target_dir, f"python{major_minor}.dll")):
        print(f"Downloading {embed_url}...")
        try:
            urllib.request.urlretrieve(embed_url, embed_zip)
        except Exception as e:
            print(f"Failed exact version, trying fallback... ({e})")
            fallbacks = {
                "3.12": "3.12.3",
                "3.11": "3.11.8",
                "3.10": "3.10.11",
                "3.9": "3.9.13",
                "3.8": "3.8.10",
            }
            mm_dot = f"{parts[0]}.{parts[1]}"
            if mm_dot in fallbacks:
                py_version = fallbacks[mm_dot]
                embed_url = f"https://www.python.org/ftp/python/{py_version}/python-{py_version}-embed-amd64.zip"
                embed_zip = os.path.join(
                    target_dir, f"python-{py_version}-embed-amd64.zip"
                )
                print(f"Downloading fallback {embed_url}...")
                try:
                    urllib.request.urlretrieve(embed_url, embed_zip)
                except Exception as e2:
                    print(f"Failed to download fallback embeddable python: {e2}")
                    sys.exit(1)
            else:
                sys.exit(1)

        print("Extracting...")
        with zipfile.ZipFile(embed_zip, "r") as zip_ref:
            zip_ref.extractall(target_dir)
        os.remove(embed_zip)

    pth_file = os.path.join(target_dir, f"python{major_minor}._pth")
    if os.path.exists(pth_file):
        with open(pth_file, "r", encoding="utf-8") as f:
            lines = f.readlines()

        out_lines = []
        has_site = False
        has_dot = False
        for line in lines:
            stripped = line.strip()
            if stripped.startswith("#") and "import site" in stripped:
                out_lines.append("import site\n")
                continue
            if stripped == "import site":
                out_lines.append("import site\n")
                continue
            if stripped == "Lib/site-packages":
                has_site = True
                out_lines.append("Lib/site-packages\n")
                continue
            if stripped == ".":
                has_dot = True
                out_lines.append(".\n")
                continue
            if stripped:
                out_lines.append(line)

        if not has_site:
            out_lines.append("Lib/site-packages\n")
        if not has_dot:
            out_lines.append(".\n")

        with open(pth_file, "w", encoding="utf-8") as f:
            f.writelines(out_lines)

    site_packages = os.path.join(target_dir, "Lib", "site-packages")
    install_requirements(site_packages, req_file, ".bundle_done", f"win-{major_minor}")

    parent_dir = os.path.dirname(target_dir)
    if os.path.basename(
        os.path.normpath(target_dir)
    ).lower() == "python" and os.path.isdir(parent_dir):
        for pattern in ("python3.dll", "python*.dll"):
            for src in glob.glob(os.path.join(target_dir, pattern)):
                name = os.path.basename(src).lower()
                if not (name.startswith("python") and name.endswith(".dll")):
                    continue
                dst = os.path.join(parent_dir, os.path.basename(src))
                try:
                    shutil.copy2(src, dst)
                except Exception as e:
                    print(f"Warning: failed to mirror {src} -> {dst}: {e}")


def main():
    if len(sys.argv) < 4:
        print(
            "Usage: python bundle_python.py <python_version> <target_dir> <requirements_file>"
        )
        sys.exit(1)

    py_version = sys.argv[1]
    target_dir = sys.argv[2]
    req_file = sys.argv[3]

    if os.name != "nt":
        raise RuntimeError("bundle_python.py is Windows-only.")

    bundle_windows_runtime(py_version, target_dir, req_file)


if __name__ == "__main__":
    main()
