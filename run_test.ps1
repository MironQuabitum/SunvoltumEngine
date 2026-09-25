Set-Location "c:\MediaWork\Sunvoltum\Internal\SunvoltumEngine\out\build\x64-Release\SunvoltumServer"
$proc = Start-Process -FilePath ".\SunvoltumServer.exe" -PassThru -RedirectStandardOutput "server_out.log" -RedirectStandardError "server_err.log"
Start-Sleep -Seconds 4
if ($proc -and -not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
}
Start-Sleep -Milliseconds 500

$lines = Get-Content "server_out.log"
$lines | Select-String -Pattern "SyncOut|Init OK|RegisterJoint|LockUpright" | Select-Object -First 30
