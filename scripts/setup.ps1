<#
.SYNOPSIS
    Cortex の依存関係セットアップスクリプト。

.DESCRIPTION
    1. git submodule (json / DirectX-Headers / DirectXTex / assimp / llama.cpp / curl) を取得・更新する  ← 常に実行
    2. assimp を CMake (Visual Studio 17 2022 / x64 / Debug + Release) で静的ライブラリとしてビルドする  ← 常に実行
       -> モデル読み込みに必須。Cortex.sln のビルド前に一度実行しておくこと。
    3. (任意) LocalLLM/llama.cpp を CMake (Visual Studio 17 2022 / x64 / Release) でビルドする
       -> BaseLLM クラスを使う場合のみ必要。-Llama を付けたときだけ実行。
    4. (任意) curl を静的ライブラリとしてビルドする  (-BuildCurl)

    json / DirectX-Headers はヘッダオンリーのため取得のみでビルド不要。
    DirectXTex は Manager が .vcxproj を ProjectReference しているため CMake ビルド不要。
    インクルードパスはリポジトリ直下の Directory.Build.props が全 .vcxproj へ一括で通す。

.PARAMETER Llama
    LocalLLM/llama.cpp をビルドする。既定ではスキップ (Cortex.sln は llama.cpp 無しでビルド可能)。

.PARAMETER Cuda
    llama.cpp を CUDA 有効でビルドする (要 NVIDIA CUDA Toolkit / nvcc)。既定は CPU のみ。

.PARAMETER BuildCurl
    curl も併せてビルドする。

.PARAMETER Clean
    assimp / llama.cpp / curl の build ディレクトリを削除してから構成し直す。

.EXAMPLE
    # submodule 取得のみ
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1

.EXAMPLE
    # llama.cpp も CPU ビルド
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -Llama

.EXAMPLE
    # llama.cpp を CUDA ビルド
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -Llama -Cuda
#>
[CmdletBinding()]
param(
    [switch]$Llama,
    [switch]$Cuda,
    [switch]$BuildCurl,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false) } catch {}
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

function Write-Step($msg) { Write-Host "`n==== $msg ====" -ForegroundColor Cyan }
function Require-Command($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "'$name' が PATH に見つかりません。インストールするか 'Developer PowerShell for VS 2022' から実行してください。"
    }
}
function Invoke-Native {
    param([Parameter(Mandatory)][string]$Exe, [Parameter(ValueFromRemainingArguments)][string[]]$Args)
    & $Exe @Args
    if ($LASTEXITCODE -ne 0) { throw "$Exe が終了コード $LASTEXITCODE で失敗しました。" }
}

Require-Command git

# ----------------------------------------------------------------------
Write-Step "git submodule を取得・更新"
Invoke-Native git submodule sync --recursive
Invoke-Native git submodule update --init --recursive --progress
git submodule status --recursive

# ----------------------------------------------------------------------
Write-Step "共通ネイティブ依存のビルド準備"
Require-Command cmake

# CMake (4.0.x) は非 ASCII パスの VS ジェネレーターでクラッシュする
if ($RepoRoot -match '[^\u0000-\u007F]') {
    throw @"
リポジトリのパスに非 ASCII 文字が含まれています:
  $RepoRoot
CMake の Visual Studio ジェネレーターはこのパスでクラッシュします。
対処:
  - リポジトリを C:\dev\Cortex などへ移動する (推奨)
  - もしくは  subst X: "<ASCII 親フォルダ>"  で ASCII ドライブを割り当て、X:\Cortex から再実行
詳細は README「事前条件：リポジトリを ASCII のみのパスに置く」。
"@
}

# ----------------------------------------------------------------------
Write-Step "assimp をビルド (Visual Studio 17 2022 / x64 / Debug + Release)"

$AssimpSrc   = Join-Path $RepoRoot 'ThirdParty\assimp'
$AssimpBuild = Join-Path $AssimpSrc 'build'
if ($Clean -and (Test-Path $AssimpBuild)) {
    Write-Host "  build ディレクトリを削除: $AssimpBuild"
    Remove-Item -Recurse -Force $AssimpBuild
}

