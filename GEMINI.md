# Geminiコードアシスタント用コンテキスト

このドキュメントは、Geminiコードアシスタントが **WindowAction (Multi-Window Action Game)** プロジェクトを効果的に理解し、貢献するための包括的なガイドです。

## 言語ルール

- **必須**: このプロジェクトに関するすべての出力は、必ず日本語で行う必要があります。

## プロジェクト概要

- **プロジェクト名**: WindowAction (Multi-Window Action Game)
- **ジャンル**: 2Dアクションパズルゲーム
- **プラットフォーム**: Windows OS
- **技術スタック**: C# .NET 6.0, Windows Forms

### ゲームコンセプト

WindowActionは、ユーザーのデスクトップ上の複数のウィンドウをまたいでゲームプレイが展開される、革新的な2Dアクションゲームです。プレイヤーキャラクターは、プラットフォームとして機能するさまざまなアプリケーションウィンドウ間をジャンプできます。このゲームはデスクトップ環境と統合されており、デスクトップアイコンとのインタラクションや衝突が可能です。主なゲームプレイの仕組みには、物理ベースの移動（重力、ジャンプ）や、ゲームウィンドウの直接操作（移動、リサイズ、最小化）によるパズル解決が含まれます。

## アーキテクチャ

このプロジェクトは、関心の分離と依存性の注入を重視した最新のC#アーキテクチャで構築されています。

- **依存性の注入 (DI)**: カスタムDIコンテナ（`ServiceContainer`および`ServiceRegistration`）を使用して、すべての主要コンポーネント（サービス、マネージャー、ファクトリー）のライフサイクルと依存関係を管理します。
- **システムベースアーキテクチャ**: `SystemManager`が、`InputSystem`や`RenderingSystem`などの優先順位付けされたシステムを通じて、コアなゲームロジックを統括します。
- **コンポーネントベースデザイン**: `Player`は、それぞれが単一の責任を持つ複数の異なるコンポーネントで構成される複合エンティティです。
    - `IPlayerPhysics`: 重力、ジャンプ、移動計算を処理します。
    - `IPlayerInputHandler`: キーボード入力を処理します。
    - `IPlayerWindowInteraction`: `GameWindow`インスタンス間のインタラクションと遷移を管理します。
    - `IPlayerStateMachine`: プレイヤーの状態（例：Jumping, Falling, Grounded）を管理します。
- **ストラテジーパターン**: 各`GameWindow`の振る舞いは、割り当てられた`IWindowStrategy`（例：`ResizableWindowStrategy`, `MovableWindowStrategy`, `NoEntryWindowStrategy`）によって決定されます。これにより、多様なパズル要素を作成できます。
- **ファクトリーパターン**: ファクトリー（`PlayerFormFactory`, `WindowFactory`, `ButtonFactory`）が複雑なゲームオブジェクトのインスタンス化を担当し、すべての依存関係が正しく注入されることを保証します。

## コアシステム

- **`Program.cs`**: アプリケーションのエントリーポイント。DIコンテナを初期化し、すべてのサービスを登録し、メインゲームを起動します。
- **`MainGame.cs`**: ゲームの中央オーケストレーター。メインの`RunGameLoopAsync`を含み、ゲームの状態（一時停止、デバッグモード）を管理し、すべてのアクティブなシステムとゲームオブジェクトの更新メソッドを呼び出します。
- **`StageManager.cs`**: ゲームステージのライフサイクルを管理します。ウィンドウのレイアウト、プレイヤーの開始位置、ゴール、その他のステージ固有の要素を定義するステージデータをロードします。
- **`WindowManager.cs`**: すべての`GameWindow`インスタンスを管理します。これには、Zオーダー、親子関係、衝突検出が含まれます。

## ビルドと実行

プロジェクトのルートディレクトリから.NET CLIを使用してビルドおよび実行します。

### ビルド
```bash
# ソリューションをビルドする（デフォルトはDebug構成）
dotnet build MultiWindowActionGame.sln

# リリース用にビルドする
dotnet build MultiWindowActionGame.sln --configuration Release
```

### 実行
```bash
# プロジェクトディレクトリからゲームを実行する
dotnet run --project MultiWindowActionGame
```

### デバッグ
- ゲーム内で**F3**キーを押すことで、デバッグオーバーレイの表示を切り替えることができます。プレイヤーの物理情報、パフォーマンスメトリクス、その他の有用なデータが表示されます。

## 開発規約

- **言語**: C# 10。
- **依存関係の管理**: すべての重要なコンポーネントは`ServiceContainer`に登録し、そこから解決する必要があります。可能な限り静的インスタンスやシングルトンを避け、コンストラクタインジェクションを優先します。
- **非同期コード**: メインゲームループと更新メソッドは非同期（`async Task`）です。長時間の操作は、UIスレッドをブロックしないように`await`する必要があります。
- **設定**: ゲーム設定（物理、コントロールなど）は`config/settings.json`で管理され、`GameSettings`クラスを介してロードされます。
- **機能の追加**:
    - **新しいウィンドウタイプ**: `IWindowStrategy`を実装する新しいクラスを作成し、対応する`WindowType`列挙型を追加し、`WindowFactory`に登録します。
    - **新しいサービス**: `Interfaces/`にインターフェースを定義し、実装クラスを作成し、`DI/ServiceRegistration.cs`に登録します。
    - **新しいステージ**: `StageManager.InitializeStages()`のリストに`StageData`オブジェクトを追加します。