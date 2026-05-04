$completionPath = Join-Path $PSScriptRoot 'projot.ps1'
$profileLine = ". '$completionPath'"

if (!(Test-Path $PROFILE)) { New-Item -Force -Path $PROFILE | Out-Null }

$profileContent = Get-Content $PROFILE -Raw -ErrorAction SilentlyContinue
if (-not ($profileContent -and $profileContent.Contains($completionPath))) {
    Add-Content -Path $PROFILE -Value "`n$profileLine"
    Write-Host "projot: added tab completion to $PROFILE"
}
