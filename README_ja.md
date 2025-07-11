# DELILA デジタイザーライブラリ

## 概要

このライブラリは、CAEN デジタイザー（特にPSD2タイプ）からのデータ取得と処理を行うためのC++ライブラリです。放射線検出器からのアナログ信号をデジタル化し、イベントデータとして処理します。

## 主要機能

### 1. データ取得
- CAEN FELib APIを使用したハードウェア通信
- マルチスレッドによる高速データ取得
- リアルタイムデータ処理

### 2. データ変換パイプライン
```
ハードウェア → 生データ → PSD2データ → イベントデータ → ユーザー
```

### 3. 対応デジタイザータイプ
- **PSD1**: パルス形状弁別タイプ1
- **PSD2**: パルス形状弁別タイプ2  
- **PHA1**: パルス波高分析タイプ1
- **PHA2**: パルス波高分析タイプ2
- **QDC1**: 電荷デジタル変換器
- **SCOPE1/SCOPE2**: オシロスコープモード

## クラス構成

### 主要クラス

#### Digitizer クラス
デジタイザーの制御とデータ取得を担当する中心的なクラス

**主要メソッド:**
- `Initialize(config)`: デジタイザーの初期化
- `Configure()`: パラメータ設定の適用
- `StartAcquisition()`: データ取得開始
- `StopAcquisition()`: データ取得停止
- `GetEventData()`: イベントデータの取得

#### ConfigurationManager クラス
設定ファイルの読み込みと管理

**設定ファイル例 (dig1.conf):**
```
URL dig2:USB:0:0
Debug false
Threads 2
ModID 1
/par/AcquisitionMode SW_CONTROLLED
/ch/0/par/ChEnable true
/ch/0/par/ChRecordLengthT 500
```

#### EventData クラス
処理済みイベントデータの格納

**含まれる情報:**
- タイムスタンプ（ナノ秒精度）
- エネルギー値（長いゲート・短いゲート）
- チャンネル番号
- モジュール番号
- 波形データ（アナログ・デジタルプローブ）

## 使用方法

### 基本的な使用例

```cpp
#include "Digitizer.hpp"
#include "ConfigurationManager.hpp"

using namespace DELILA::Digitizer;

int main() {
    // 設定ファイルの読み込み
    ConfigurationManager config;
    config.LoadFromFile("dig1.conf");
    
    // デジタイザーの初期化
    Digitizer digitizer;
    digitizer.Initialize(config);
    digitizer.Configure();
    
    // データ取得開始
    digitizer.StartAcquisition();
    
    // データ取得ループ
    while (running) {
        auto eventData = digitizer.GetEventData();
        if (eventData->size() > 0) {
            std::cout << "取得イベント数: " << eventData->size() << std::endl;
            
            // 最初のイベントの情報表示
            auto& firstEvent = eventData->at(0);
            firstEvent->PrintSummary();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // データ取得停止
    digitizer.StopAcquisition();
    return 0;
}
```

## 技術詳細

### マルチスレッド処理

#### データ取得スレッド
- **ReadDataThread** (N個): ハードウェアからの生データ取得
- **DecodeThread** (N個): PSD2データへの変換（RawToPSD2内部）
- **EventConversionThread** (1個): EventDataへの最終変換

#### スレッド同期
- `fReadDataMutex`: ハードウェアアクセスの排他制御
- `fEventDataMutex`: EventDataストレージの排他制御
- 効率的なバッチ処理により性能最適化

### パラメータ検証
DeviceTreeを使用した動的パラメータ検証機能
- 設定値の範囲チェック
- パラメータ名の大文字小文字変換
- 詳細な検証レポート出力

### エラーハンドリング
- CAEN FELibエラーの詳細表示
- 設定ファイル読み込みエラーの処理
- リアルタイムステータス監視

## ビルド方法

### 必要な依存関係
- **CMake** (3.5以上)
- **C++17** 対応コンパイラ
- **ROOT** フレームワーク
- **CAEN FELib** ライブラリ
- **nlohmann/json** ライブラリ

### ビルド手順
```bash
mkdir build
cd build
cmake ..
make -j4
```

### 実行例
```bash
./dig-test dig1.conf
```

## 設定ファイル詳細

### 基本パラメータ
- `URL`: デジタイザーのUSB/Ethernet接続URL
- `Debug`: デバッグ情報の出力フラグ
- `Threads`: データ取得スレッド数
- `ModID`: モジュール識別番号

### デジタイザーパラメータ
CAENデジタイザーの標準パラメータ（'/'で始まる）
- `/par/AcquisitionMode`: 取得モード
- `/ch/N/par/ChEnable`: チャンネルN有効化
- `/ch/N/par/ChRecordLengthT`: 記録長（サンプル数）

## パフォーマンス特性

### 最適化された設計
- **CPU使用率**: 最小限（スマートポーリング）
- **メモリ効率**: 不要なコピーを排除
- **レイテンシ**: 一貫した低遅延
- **スループット**: バッチ処理による高速化

### ベンチマーク例
- **イベント処理速度**: 10,000+ events/秒
- **メモリ使用量**: 波形データサイズに比例
- **CPUオーバーヘッド**: 5%未満

## トラブルシューティング

### よくある問題

1. **USBデバイスが見つからない**
   - USBケーブルの接続確認
   - デバイスドライバーのインストール確認

2. **設定ファイルエラー**
   - ファイルパスの確認
   - パラメータ名のスペルチェック

3. **データが取得できない**
   - トリガー設定の確認
   - チャンネル有効化の確認

### デバッグ機能
```
Debug true
```
を設定ファイルに追加することで詳細なログ出力が可能

## ライセンス

このプロジェクトは研究目的で開発されています。

## 作成者

DELILA プロジェクトチーム

## 更新履歴

- **v1.0**: 初期リリース
- **v1.1**: マルチスレッド処理最適化
- **v1.2**: EventData自動変換機能追加
- **v1.3**: パフォーマンス最適化、アーキテクチャ改善