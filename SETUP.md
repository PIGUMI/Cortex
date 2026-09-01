# Cortex セットアップ手順

Windows / C++20 / DirectX12 + llama.cpp のデスクトップアプリ。
ビルドは **Visual Studio 2022 の `Cortex.sln`**、外部依存は **git submodule + CMake** で用意する。
（NuGet は廃止済み。）

---

## 1. 必要なもの

| ツール | 用途 | 備考 |
|---|---|---|
| Visual Studio 2022 (v143) | 本体ビルド | ワークロード「C++ によるデスクトップ開発」/ 最新 Windows 10 or 11 SDK |
| Git | submodule 取得 | |
| CMake 3.24+ | llama.cpp / curl のビルド | VS 同梱のもので可 (`C:\Program Files\CMake` でも可) |
| NVIDIA CUDA Toolkit | llama.cpp の GPU 実行 | **任意**。無い場合は CPU ビルド (`-NoCuda`)。既定は CPU ビルド |

---

## 2. 一発セットアップ

リポジトリ直下で PowerShell を開いて:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup.ps1
```

やること:

1. `git submodule update --init --recursive` で全 submodule を取得
2. `LocalLLM/llama.cpp` を CMake でビルド（`Visual Studio 17 2022` / `x64` / `Release` / CUDA 有効）
   → `Application` がリンクする `llama.lib` / `ggml*.lib` / `llama-common*.lib` と実行時 DLL を生成
3. json / DirectX-Headers は**ヘッダオンリー**なので取得のみ（ビルド不要）

オプション:

```powershell
# CUDA 有効でビルド（要 CUDA Toolkit）
powershell -ExecutionPolicy Bypass -File scripts\setup.ps1

# CUDA 無し（CPU のみ）★既定。CUDA Toolkit 未導入ならこちら
powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -NoCuda

# curl も静的ライブラリでビルド
powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -BuildCurl

# build ディレクトリを消してクリーン構成
powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -Clean
```

> CUDA ビルドは 15〜40 分かかることがある。`nvcc` が PATH に無いと警告が出る。

### CPU ビルド ⇄ CUDA ビルドの切り替え

`Application` が `ggml-cuda.lib` をリンクするかどうかは
**`Directory.Build.props` の `<CortexLlamaCuda>`** で決まる（既定 `false` = CPU）。

- CUDA を使う場合: `<CortexLlamaCuda>true</CortexLlamaCuda>` にして
  `scripts\setup.ps1`（`-NoCuda` 無し）で llama.cpp を再ビルド。
- `Application.vcxproj` 側は編集不要（llama.cpp のリンク設定は `Directory.Build.props` に集約済み）。

---

## 3. ビルド & 実行

1. Visual Studio で `Cortex.sln` を開く
2. 構成を **`Release` / `x64`** にする（llama.cpp を Release でビルドしているため）
3. スタートアッププロジェクトを **`Application`** にして F5

`Application` はビルド後に `LocalLLM/llama.cpp/build/bin/Release/*.dll` を出力フォルダへコピーする
（`Application.vcxproj` の `CopyLlamaRuntimeDlls` ターゲット）。

---

## 4. submodule 一覧

| パス | リポジトリ | 用途 | ビルド |
|---|---|---|---|
| `LocalLLM/llama.cpp` | ggml-org/llama.cpp | LLM 推論 (`BaseLLM.cpp` が `llama.h` を使用) | CMake で必要 |
| `Curl/curl` | curl/curl | 将来用（現状ソース未参照） | 任意 |
| `ThirdParty/json` | nlohmann/json | `<nlohmann/json.hpp>` | ヘッダオンリー |
| `ThirdParty/DirectX-Headers` | microsoft/DirectX-Headers | `<d3d12.h>` / `<d3dx12.h>`（旧 `Microsoft.Direct3D.D3D12` NuGet 相当・タグ `v1.619.5`） | ヘッダオンリー |

### インクルードパスの通し方

リポジトリ直下の **`Directory.Build.props`** を MSBuild が全 `.vcxproj` へ自動 import し、
以下を `AdditionalIncludeDirectories` に追加している:

```
ThirdParty\json\single_include
ThirdParty\DirectX-Headers\include\directx   ← <d3d12.h> / <d3dx12.h>
ThirdParty\DirectX-Headers\include
```

そのため各プロジェクトファイルを個別にいじる必要はない。

---

## 5. 新しい submodule を足すには（手順の一般形）

```powershell
# 追加（--depth 1 で履歴を浅く）
git submodule add --depth 1 https://github.com/<owner>/<repo>.git ThirdParty/<name>

# ヘッダオンリーなら Directory.Build.props の <AdditionalIncludeDirectories> に
#   $(CortexRoot)ThirdParty\<name>\include
# を足すだけ。

# CMake ライブラリなら scripts\setup.ps1 にビルド手順を追記し、
# 生成される .lib のパスを対象 .vcxproj の <AdditionalLibraryDirectories> /
# <AdditionalDependencies> に追加する（llama.cpp が実例）。

git commit -am "deps: add <name> submodule"
```

クローンした人は:

```powershell
git submodule update --init --recursive
```

submodule のコミットを更新したいとき:

```powershell
cd ThirdParty/<name>
git fetch && git checkout <tag-or-commit>
cd ../..
git add ThirdParty/<name>
git commit -m "deps: bump <name> to <tag>"
```

---

## 6. トラブルシューティング

**`d3d12.h` の再定義 / コンパイルエラー**
`Directory.Build.props` が DirectX-Headers 版 `d3d12.h` を Windows SDK 版より優先させている
（旧 Agility SDK NuGet と同じ挙動）。SDK 版に戻したい場合は `Directory.Build.props` から
`...\include\directx` の行を消す（`d3dx12.h` は `include\directx` 側にしか無いので、その場合は
ソースの `#include <d3dx12.h>` を `#include <directx/d3dx12.h>` に変える）。

**`nvcc` が無い / CUDA でビルドが止まる**
`scripts\setup.ps1 -NoCuda -Clean` で CPU ビルドし直す。
`<CortexLlamaCuda>` が `false`（既定）なら `Application` は `ggml-cuda.lib` をリンクしないので
プロジェクトファイルの編集は不要。

**`llama.lib` が見つからない (LNK1104)**
llama.cpp のビルドが未完了か Debug でビルドしている。`Release / x64` で `scripts\setup.ps1` を再実行。

**submodule が空**
`git submodule update --init --recursive` を実行。プロキシ環境では git の HTTPS 設定を確認。

**`scripts\setup.ps1` が文字化けエラー**
ファイルは UTF-8 BOM 付き。エディタで保存し直す場合は BOM を保持する
（Windows PowerShell 5.1 は BOM 無し UTF-8 の日本語コメントを Shift-JIS 誤認する）。
または `pwsh`（PowerShell 7）で実行する。
