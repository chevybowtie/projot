#!/bin/sh
# scripts/install-completion.sh
# Installs the projot shell completion script for the current user.
# Called by `make install-completion`.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPLETIONS_DIR="${SCRIPT_DIR}/../completions"

shell="${SHELL##*/}"

case "${shell}" in
    bash)
        dest="${HOME}/.local/share/bash-completion/completions/projot"
        src="${COMPLETIONS_DIR}/projot.bash"
        mkdir -p "$(dirname "${dest}")"
        cp "${src}" "${dest}"
        echo "Installed bash completion to ${dest}"
        echo "Restart your shell or run: source ${dest}"
        ;;
    zsh)
        dest="${HOME}/.zsh/completions/_projot"
        src="${COMPLETIONS_DIR}/_projot"
        mkdir -p "$(dirname "${dest}")"
        cp "${src}" "${dest}"
        echo "Installed zsh completion to ${dest}"
        echo "Ensure ${HOME}/.zsh/completions is in your \$fpath, then restart your shell."
        ;;
    fish)
        dest="${HOME}/.config/fish/completions/projot.fish"
        src="${COMPLETIONS_DIR}/projot.fish"
        mkdir -p "$(dirname "${dest}")"
        cp "${src}" "${dest}"
        echo "Installed fish completion to ${dest}"
        ;;
    *)
        echo "Shell '${shell}' not recognised. Available completion scripts:"
        echo "  Bash:       ${COMPLETIONS_DIR}/projot.bash"
        echo "  Zsh:        ${COMPLETIONS_DIR}/_projot"
        echo "  Fish:       ${COMPLETIONS_DIR}/projot.fish"
        echo "  PowerShell: ${COMPLETIONS_DIR}/projot.ps1"
        echo
        echo "Install the appropriate script manually for your shell."
        exit 1
        ;;
esac
