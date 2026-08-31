<#
.SYNOPSIS
    テキストファイルに列挙した相対パスを、指定ディレクトリへ一括コピーする。

.DESCRIPTION
    リストファイルの書式:
      - '#' で始まる行はコメント、空行は無視。
      - コメント/空行を除いた最初の行 = リポジトリルート。
        絶対パスならそのまま、相対パスならスクリプト位置 ($PSScriptRoot) 基準で解決。
      - 以降の行 = ルートからの相対パス (ファイル or ディレクトリ)。ワイルドカード可。
    コピー先には末端の名前のみで配置する (中間階層は落とす)。
    ディレクトリは再帰的にコピーする。

.PARAMETER Destination
    コピー先ディレクトリ。存在しなければ作成する。

.PARAMETER ListFile
    リストファイルパス。既定は 'copy_list.txt' ($PSScriptRoot 基準)。

.EXAMPLE
    pwsh Copy-FileList.ps1 D:\out
    pwsh Copy-FileList.ps1 D:\out -ListFile mylist.txt
#>
[CmdletBinding()]
Param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Destination,
    [string]$ListFile = "copy_list.txt"
)

Set-Location -Path $PSScriptRoot

Write-Host ">> $(Split-Path -Leaf $MyInvocation.MyCommand.Path) ----------------------------------------"

# --- リストファイルの解決 ---
if (-not [System.IO.Path]::IsPathRooted($ListFile)) {
    $ListFile = Join-Path $PSScriptRoot $ListFile
}
if (-not (Test-Path -Path $ListFile -PathType Leaf)) {
    Write-Host "Error: List file not found: $ListFile" -ForegroundColor DarkRed
    exit 1
}

# --- 行を読み込み、コメント/空行を除外 ---
$lines = @(Get-Content -Path $ListFile |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -ne "" -and -not $_.StartsWith("#") })

if ($lines.Count -lt 1) {
    Write-Host "Error: List file has no repository root line." -ForegroundColor DarkRed
    exit 1
}

# --- 先頭行 = リポジトリルート ---
$repoRoot = $lines[0]
if (-not [System.IO.Path]::IsPathRooted($repoRoot)) {
    $repoRoot = Join-Path $PSScriptRoot $repoRoot
}
$resolvedRoot = Resolve-Path -Path $repoRoot -ErrorAction SilentlyContinue
if (-not $resolvedRoot) {
    Write-Host "Error: Repository root not found: $repoRoot" -ForegroundColor DarkRed
    exit 1
}
$repoRoot = $resolvedRoot.Path
Write-Host "Repository root : $repoRoot"
Write-Host "Destination     : $Destination"

# --- コピー先の作成 ---
if (-not (Test-Path -Path $Destination)) {
    New-Item -ItemType Directory -Path $Destination | Out-Null
    Write-Host "Created destination directory: $Destination"
}

# --- コピー処理 ---
$patterns = $lines | Select-Object -Skip 1
$copied = 0
$missing = 0

foreach ($rel in $patterns) {
    $pattern = Join-Path $repoRoot $rel
    $items = Resolve-Path -Path $pattern -ErrorAction SilentlyContinue
    if (-not $items) {
        Write-Host "  [WARN] not found: $rel" -ForegroundColor Yellow
        $missing++
        continue
    }
    foreach ($it in $items) {
        Copy-Item -Path $it.Path -Destination $Destination -Recurse -Force
        Write-Host "  copied: $($it.Path)"
        $copied++
    }
}

Write-Host "Done. Copied $copied item(s), $missing pattern(s) not found."
