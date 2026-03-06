# Clear Windows icon/thumbnail cache and restart Explorer

# 1. Delete icon cache databases
$cachePath = "$env:LOCALAPPDATA\Microsoft\Windows\Explorer"
$cacheFiles = Get-ChildItem -Path $cachePath -Filter "IconCache*" -ErrorAction SilentlyContinue
$thumbFiles = Get-ChildItem -Path $cachePath -Filter "thumbcache_*" -ErrorAction SilentlyContinue

Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800

foreach ($f in ($cacheFiles + $thumbFiles)) {
    Remove-Item $f.FullName -Force -ErrorAction SilentlyContinue
    Write-Host "Deleted: $($f.Name)"
}

# 2. Restart Explorer
Start-Process explorer
Start-Sleep -Milliseconds 600

# 3. ie4uinit refresh
ie4uinit.exe -show 2>$null

Write-Host "Done. Taskbar icons should refresh."