# 静的ライブラリ / FBX + glTF + OBJ の import・export のみ / 独自 zlib 同梱。
# assimp.lib と zlibstatic.lib を build\lib\<Config>\ へまとめて出力し、
# ライブラリ名にツールセット接尾辞 (-vc143-mt) や Debug 接尾辞 (d) を付けない。
Invoke-Native cmake `
    -S $AssimpSrc -B $AssimpBuild -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=$AssimpBuild\lib" `
    -DBUILD_SHARED_LIBS=OFF `
    -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_BUILD_SAMPLES=OFF `
    -DASSIMP_INSTALL=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF `
    -DASSIMP_BUILD_ZLIB=ON `
    -DASSIMP_INJECT_DEBUG_POSTFIX=OFF "-DLIBRARY_SUFFIX=" `
    -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF `
    -DASSIMP_BUILD_OBJ_IMPORTER=ON -DASSIMP_BUILD_FBX_IMPORTER=ON -DASSIMP_BUILD_GLTF_IMPORTER=ON `
    -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF `
    -DASSIMP_BUILD_OBJ_EXPORTER=ON -DASSIMP_BUILD_FBX_EXPORTER=ON -DASSIMP_BUILD_GLTF_EXPORTER=ON
Invoke-Native cmake --build $AssimpBuild --config Debug   --parallel
Invoke-Native cmake --build $AssimpBuild --config Release --parallel

Write-Host "`n  生成された .lib:" -ForegroundColor Green
Get-ChildItem -Recurse -Filter *.lib $AssimpBuild |
    Where-Object { $_.Name -match '^(assimp|zlibstatic)\.lib$' } |
    ForEach-Object { "    " + $_.FullName.Substring($RepoRoot.Length + 1) }

if (-not $Llama) {
    Write-Step "完了 (submodule + assimp)"
    Write-Host @"
Cortex.sln は Release / x64 でそのままビルドできます (assimp はビルド済み)。
ローカル LLM (BaseLLM / llama.cpp) を使う場合は -Llama を付けて再実行してください。
詳細は README「LocalLLM / llama.cpp のビルド」。
"@ -ForegroundColor Green
    return
}

# ----------------------------------------------------------------------
Write-Step "llama.cpp をビルド (Visual Studio 17 2022 / x64 / Release)"

$LlamaSrc   = Join-Path $RepoRoot 'LocalLLM\llama.cpp'
$LlamaBuild = Join-Path $LlamaSrc 'build'
if ($Clean -and (Test-Path $LlamaBuild)) {
    Write-Host "  build ディレクトリを削除: $LlamaBuild"
    Remove-Item -Recurse -Force $LlamaBuild
}

$cudaFlag = if ($Cuda) { 'ON' } else { 'OFF' }
if ($Cuda -and -not (Get-Command nvcc -ErrorAction SilentlyContinue)) {
    throw "nvcc が見つかりません。CUDA Toolkit を導入するか -Cuda を外して実行してください。"
}

Invoke-Native cmake `
    -S $LlamaSrc -B $LlamaBuild -G 'Visual Studio 17 2022' -A x64 `
    -DBUILD_SHARED_LIBS=ON `
    "-DGGML_CUDA=$cudaFlag" `
    -DLLAMA_CURL=OFF `
    -DLLAMA_BUILD_COMMON=ON `
    -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF `
    -DLLAMA_BUILD_TOOLS=OFF -DLLAMA_BUILD_SERVER=OFF
Invoke-Native cmake --build $LlamaBuild --config Release --parallel

Write-Host "`n  生成された .lib:" -ForegroundColor Green
Get-ChildItem -Recurse -Filter *.lib $LlamaBuild |
    Where-Object { $_.Name -match '^(llama|ggml|ggml-base|ggml-cpu|ggml-cuda|llama-common|llama-common-base)\.lib$' } |
    ForEach-Object { "    " + $_.FullName.Substring($RepoRoot.Length + 1) }

# ----------------------------------------------------------------------
if ($BuildCurl) {
    Write-Step "curl をビルド"
    $CurlSrc   = Join-Path $RepoRoot 'Curl\curl'
    $CurlBuild = Join-Path $CurlSrc 'build'
    if ($Clean -and (Test-Path $CurlBuild)) { Remove-Item -Recurse -Force $CurlBuild }
    Invoke-Native cmake -S $CurlSrc -B $CurlBuild -G 'Visual Studio 17 2022' -A x64 `
        -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DCURL_USE_SCHANNEL=ON `
        -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_LIBIDN2=OFF
    Invoke-Native cmake --build $CurlBuild --config Release --parallel
}

Write-Step "完了"
Write-Host @"
次の手順:
  1. Directory.Build.props で  <CortexUseLlama> を true$(if ($Cuda) { ' 、<CortexLlamaCuda> も true' })  にする
     (または  msbuild Cortex.sln /p:CortexUseLlama=true$(if ($Cuda) { ' /p:CortexLlamaCuda=true' }) )
  2. Visual Studio で Cortex.sln を開き、Release / x64 で Application をビルド & 実行
"@ -ForegroundColor Green
