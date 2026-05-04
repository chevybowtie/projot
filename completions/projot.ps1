# projot PowerShell completion
# Dot-source this file from your $PROFILE:
#   . "$HOME\Documents\projot\completions\projot.ps1"

Register-ArgumentCompleter -Native -CommandName projot -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $elements = $commandAst.CommandElements
    $subcommand = if ($elements.Count -ge 2) { $elements[1].ToString() } else { $null }

    $subcommands = @(
        'init', 'new', 'add-todo', 'list', 'complete', 'add-note',
        'set-link', 'set-app-id', 'add-github', 'add-swagger', 'add-blizzard',
        'render', 'install-hook'
    )

    # Helper: get open todo IDs from the notes file
    function Get-ProjotOpenIds {
        try {
            $root = & git rev-parse --show-toplevel 2>$null
            if (-not $root) { return @() }
            $config = Join-Path $root '.projot\config'
            $rpmLine = Get-Content $config -ErrorAction Stop | Where-Object { $_ -match '^rpm\s*=' } | Select-Object -First 1
            $rpm = ($rpmLine -replace '^rpm\s*=\s*', '').Trim()
            $notes = Join-Path $root ".projot\$rpm.md"
            (Get-Content $notes -ErrorAction Stop) |
                Where-Object { $_ -match '^\d+\. \[ \]' } |
                ForEach-Object { ($_ -split '\.')[0] }
        } catch { @() }
    }

    # No subcommand yet — complete subcommands and global flags
    if (-not $subcommand -or $subcommand -eq $wordToComplete) {
        $candidates = $subcommands + @('--help', '--version')
        return $candidates |
            Where-Object { $_ -like "$wordToComplete*" } |
            ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
    }

    # Determine what flag we're completing a value for
    $prevToken = if ($elements.Count -ge 2) { $elements[$elements.Count - 2].ToString() } else { $null }

    if ($prevToken -eq '--todo') {
        $ids = Get-ProjotOpenIds
        return $ids |
            Where-Object { $_ -like "$wordToComplete*" } |
            ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', "Todo ID $_") }
    }

    if ($prevToken -eq '--key') {
        return @('teams', 'itrack', 'rpm', 'other') |
            Where-Object { $_ -like "$wordToComplete*" } |
            ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
    }

    # Flag completion per subcommand
    $flags = switch ($subcommand) {
        'init'         { @('--app-id', '--github', '--swagger', '--blizzard', '--help') }
        'new'          { @('--rpm', '--name', '--itrack', '--teams', '--rpm-url', '--itrack-url', '--other', '--no-hook', '--help') }
        'add-todo'     { @('--help') }
        'list'         { @('--open', '--closed', '--all', '--help') }
        'complete'     { @('--todo', '--help') }
        'add-note'     { @('--todo', '--text', '--help') }
        'set-link'     { @('--key', '--url', '--help') }
        'set-app-id'   { @('--app-id', '--force', '--help') }
        'add-github'   { @('--url', '--help') }
        'add-swagger'  { @('--url', '--help') }
        'add-blizzard' { @('--url', '--help') }
        'render'       { @('--help') }
        'install-hook' { @('--help') }
        default        { @() }
    }

    $flags |
        Where-Object { $_ -like "$wordToComplete*" } |
        ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
}
