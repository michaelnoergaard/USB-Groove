# Distributing USB Groove via winget

This document explains how USB Groove is published to the [Windows Package Manager (winget)](https://github.com/microsoft/winget-pkgs) community repository and how to maintain it.

## Overview

USB Groove is distributed as a **portable** package (`InstallerType: portable`). When users run `winget install michaelnoergaard.USBGroove`, winget downloads `USBGroove.exe` to `%LOCALAPPDATA%\Microsoft\WinGet\Packages\` and creates a symlink so it can be launched from the command line.

No MSI/MSIX installer is needed — the standalone `.exe` is the installer.

## Automated Submissions

The `.github/workflows/winget.yml` workflow automatically submits new versions to winget whenever a GitHub Release is published. It uses [wingetcreate](https://github.com/microsoft/winget-create) to update the manifest and open a PR against `microsoft/winget-pkgs`.

### Setup Requirements

1. **Create a GitHub Personal Access Token (PAT)** with `public_repo` scope
2. **Add it as a repository secret** named `WINGET_PAT`

The PAT must have permission to fork and create pull requests on `microsoft/winget-pkgs`.

## First-Time Submission

Before the automated workflow can update an existing package, you need to submit the initial manifest manually.

### Step 1: Generate the manifest

On a Windows machine with winget installed:

```powershell
# Install wingetcreate
winget install wingetcreate

# Generate manifest interactively
wingetcreate new https://github.com/michaelnoergaard/USB-Groove/releases/download/v1.0/USBGroove.exe
```

Or use the example manifests in the `winget/` directory of this repository as a starting point.

### Step 2: Validate

```powershell
winget validate --manifest <path-to-manifest-folder>
```

### Step 3: Test in Windows Sandbox (recommended)

Clone the winget-pkgs repo and use its sandbox test script:

```powershell
git clone https://github.com/microsoft/winget-pkgs.git
cd winget-pkgs
.\Tools\SandboxTest.ps1 <path-to-manifest-folder>
```

### Step 4: Submit

Option A — Using wingetcreate (easiest):

```powershell
wingetcreate submit --token <YOUR_GITHUB_PAT> <path-to-manifest-folder>
```

Option B — Manually:

1. Fork [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs)
2. Add your manifest files under `manifests/m/michaelnoergaard/USBGroove/<version>/`
3. Open a pull request (one version per PR)
4. Sign the CLA when prompted by the bot
5. Wait for automated validation (~30-40 minutes)
6. A moderator reviews and merges (typically within a day)

## Manifest Structure

winget requires a **multi-file manifest** with 3 YAML files:

```
manifests/m/michaelnoergaard/USBGroove/<version>/
  michaelnoergaard.USBGroove.yaml                   # Version file
  michaelnoergaard.USBGroove.installer.yaml          # Installer file
  michaelnoergaard.USBGroove.locale.en-US.yaml       # Default locale file
```

See the `winget/` directory in this repo for example templates.

## Updating Versions

After the first submission, updates happen automatically via GitHub Actions on each new release. You can also update manually:

```powershell
wingetcreate update michaelnoergaard.USBGroove \
  --version 1.5 \
  --urls https://github.com/michaelnoergaard/USB-Groove/releases/download/v1.5/USBGroove.exe \
  --submit \
  --token <YOUR_GITHUB_PAT>
```

## Key Notes

- **No code signing required** — unsigned executables work but may trigger SmartScreen warnings initially
- **Validation deadline** — if automated checks fail on your PR, you have 7 days to fix before the bot auto-closes it
- **One PR per version** — don't bundle multiple version updates in a single pull request
- **Stable URLs** — GitHub Release download URLs must not return 403 or time out
