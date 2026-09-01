<#
.SYNOPSIS
    Cortex の依存関係セットアップスクリプト。

.DESCRIPTION
    1. git submodule (llama.cpp / curl / json / DirectX-Headers) を取得・更新する
    2. llama.cpp を CMake (Visual Studio 17 2022 / x64 / Release) でビルドする
       -> Application プロジェクトが参照する .lib / .dll を生成する
    3. (任意) curl を静的ライブラリとしてビルドする

    json と DirectX-Headers はヘッダオンリーのため取得のみでビルド不要。
    インクルードパスはリポジトリ直下の Directory.Build.props が全 .vcxproj へ一括で通す。

.PARAMETER NoCuda
    llama.cpp を CUDA 無し (CPU のみ) でビルドする。既定は CUDA 有効。
    ※ CUDA 有効ビルドには NVIDIA CUDA Toolkit が必要。

.PARAMETER BuildCurl
    curl も併せてビルドする (既定はスキップ)。

.PARAMETER Clean
    llama.cpp / curl の build ディレクトリを削除してから構成し直す。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -NoCuda -BuildCurl
#>
[CmdletBinding()]
param(
    [switch]$NoCuda,
    [switch]$BuildCurl,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

function Write-Step($msg) { Write-Host "`n==== $msg ====" -ForegroundColor Cyan }
function Require-Command($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "'$name' が PATH に見つかりません。インストールするか、'Developer PowerShell for VS 2022' から実行してください。"
    }
}

Require-Command git
Require-Command cmake

# ----------------------------------------------------------------------
Write-Step "1/3  git submodule を取得・更新"
git submodule sync --recursive
git submodule update --init --recursive --progress
git submodule status --recursive

# ----------------------------------------------------------------------
Write-Step "2/3  llama.cpp をビルド (Visual Studio 17 2022 / x64 / Release)"

$LlamaSrc   = Join-Path $RepoRoot 'LocalLLM\llama.cpp'
$LlamaBuild = Join-Path $LlamaSrc 'build'

if ($Clean -and (Test-Path $LlamaBuild)) {
    Write-Host "  build ディレクトリを削除: $LlamaBuild"
    Remove-Item -Recurse -Force $LlamaBuild
}

$cudaFlag = if ($NoCuda) { 'OFF' } else { 'ON' }
if (-not $NoCuda -and -not (Get-Command nvcc -ErrorAction SilentlyContinue)) {
    Write-Warning "nvcc が見つかりません。CUDA Toolkit 未導入の場合は -NoCuda を付けて再実行してください。"
}

$llamaArgs = @(
    '-S', $LlamaSrc,
    '-B', $LlamaBuild,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    '-DBUILD_SHARED_LIBS=ON',      # llama / ggml を DLL 化 (Application が実行時に参照)
    "-DGGML_CUDA=$cudaFlag",
    '-DLLAMA_CURL=OFF',            # モデル取得は使わない (HTTP は WinHTTP 実装)
    '-DLLAMA_BUILD_COMMON=ON',     # llama-common(-base).lib が必要
    '-DLLAMA_BUILD_TESTS=OFF',
    '-DLLAMA_BUILD_EXAMPLES=OFF',
    '-DLLAMA_BUILD_TOOLS=OFF',
    '-DLLAMA_BUILD_SERVER=OFF'
)
cmake @llamaArgs
cmake --build $LlamaBuild --config Release --parallel

Write-Host "`n  生成された .lib:" -ForegroundColor Green
Get-ChildItem -Recurse -Filter *.lib $LlamaBuild |
    Where-Object { $_.Name -match '^(llama|ggml|ggml-base|ggml-cpu|ggml-cuda|llama-common|llama-common-base)\.lib$' } |
    ForEach-Object { "    " + $_.FullName.Substring($RepoRoot.Length + 1) }

# ----------------------------------------------------------------------
Write-Step "3/3  curl"
if ($BuildCurl) {
    $CurlSrc   = Join-Path $RepoRoot 'Curl\curl'
    $CurlBuild = Join-Path $CurlSrc 'build'
    if ($Clean -and (Test-Path $CurlBuild)) { Remove-Item -Recurse -Force $CurlBuild }
    cmake -S $CurlSrc -B $CurlBuild -G 'Visual Studio 17 2022' -A x64 `
        -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DCURL_USE_SCHANNEL=ON `
        -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_LIBIDN2=OFF
    cmake --build $CurlBuild --config Release --parallel
} else {
    Write-Host "  スキップ (-BuildCurl 未指定)。curl はまだソースから参照されていません。"
}

Write-Step "完了"
Write-Host @"
次の手順:
  1. Visual Studio で Cortex.sln を開く
  2. 構成を  Release / x64  に設定
  3. Application プロジェクトをビルド & 実行

NuGet 復元は不要になりました (nlohmann/json と d3dx12.h は submodule + Directory.Build.props 経由)。
"@ -ForegroundColor Green
