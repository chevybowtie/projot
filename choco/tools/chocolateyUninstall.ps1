$completionPath = Join-Path $PSScriptRoot 'projot.ps1'

if (Test-Path $PROFILE) {
    $lines = Get-Content $PROFILE | Where-Object { $_ -notmatch [regex]::Escape($completionPath) }
    Set-Content $PROFILE $lines
    Write-Host "projot: removed tab completion from $PROFILE"
}
