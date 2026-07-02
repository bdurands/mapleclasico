$baseDir    = $PSScriptRoot
$mobWzDir   = "$baseDir\wz\Mob.wz"
$stringFile = "$baseDir\wz\String.wz\Mob.img.xml"
$outputCsv  = "$baseDir\mobs_por_nivel.csv"

Write-Host "Leyendo nombres de mobs..." -ForegroundColor Cyan
[xml]$strXml = Get-Content $stringFile -Encoding UTF8
$names = @{}
foreach ($mob in $strXml.imgdir.imgdir) {
    $mobId = $mob.name
    $nameNode = $mob.string | Where-Object { $_.name -eq "name" }
    if ($nameNode) { $names[$mobId] = $nameNode.value }
}
Write-Host "Nombres cargados: $($names.Count)" -ForegroundColor Green

Write-Host "Leyendo stats de mobs..." -ForegroundColor Cyan
$mobs = @()
Get-ChildItem -Path $mobWzDir -Filter "*.img.xml" | ForEach-Object {
    $mobId = $_.Name -replace '\.img\.xml$', ''
    try {
        [xml]$xml = Get-Content $_.FullName -Encoding UTF8 -ErrorAction Stop
        $info = $xml.imgdir.imgdir | Where-Object { $_.name -eq "info" }
        if ($info) {
            $level = ($info.int | Where-Object { $_.name -eq "level" }).value
            $maxHP  = ($info.int | Where-Object { $_.name -eq "maxHP"  }).value
            $exp    = ($info.int | Where-Object { $_.name -eq "exp"    }).value
            $boss   = ($info.int | Where-Object { $_.name -eq "boss"   }).value
            if ($level -and [int]$level -ge 30 -and [int]$level -le 100) {
                $mobName = if ($names.ContainsKey($mobId)) { $names[$mobId] } else { "(ID: $mobId)" }
                $isBoss  = if ($boss -eq "1") { "SI" } else { "NO" }
                $lvl     = [int]$level
                # Calcular rango
                $rango = switch ($lvl) {
                    {$_ -ge 30 -and $_ -le 35} { "30-35"  }
                    {$_ -ge 36 -and $_ -le 40} { "36-40"  }
                    {$_ -ge 41 -and $_ -le 45} { "41-45"  }
                    {$_ -ge 46 -and $_ -le 50} { "46-50"  }
                    {$_ -ge 51 -and $_ -le 55} { "51-55"  }
                    {$_ -ge 56 -and $_ -le 60} { "56-60"  }
                    {$_ -ge 61 -and $_ -le 65} { "61-65"  }
                    {$_ -ge 66 -and $_ -le 70} { "66-70"  }
                    {$_ -ge 71 -and $_ -le 75} { "71-75"  }
                    {$_ -ge 76 -and $_ -le 80} { "76-80"  }
                    {$_ -ge 81 -and $_ -le 85} { "81-85"  }
                    {$_ -ge 86 -and $_ -le 90} { "86-90"  }
                    {$_ -ge 91 -and $_ -le 95} { "91-95"  }
                    {$_ -ge 96 -and $_ -le 100}{ "96-100" }
                    default                     { "otro"   }
                }
                $mobs += [PSCustomObject]@{
                    Rango  = $rango
                    Nombre = $mobName
                    ID     = $mobId
                    Nivel  = $lvl
                    HP     = if ($maxHP) { [int]$maxHP } else { 0 }
                    EXP    = if ($exp)   { [int]$exp   } else { 0 }
                    Boss   = $isBoss
                }
            }
        }
    } catch {}
}
Write-Host "Mobs cargados: $($mobs.Count)" -ForegroundColor Green

# Ordenar por rango y nivel
$sorted = $mobs | Sort-Object Rango, Nivel, EXP

# Exportar CSV con separador punto y coma (compatible con Excel en español)
$sorted | Export-Csv -Path $outputCsv -NoTypeInformation -Encoding UTF8 -Delimiter ";"

Write-Host ""
Write-Host "==> Excel listo en: $outputCsv" -ForegroundColor Green
Write-Host "Total mobs: $($sorted.Count)" -ForegroundColor Cyan
