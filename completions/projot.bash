# projot bash completion
# Source this file or install to /etc/bash_completion.d/ or
# ~/.local/share/bash-completion/completions/projot

_projot_open_todo_ids() {
    local config rpm notes
    config="$(git -C . rev-parse --show-toplevel 2>/dev/null)/.projot/config"
    rpm=$(grep -m1 '^rpm\s*=' "$config" 2>/dev/null | sed 's/.*=\s*//' | tr -d '[:space:]')
    notes="$(git -C . rev-parse --show-toplevel 2>/dev/null)/.projot/${rpm}.md"
    grep -oP '^\d+(?=\. \[ \])' "$notes" 2>/dev/null
}

_projot() {
    local cur prev words
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    words=("${COMP_WORDS[@]}")

    local subcommands="init new close add-todo list complete add-note set-link set-app-id add-github add-swagger add-blizzard add-azure render install-hook install-mcp-server set-global"

    # First word after projot — complete subcommands
    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "--help --version ${subcommands}" -- "${cur}") )
        return
    fi

    local subcmd="${words[1]}"

    # --help available everywhere
    if [[ "${cur}" == --* ]]; then
        case "${subcmd}" in
            init)
                COMPREPLY=( $(compgen -W "--app-id --github --swagger --blizzard --help" -- "${cur}") )
                ;;
            new)
                COMPREPLY=( $(compgen -W "--rpm --name --itrack --teams --rpm-url --itrack-url --other --no-hook --help" -- "${cur}") )
                ;;
            close)
                COMPREPLY=( $(compgen -W "--help" -- "${cur}") )
                ;;
            add-todo)
                COMPREPLY=( $(compgen -W "--help" -- "${cur}") )
                ;;
            list)
                COMPREPLY=( $(compgen -W "--open --closed --all --help" -- "${cur}") )
                ;;
            complete)
                COMPREPLY=( $(compgen -W "--todo --help" -- "${cur}") )
                ;;
            add-note)
                COMPREPLY=( $(compgen -W "--todo --text --help" -- "${cur}") )
                ;;
            set-link)
                COMPREPLY=( $(compgen -W "--key --url --help" -- "${cur}") )
                ;;
            set-app-id)
                COMPREPLY=( $(compgen -W "--app-id --force --help" -- "${cur}") )
                ;;
            add-github|add-swagger|add-blizzard)
                COMPREPLY=( $(compgen -W "--url --help" -- "${cur}") )
                ;;
            add-azure)
                COMPREPLY=( $(compgen -W "--type --name --url --help" -- "${cur}") )
                ;;
            render|install-hook)
                COMPREPLY=( $(compgen -W "--help" -- "${cur}") )
                ;;
            install-mcp-server)
                COMPREPLY=( $(compgen -W "--no-vscode --help" -- "${cur}") )
                ;;
            set-global)
                COMPREPLY=( $(compgen -W "--rpm-base-url --itrack-base-url --help" -- "${cur}") )
                ;;
        esac
        return
    fi

    # Value completions for specific flags
    case "${prev}" in
        --todo)
            local ids
            ids=$(_projot_open_todo_ids)
            COMPREPLY=( $(compgen -W "${ids}" -- "${cur}") )
            ;;
        --key)
            COMPREPLY=( $(compgen -W "teams itrack rpm other" -- "${cur}") )
            ;;
    esac
}

complete -F _projot projot
