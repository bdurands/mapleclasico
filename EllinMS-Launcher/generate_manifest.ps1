$sourceDir = "C:\EllinMS"
$outputFile = "C:\EllinMS\downloads.xml"
$baseUrl = "https://cdn.ellinms.com/"

if (-not (Test-Path $sourceDir)) {
    Write-Output "Directory $sourceDir does not exist."
    exit
}

$xmlWriter = New-Object System.Xml.XmlTextWriter($outputFile, [System.Text.Encoding]::UTF8)
$xmlWriter.Formatting = [System.Xml.Formatting]::Indented
$xmlWriter.Indentation = 4

$xmlWriter.WriteStartDocument()
$xmlWriter.WriteStartElement("downloads")

$files = Get-ChildItem -Path $sourceDir -File -Recurse

foreach ($file in $files) {
    if ($file.Name -eq "downloads.xml") { continue }
    
    $relPath = $file.FullName.Substring($sourceDir.Length + 1).Replace('\', '/')
    $size = $file.Length
    
    # Calculate MD5
    $md5 = [System.Security.Cryptography.MD5]::Create()
    $stream = [System.IO.File]::OpenRead($file.FullName)
    $hashBytes = $md5.ComputeHash($stream)
    $stream.Close()
    
    $hashHex = ([System.BitConverter]::ToString($hashBytes) -replace '-', '').ToLower()
    $url = $baseUrl + $relPath -replace ' ', '%20'
    
    $xmlWriter.WriteStartElement("file")
    
    $xmlWriter.WriteElementString("file_name", $relPath)
    $xmlWriter.WriteElementString("file_hash", $hashHex)
    $xmlWriter.WriteElementString("file_link", $url)
    $xmlWriter.WriteElementString("file_size", $size.ToString())
    
    $xmlWriter.WriteEndElement()
}

$xmlWriter.WriteEndElement()
$xmlWriter.WriteEndDocument()
$xmlWriter.Close()

Write-Output "Successfully generated downloads.xml at $outputFile"
