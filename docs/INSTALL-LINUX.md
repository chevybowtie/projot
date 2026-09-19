# Installation — Linux

## From a pre-built release (recommended)

Download the latest release from the [GitHub Releases](../../releases/latest) page.

### Option 1: Debian package (recommended)

```sh
# Download and install the .deb package
wget https://github.com/chevybowtie/projot/releases/latest/download/projot_*.deb
sudo apt install ./projot_*.deb
```

This installs the binary and shell completions for bash, zsh, and fish. Completions are enabled automatically on your next shell restart.

### Option 2: Binary only

```sh
# Download and install the binary
curl -Lo projot https://github.com/chevybowtie/projot/releases/latest/download/projot-linux-x86_64
chmod +x projot
sudo mv projot /usr/local/bin/
```

(Optional) Verify the SHA-256 checksum printed on the release page:

```sh
sha256sum projot
```

For shell completions, see [Shell completion](#shell-completion-optional) below.

### Option 3: Tarball

Useful for non-Debian systems:

```sh
wget https://github.com/chevybowtie/projot/releases/latest/download/projot-*-linux-x86_64.tar.gz
tar -xzf projot-*-linux-x86_64.tar.gz
sudo cp -r usr/bin/* /usr/local/bin/
sudo cp -r usr/share/* /usr/local/share/
```

(The tarball unpacks to a `usr/` tree containing `bin/` and `share/`.)

## Shell completion (optional)

### If you installed via .deb

Shell completions (bash, zsh, fish) are included automatically. No additional steps needed — they'll be available on your next shell restart.

### For binary or tarball installs

Download the completion script for your shell from the release page and install it:

| Shell | Download | Install path |
|---|---|---|
| Bash | `projot.bash` | `~/.local/share/bash-completion/completions/projot` |
| Zsh | `_projot` | `~/.zsh/completions/_projot` (must be on `$fpath`) |
| Fish | `projot.fish` | `~/.config/fish/completions/projot.fish` |

Example (Bash):

```sh
mkdir -p ~/.local/share/bash-completion/completions
curl -o ~/.local/share/bash-completion/completions/projot \
  https://github.com/chevybowtie/projot/releases/latest/download/projot.bash
```

Restart your shell or source the completion file:

```sh
source ~/.local/share/bash-completion/completions/projot
```

## Building from source

Requirements: C++17, CMake 3.16+, make

```sh
git clone https://github.com/chevybowtie/projot
cd projot
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # run tests
```

Install to system:

```sh
sudo cmake --install build --prefix /usr/local
```

Or copy the binary manually:

```sh
sudo cp build/projot /usr/local/bin/
```

## Global config (optional)

projot reads global configuration from `~/.config/projot/config` (XDG-compliant). Set base URLs once for all projects:

```sh
projot set-global \
  --rpm-base-url "https://rpm.example.com/" \
  --jira-base-url "https://yourcompany.atlassian.net/browse/"
```

These base URLs are automatically used by all projects in the MCP tools (e.g., `projot_open_rpm`, `projot_open_jira`). The Jira base URL is used for Jira integration.

## Troubleshooting

**Completion not working after installing .deb**

- Start a new shell session: `bash` or `zsh` or `fish`
- Or reload your shell config: `source ~/.bashrc` (Bash), `exec zsh` (Zsh), `exec fish` (Fish)

**Bash completion not working at all (common on WSL and minimal/corporate images)**

The .deb installs its completion to `/usr/share/bash-completion/completions/projot`, which is loaded on demand by the `bash-completion` framework — plain bash never reads that directory. If the framework is missing or not enabled, projot completion silently does nothing.

Check whether the framework is active in your shell:

```sh
type -t _init_completion    # prints "function" if bash-completion is loaded
```

If it prints nothing:

```sh
sudo apt install bash-completion
```

Then make sure your `~/.bashrc` sources it (Ubuntu's default bashrc does; Debian and hardened corporate images often don't):

```sh
if [ -f /usr/share/bash-completion/bash_completion ]; then
    . /usr/share/bash-completion/bash_completion
fi
```

Quick workaround without the framework — source the projot script directly from `~/.bashrc`:

```sh
source /usr/share/bash-completion/completions/projot
```

**Binary not found after install**

- Verify it's in your PATH: `which projot`
- If not, check that `/usr/local/bin` is in `$PATH`: `echo $PATH`

**Cannot read .projot/config**

- Ensure you're inside a git repository that has been initialized: `projot init`
