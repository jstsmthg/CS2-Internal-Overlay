<#
.SYNOPSIS
Interactive CS2 Workshop Grenade Importer for Antigravity Cheat.
Scans the CS2 Steam Workshop directory for KV3 Grenade Guides and converts them to .csv
#>

$workshopDir = "D:\SteamLibrary\steamapps\workshop\content\730"
if (-not (Test-Path $workshopDir)) {
    $workshopDir = "C:\Program Files (x86)\Steam\steamapps\workshop\content\730"
}
if (-not (Test-Path $workshopDir)) {
    Write-Host "Could not find CS2 workshop directory!" -ForegroundColor Red
    exit
}

Write-Host "Scanning Steam Workshop for CS2 Grenade Guides..." -ForegroundColor Cyan

$files = Get-ChildItem -Path $workshopDir -Recurse -Filter "*.txt" | Where-Object { $_.Length -gt 10KB }
$guides = @()

foreach ($f in $files) {
    # Check if KV3
    $firstLine = Get-Content $f.FullName -TotalCount 1
    if ($firstLine -match "kv3 encoding") {
        # Fast extraction of map name
        $mapName = "unknown"
        $content = Get-Content $f.FullName -TotalCount 50
        foreach ($l in $content) {
            if ($l -match '^\s*MapName\s*=\s*"([^"]+)"') {
                $mapName = $matches[1]
                break
            }
        }
        
        # Get Workshop ID from path
        $id = $f.Directory.Name
        $publishData = Join-Path $f.Directory.FullName "publish_data.txt"
        $title = $id
        if (Test-Path $publishData) {
            $titleContent = Get-Content $publishData -Raw
            if ($titleContent -match '"title"\s*:\s*"([^"]+)"') {
                $title = $matches[1]
            }
        }
        
        $guides += @{
            Path = $f.FullName
            Map = $mapName
            Title = $title
        }
    }
}

if ($guides.Count -eq 0) {
    Write-Host "No CS2 grenade guides found in workshop!" -ForegroundColor Yellow
    exit
}

Write-Host "`nFound $($guides.Count) Workshop Guides:" -ForegroundColor Green
for ($i = 0; $i -lt $guides.Count; $i++) {
    Write-Host "[$($i+1)] $($guides[$i].Map) - $($guides[$i].Title)"
}

Write-Host "`nWhich guides would you like to import? (e.g., '1', '1,3', or 'all')" -ForegroundColor Cyan
$choice = Read-Host ">"

if ([string]::IsNullOrWhiteSpace($choice)) { exit }

$selectedGuides = @()
if ($choice -eq 'all') {
    $selectedGuides = $guides
} else {
    $indices = $choice.Split(',') | ForEach-Object { $_.Trim() }
    foreach ($idx in $indices) {
        $num = [int]$idx
        if ($num -gt 0 -and $num -le $guides.Count) {
            $selectedGuides += $guides[$num - 1]
        }
    }
}

$outputDir = $PSScriptRoot
if (-not (Test-Path $outputDir)) { New-Item -ItemType Directory -Path $outputDir | Out-Null }

function Convert-GrenadeType($type) {
    switch ($type.ToLower()) {
        "smoke" { return 1 }
        "flash" { return 2 }
        "molotov" { return 3 }
        "he" { return 4 }
        default { return 0 }
    }
}

foreach ($g in $selectedGuides) {
    Write-Host "`nImporting $($g.Title) ($($g.Map))..." -ForegroundColor Cyan
    $lines = Get-Content $g.Path

    $nodes = @{}
    $currentNode = $null
    $currentBlock = $null

    foreach ($line in $lines) {
        if ($line -match '^\s*MapAnnotationNode\d+\s*=') {
            if ($currentNode -and $currentNode.Id) { $nodes[$currentNode.Id] = $currentNode }
            $currentNode = @{ Id = [guid]::NewGuid().ToString() }
            $currentBlock = "Node"
        } elseif ($line -match '^\s*Title\s*=') {
            $currentBlock = "Title"
        } elseif ($line -match '^\s*Desc\s*=') {
            $currentBlock = "Desc"
        } elseif ($line -match '^\s*\}\s*$') {
            if ($currentBlock -ne "Node") { $currentBlock = "Node" }
        }
        
        if (-not $currentNode) { continue }

        if ($line -match '^\s*Id\s*=\s*"([^"]+)"') { $currentNode.Id = $matches[1] }
        elseif ($line -match '^\s*SubType\s*=\s*"([^"]+)"') { $currentNode.SubType = $matches[1] }
        elseif ($line -match '^\s*MasterNodeId\s*=\s*"([^"]+)"') { $currentNode.MasterNodeId = $matches[1] }
        elseif ($line -match '^\s*Position\s*=\s*\[\s*([^,]+),\s*([^,]+),\s*([^\]]+)\]') {
            $currentNode.Position = @($matches[1].Trim(), $matches[2].Trim(), $matches[3].Trim())
        }
        elseif ($line -match '^\s*Angles\s*=\s*\[\s*([^,]+),\s*([^,]+),\s*([^\]]+)\]') {
            $currentNode.Angles = @($matches[1].Trim(), $matches[2].Trim(), $matches[3].Trim())
        }
        elseif ($line -match '^\s*GrenadeType\s*=\s*"([^"]+)"') { $currentNode.GrenadeType = $matches[1] }
        elseif ($line -match '^\s*Text\s*=\s*"(.*)"') {
            if ($currentBlock -eq "Title") { $currentNode.Title = $matches[1] }
            if ($currentBlock -eq "Desc") { $currentNode.Desc = $matches[1] }
        }
    }
    if ($currentNode -and $currentNode.Id) { $nodes[$currentNode.Id] = $currentNode }

    $mainNodes = $nodes.Values | Where-Object { $_.SubType -eq 'main' }
    $outFile = Join-Path $outputDir "$($g.Map).csv"
    $count = 0

    foreach ($main in $mainNodes) {
        $aimId = $nodes.Keys | Where-Object { $nodes[$_].MasterNodeId -eq $main.Id -and $nodes[$_].SubType -eq 'aim_target' }
        if ($aimId) {
            $aim = $nodes[$aimId]
            
            $typeId = Convert-GrenadeType $main.GrenadeType
            $name = $main.Title
            if ([string]::IsNullOrWhiteSpace($name)) { $name = "Spot" }
            
            $desc = $aim.Desc
            if ($desc -match '(?i)W\s*\+\s*JUMP' -or $desc -match '(?i)RUN.*JUMP' -or $desc -match '(?i)FORWARD.*JUMP') {
                $name += " (WJT)"
            } elseif ($desc -match '(?i)JUMP') {
                $name += " (JT)"
            }

            # Check if this precise position/angle already exists in the CSV to avoid massive duplicates
            $duplicate = $false
            $csvLine = "$typeId,$name,$($main.Position[0]),$($main.Position[1]),$($main.Position[2]),$($aim.Angles[0]),$($aim.Angles[1])"
            
            if (Test-Path $outFile) {
                $existing = Get-Content $outFile
                foreach ($ex in $existing) {
                    if ($ex -eq $csvLine) {
                        $duplicate = $true
                        break
                    }
                }
            }
            
            if (-not $duplicate) {
                Add-Content -Path $outFile -Value $csvLine
                $count++
            }
        }
    }
    
    Write-Host "Successfully imported $count new spots to $($g.Map).csv" -ForegroundColor Green
}

Write-Host "`nAll done! Press any key to exit."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
