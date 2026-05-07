# Installation — Windows

## From a pre-built release (recommended)

Download the latest release from the [GitHub Releases](../../releases/latest) page.

### Option 1: Chocolatey package (recommended)

If you have [Chocolatey](https://chocolatey.org/) installed:

```powershell
# Download the .nupkg from GitHub Releases, then install
choco install projot.*.nupkg --source .
```

This installs the binary and enables PowerShell tab completion automatically.

### Option 2: Manual binary installation

1. Download `projot-windows-x86_64.exe` from the release page
2. Rename it to `projot.exe`
3. Choose a directory on your `PATH`, e.g. `C:\Users\<YourUsername>\AppData\Local\Programs\projot\`
4. Move `projot.exe` to that directory
5. Add the directory to `PATH`:
   - Open **Settings → System → About → Advanced system settings**
   - Click **Environment Variables** at the bottom
   - Under **User variables**, click **Path → Edit**
   - Click **New** and add your directory
   - Click **OK** three times and restart your shell

Verify the install:
```powershell
projot --help
```

## PowerShell completion (optional)

Tab completion in PowerShell requires a one-time setup.

### If you installed via Chocolatey

PowerShell completion is configured automatically — it's available in new PowerShell windows.

### For manual binary install

1. Download `projot.ps1` from the release page. Save it somewhere permanent (e.g. `C:\Users\<YourUsername>\Documents\PowerShell\`)

2. Open your PowerShell profile for editing:
   ```powershell
   notepad $PROFILE
   ```
   - If the file doesn't exist, create it first: `New-Item -Path $PROFILE -ItemType File -Force`

3. Add this line (adjust path to where you saved `projot.ps1`):
   ```powershell
   . "C:\Users\<YourUsername>\Documents\PowerShell\projot.ps1"
   ```

4. Save and close the file

5. Restart PowerShell or reload your profile:
   ```powershell
   & $PROFILE
   ```

Test completion:
```powershell
projot set-<TAB>    # Should complete to "set-global", etc.
```

## Building from source

Requirements:
- [CMake](https://cmake.org/download/) 3.16 or newer
- C++17 compiler (MSVC 2019+ or Clang)
- Optional: [Git Bash](https://gitforwindows.org/) for Unix-like build commands

### Using CMake (all platforms)

```powershell
git clone https://github.com/ORG/projot
cd projot
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure    # run tests
```

### Using Visual Studio (Windows native)

```powershell
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Global config (optional)

projot reads global configuration from `%APPDATA%\projot\config`. Set base URLs once for all projects:

```powershell
projot set-global `
  --rpm-base-url "https://rpm.example.com/" `
  --itrack-base-url "https://itrack.example.com/record/"
```

These base URLs are automatically used by all projects in the MCP tools (e.g., `open_rpm`, `open_itrack`).

## Troubleshooting

**PowerShell completion not working**
- Verify the script is sourced: Run `Test-Path $PROFILE` (should return `True`)
- Check that the dot-source line is in your profile: `Get-Content $PROFILE | Select-String "projot.ps1"`
- Restart PowerShell: `exit` and open a new window
- If still not working, check execution policy: `Get-ExecutionPolicy` should be `RemoteSigned` or `Unrestricted`

**Binary not found after install**
- Verify it's in your PATH: `Get-Command projot` (should show the path)
- If not found, check that you added the directory to `PATH` and restarted your shell
- Test from Command Prompt: `projot --help`

**Cannot read .projot/config**
- Ensure you're inside a git repository that has been initialized: `projot init`
- Run from Git Bash or PowerShell in the repo directory: `cd C:\path\to\repo` then `projot list`

**MCP server not found**
- The MCP server (`projot.ps1` script) is installed at `%APPDATA%\projot\projot.ps1` if you used Chocolatey
- For manual installs, you need to configure it manually in your IDE's settings — see [mcp/README.md](../mcp/README.md)
