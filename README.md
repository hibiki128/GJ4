# GJ4

自作エンジン [Hagine](https://github.com/hibiki128/engine-development) をサブモジュールとして参照するアプリケーションリポジトリ。

## 構成

```
GJ4/
├── app/                アプリ本体（app.slnx を Visual Studio で開く）
│   ├── src/            ゲームのソース
│   ├── Assets/         アプリ側アセット (images / jsons / models / sounds)
│   └── Application/    設定ファイル (imgui のレイアウト ini など)
├── automation/         vcxproj のフィルタ生成・成果物コピー用スクリプト
└── module/
    ├── Hagine/         エンジン（サブモジュール）
    └── Generated/      エンジンのビルド成果物（git 管理外）
```

## クローン

サブモジュールを含めて取得する。

```sh
git clone --recursive https://github.com/hibiki128/GJ4.git
```

`--recursive` を忘れた場合は次を実行する。

```sh
git submodule update --init --recursive
```

## ビルド

`app/app.slnx` を Visual Studio 2022 以降で開き、`x64` / `Debug` または `Release` でビルドする。

## エンジンの更新

サブモジュールを最新にして、その参照コミットをアプリ側にコミットする。

```sh
git -C module/Hagine pull origin main
git add module/Hagine
git commit -m "update: エンジンを最新に更新"
```

## フィルタの再生成

`app/src` にファイルを追加・移動したら実行する（`app.vcxproj.filters` を作り直す）。

```sh
automation/Run-Generate-VcxprojFilters.bat
```
