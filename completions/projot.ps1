# projot PowerShell completion
# Add to your PowerShell profile with:
#   . "path/to/projot.ps1"
# Or install via projot's installer

$projotSubcommands = @(
    'init', 'new', 'close', 'add-todo', 'list', 'complete', 'add-note',
    'set-link', 'set-app-id', 'add-github', 'add-swagger', 'add-blizzard',
    'add-azure', 'render', 'install-hook', 'install-mcp-server', 'set-global'
)

$projotFlags = @{
    'init'               = @('--app-id', '--github', '--swagger', '--blizzard', '--help')
    'new'                = @('--rpm', '--name', '--itrack', '--teams', '--rpm-url', '--itrack-url', '--other', '--no-hook', '--help')
    'close'              = @('--help')
    'add-todo'           = @('--help')
    'list'               = @('--open', '--closed', '--all', '--help')
    'complete'           = @('--todo', '--help')
    'add-note'           = @('--todo', '--text', '--help')
    'set-link'           = @('--key', '--url', '--help')
    'set-app-id'         = @('--app-id', '--force', '--help')
    'add-github'         = @('--url', '--help')
    'add-swagger'        = @('--url', '--help')
    'add-blizzard'       = @('--url', '--help')
    'add-azure'          = @('--type', '--name', '--url', '--help')
    'render'             = @('--help')
    'install-hook'       = @('--help')
    'install-mcp-server' = @('--no-vscode', '--help')
    'set-global'         = @('--rpm-base-url', '--itrack-base-url', '--help')
}

$projotFlagValues = @{
    '--key'   = @('teams', 'itrack', 'rpm', 'other')
    '--type'  = @('subscription', 'key-vault', 'resource-group', 'aks', 'log-analytics', 'storage', 'private-dns')
    '--open'  = @()
    '--closed'= @()
    '--all'   = @()
    '--no-hook' = @()
    '--no-vscode' = @()
    '--force' = @()
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
    $previousElement = $null
    if ($elements.Count -ge 2) {
        $previousElement = $elements[-2].Value
    }

    if ($projotFlagValues.ContainsKey($previousElement)) {
        $values = $projotFlagValues[$previousElement]
        if ($values.Count -gt 0) {
            $completions = $values | Where-Object { $_ -like "$word*" }
            $completions | ForEach-Object {
                [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
            }
        }
    }
}
