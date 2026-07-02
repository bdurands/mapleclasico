$results = @()
$mobDir = "$PSScriptRoot\wz\Mob.wz"

Get-ChildItem -Path $mobDir -Filter "*.img.xml" | ForEach-Object {
    $file = $_.Name
    $mobId = $file -replace '\.img\.xml$', ''
    try {
        [xml]$xml = Get-Content $_.FullName -Encoding UTF8 -ErrorAction Stop
        $info = $xml.imgdir.imgdir | Where-Object { $_.name -eq "info" }
        if ($info) {
            $level = ($info.int | Where-Object { $_.name -eq "level" }).value
            $maxHP  = ($info.int | Where-Object { $_.name -eq "maxHP"  }).value
            $exp    = ($info.int | Where-Object { $_.name -eq "exp"    }).value
            if ($exp -and [int]$exp -gt 0) {
                $results += [PSCustomObject]@{
                    MobID = $mobId
                    Level = if ($level) { $level } else { "?" }
                    MaxHP = if ($maxHP)  { $maxHP  } else { "?" }
                    EXP   = [int]$exp
                }
            }
        }
    } catch {}
}

$sorted = $results | Sort-Object EXP

# Mostrar en consola
$sorted | Format-Table -AutoSize

# Exportar a CSV
$csvPath = "$PSScriptRoot\mobs_exp.csv"
$sorted | Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8
Write-Host "`n==> Exportado a: $csvPath" -ForegroundColor Green
Write-Host "Total mobs con EXP: $($sorted.Count)" -ForegroundColor Cyan
