# projot PowerShell completion
# Add to your PowerShell profile with:
#   . "path/to/projot.ps1"
# Or install via projot's installer

$projotSubcommands = @(
    'init', 'new', 'close', 'add', 'add-todo', 'list', 'links', 'complete', 'status', 'summarize', 'add-note',
    'set-link', 'set-app-id', 'add-github', 'add-swagger', 'add-blizzard',
    'add-azure', 'render', 'install-hook', 'uninstall-hook',
    'install-mcp-server', 'uninstall-mcp-server', 'set-global', 'set-teams-webhook', 'set-teams-post'
)

$projotFlags = @{
    'init'                 = @('--app-id', '--github', '--swagger', '--blizzard', '--help')
    'new'                  = @('--rpm', '--name', '--jira', '--teams', '--teams-sync-url', '--teams-webhook', '--rpm-url', '--jira-url', '--other', '--no-hook', '--help')
    'close'                = @('--help')
    'add'                  = @('--help')
    'add-todo'             = @('--help')
    'list'                 = @('--open', '--closed', '--all', '--help')
    'links'                = @('--help')
    'complete'             = @('--todo', '--help')
    'status'               = @('--todo', '--help')
    'summarize'            = @('--today', '--help')
    'add-note'             = @('--todo', '--help')
    'set-link'             = @('--key', '--url', '--help')
    'set-app-id'           = @('--app-id', '--force', '--help')
    'add-github'           = @('--url', '--help')
    'add-swagger'          = @('--url', '--help')
    'add-blizzard'         = @('--url', '--help')
    'add-azure'            = @('--type', '--name', '--url', '--help')
    'render'               = @('--help')
    'install-hook'         = @('--help')
    'uninstall-hook'       = @('--help')
    'install-mcp-server'   = @('--no-vscode', '--help')
    'uninstall-mcp-server' = @('--no-vscode', '--help')
    'set-global'           = @('--rpm-base-url', '--jira-base-url', '--help')
    'set-teams-webhook'    = @('--help')
    'set-teams-post'       = @('--render', '--summarize', '--help')
}

$projotFlagValues = @{
    '--key'   = @('teams', 'jira', 'rpm', 'other')
    '--type'  = @('subscription', 'key-vault', 'resource-group', 'aks', 'log-analytics', 'storage', 'private-dns')
    '--render'    = @('on', 'off')
    '--summarize' = @('on', 'off')
    '--open'  = @()
    '--closed'= @()
    '--all'   = @()
    '--no-hook' = @()
    '--no-vscode' = @()
    '--force' = @()
}

# Emit open todo IDs from the current project (mirrors the other shells' helpers)
function Get-ProjotOpenTodoIds {
    $root = git rev-parse --show-toplevel 2>$null
    if (-not $root) { return }
    $config = Join-Path $root '.projot/config'
    if (-not (Test-Path $config)) { return }
    $rpmLine = Select-String -Path $config -Pattern '^rpm\s*=' | Select-Object -First 1
    if (-not $rpmLine) { return }
    $rpm = ($rpmLine.Line -replace '^rpm\s*=\s*', '').Trim()
    $notes = Join-Path $root ".projot/$rpm.md"
    if (-not (Test-Path $notes)) { return }
    Select-String -Path $notes -Pattern '^(\d+)\. \[ \]' | ForEach-Object { $_.Matches[0].Groups[1].Value }
}

# Tooltips shown next to flag values (the other shells show these as descriptions)
$projotValueTooltips = @{
    'subscription'   = 'Subscriptions'
    'key-vault'      = 'Key Vaults'
    'resource-group' = 'Resource Groups'
    'aks'            = 'AKS Clusters'
    'log-analytics'  = 'Log Analytics'
    'storage'        = 'Storage Containers'
    'private-dns'    = 'Private DNS Zones'
}

Register-ArgumentCompleter -CommandName projot -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $ast = $commandAst.ToString()
    $elements = $commandAst.CommandElements

    # Get the current word being completed
    $word = $wordToComplete

    # Find which subcommand (if any) is being used
    $subcommand = $null
    foreach ($i in 1..($elements.Count - 1)) {
        $elem = $elements[$i].Value
        if ($elem -in $projotSubcommands) {
            $subcommand = $elem
            break
        }
    }

    # If we haven't found a subcommand yet and word looks like a subcommand
    if (-not $subcommand -and -not $word.StartsWith('-')) {
        $completions = $projotSubcommands | Where-Object { $_ -like "$word*" }
        $completions | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
        return
    }

    # If no subcommand, show top-level options
    if (-not $subcommand) {
        $topLevel = @('--help', '--version') + $projotSubcommands
        $completions = $topLevel | Where-Object { $_ -like "$word*" }
        $completions | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
        return
    }

    # Word starts with -: complete flags for this subcommand
    if ($word.StartsWith('-')) {
        if ($projotFlags.ContainsKey($subcommand)) {
            $flags = $projotFlags[$subcommand]
            $completions = $flags | Where-Object { $_ -like "$word*" }
            $completions | ForEach-Object {
                [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
            }
        }
        return
    }

    # Determine if we're completing a flag value
    # An empty word is not part of the AST, so the flag is then the last element
    $previousElement = $null
    if ($word -eq '' -and $elements.Count -ge 1) {
        $previousElement = $elements[-1].Value
    } elseif ($elements.Count -ge 2) {
        $previousElement = $elements[-2].Value
    }

    if ($previousElement -eq '--todo') {
        Get-ProjotOpenTodoIds | Where-Object { $_ -like "$word*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
        return
    }

    if ($projotFlagValues.ContainsKey($previousElement)) {
        $values = $projotFlagValues[$previousElement]
        if ($values.Count -gt 0) {
            $completions = $values | Where-Object { $_ -like "$word*" }
            $completions | ForEach-Object {
                $tip = if ($projotValueTooltips.ContainsKey($_)) { $projotValueTooltips[$_] } else { $_ }
                [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $tip)
            }
        }
        return
    }

    # Positional status value for `status` (after --todo <ID>)
    if ($subcommand -eq 'status' -and $previousElement -ne '--todo') {
        $statuses = @('todo', 'in-progress', 'blocked', 'done') | Where-Object { $_ -like "$word*" }
        $statuses | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    }
}
