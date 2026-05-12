# projot fish completion
# Place in ~/.config/fish/completions/projot.fish

# Disable file completion for projot by default
complete -c projot -f

# Helper: emit open todo IDs from the current project
function __projot_open_todo_ids
    set -l root (git rev-parse --show-toplevel 2>/dev/null)
    or return
    set -l config "$root/.projot/config"
    set -l rpm (grep -m1 '^rpm\s*=' "$config" 2>/dev/null | sed 's/.*=\s*//' | string trim)
    set -l notes "$root/.projot/$rpm.md"
    grep -oE '^[0-9]+\. \[ \]' "$notes" 2>/dev/null | grep -oE '^[0-9]+'
end

function __projot_no_subcommand
    not __fish_seen_subcommand_from init new close add-todo list complete add-note \
        set-link set-app-id add-github add-swagger add-blizzard add-azure render \
        install-hook install-mcp-server set-global
end

# Top-level flags
complete -c projot -n __projot_no_subcommand -l help    -d 'Show help'
complete -c projot -n __projot_no_subcommand -l version -d 'Show version'

# Subcommands
complete -c projot -n __projot_no_subcommand -a init          -d 'Initialize projot for this repository'
complete -c projot -n __projot_no_subcommand -a new           -d 'Start a new RPM project'
complete -c projot -n __projot_no_subcommand -a close         -d 'Archive current project and reset'
complete -c projot -n __projot_no_subcommand -a add-todo      -d 'Append a new todo'
complete -c projot -n __projot_no_subcommand -a list          -d 'Show project summary and todos'
complete -c projot -n __projot_no_subcommand -a complete      -d 'Mark a todo completed'
complete -c projot -n __projot_no_subcommand -a add-note      -d 'Add a note to a todo'
complete -c projot -n __projot_no_subcommand -a set-link      -d 'Set or update a single-value link URL'
complete -c projot -n __projot_no_subcommand -a set-app-id    -d 'Set the application ID'
complete -c projot -n __projot_no_subcommand -a add-github    -d 'Add a GitHub URL'
complete -c projot -n __projot_no_subcommand -a add-swagger   -d 'Add a Swagger URL'
complete -c projot -n __projot_no_subcommand -a add-blizzard      -d 'Add a Blizzard URL'
complete -c projot -n __projot_no_subcommand -a add-azure        -d 'Add an Azure resource'
complete -c projot -n __projot_no_subcommand -a render           -d 'Re-render notes file and stage it'
complete -c projot -n __projot_no_subcommand -a install-hook     -d 'Install the pre-commit git hook'
complete -c projot -n __projot_no_subcommand -a install-mcp-server -d 'Configure MCP server'
complete -c projot -n __projot_no_subcommand -a set-global       -d 'Set global defaults'

# --help on every subcommand
for sub in init new close add-todo list complete add-note set-link set-app-id \
           add-github add-swagger add-blizzard add-azure render install-hook \
           install-mcp-server set-global
    complete -c projot -n "__fish_seen_subcommand_from $sub" -l help -d 'Show help'
end

# init
complete -c projot -n '__fish_seen_subcommand_from init' -l app-id   -d 'Application ID' -r
complete -c projot -n '__fish_seen_subcommand_from init' -l github   -d 'GitHub URL' -r
complete -c projot -n '__fish_seen_subcommand_from init' -l swagger  -d 'Swagger URL' -r
complete -c projot -n '__fish_seen_subcommand_from init' -l blizzard -d 'Blizzard URL' -r

# new
complete -c projot -n '__fish_seen_subcommand_from new' -l rpm       -d 'RPM project number' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l name      -d 'Project name' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l itrack    -d 'iTrack ticket number' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l teams     -d 'Teams channel URL' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l rpm-url   -d 'RPM system link' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l itrack-url -d 'iTrack link' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l other     -d 'Other URL' -r
complete -c projot -n '__fish_seen_subcommand_from new' -l no-hook   -d 'Skip hook installation'

# add-todo (accepts a positional description argument)

# list
complete -c projot -n '__fish_seen_subcommand_from list' -l open   -d 'Open todos only'
complete -c projot -n '__fish_seen_subcommand_from list' -l closed -d 'Closed todos only'
complete -c projot -n '__fish_seen_subcommand_from list' -l all    -d 'All todos'

# complete
complete -c projot -n '__fish_seen_subcommand_from complete' -l todo \
    -d 'Todo ID' -r -a '(__projot_open_todo_ids)'

# add-note
complete -c projot -n '__fish_seen_subcommand_from add-note' -l todo \
    -d 'Todo ID' -r -a '(__projot_open_todo_ids)'
complete -c projot -n '__fish_seen_subcommand_from add-note' -l text -d 'Note text' -r

# set-link
complete -c projot -n '__fish_seen_subcommand_from set-link' -l key \
    -d 'Link key' -r -a 'teams itrack rpm other'
complete -c projot -n '__fish_seen_subcommand_from set-link' -l url -d 'URL' -r

# set-app-id
complete -c projot -n '__fish_seen_subcommand_from set-app-id' -l app-id -d 'Application ID' -r
complete -c projot -n '__fish_seen_subcommand_from set-app-id' -l force  -d 'Overwrite existing value'

# add-github / add-swagger / add-blizzard
for sub in add-github add-swagger add-blizzard
    complete -c projot -n "__fish_seen_subcommand_from $sub" -l url -d 'URL' -r
end

# add-azure
complete -c projot -n '__fish_seen_subcommand_from add-azure' -l type -d 'Resource type' -r \
    -a 'subscription\tSubscription key-vault\tKeyVault resource-group\tResourceGroup aks\tAKS log-analytics\tLogAnalytics storage\tStorage private-dns\tPrivateDNS'
complete -c projot -n '__fish_seen_subcommand_from add-azure' -l name -d 'Resource name' -r
complete -c projot -n '__fish_seen_subcommand_from add-azure' -l url -d 'URL' -r

# install-mcp-server
complete -c projot -n '__fish_seen_subcommand_from install-mcp-server' -l no-vscode -d 'Skip VS Code configuration'

# set-global
complete -c projot -n '__fish_seen_subcommand_from set-global' -l rpm-base-url -d 'Base URL for RPM links' -r
complete -c projot -n '__fish_seen_subcommand_from set-global' -l itrack-base-url -d 'Base URL for iTrack links' -r
