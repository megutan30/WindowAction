# WindowAction プロジェクト仕様書

このドキュメントはClaude Code（claude.ai/code）がこのリポジトリで作業する際の包括的なガイドです。

## 出力言語と設計原則

**出力言語**: 日本語

**必須原則**:
- **YAGNI原則** (You Aren't Gonna Need It): 必要になるまで機能を実装しない
- **DRY原則** (Don't Repeat Yourself): コードの重複を避ける
- **KISS原則** (Keep It Simple, Stupid): シンプルに保つ

仕様変更や追加の際にはClaude.mdに反映
---

## プロジェクト概要

### 基本情報
- **プロジェクト名**: WindowAction (Multi-Window Action Game)
- **ジャンル**: 2Dアクションパズルゲーム
- **プラットフォーム**: Windows OS専用
- **技術スタック**: C# .NET 6.0, Windows Forms
- **開発開始**: 2024年
- **最終更新**: 2025年1月

### ゲームコンセプト
WindowActionは、従来のゲームの枠を超えた革新的な2Dアクションパズルゲームです。プレイヤーは複数のウィンドウ間を移動し、デスクトップ環境そのものと相互作用します。

**特徴**:
1. **マルチウィンドウシステム**: 複数のウィンドウが同時に表示され、それぞれが独立したプラットフォーム
2. **デスクトップ統合**: デスクトップアイコンやシステムウィンドウとの衝突判定
3. **物理ベースのゲームプレイ**: 重力、ジャンプ、慣性を持つリアルな物理演算
4. **ウィンドウ操作**: プレイヤーがウィンドウを移動、リサイズ、最小化してパズルを解く

---

## ビルドと実行

### プロジェクトのビルド
```bash
# ソリューション全体をビルド
dotnet build MultiWindowActionGame.sln

# Debugモードでビルド（デフォルト）
dotnet build MultiWindowActionGame.sln --configuration Debug

# Releaseモードでビルド
dotnet build MultiWindowActionGame.sln --configuration Release
```

### アプリケーションの実行
```bash
# プロジェクトディレクトリから実行
dotnet run --project MultiWindowActionGame

# 特定の構成で実行
dotnet run --project MultiWindowActionGame --configuration Debug
```

### テストとデバッグ
- **現在のテスト方法**: 手動テスト（ゲームプレイを通じて）
- **自動テストフレームワーク**: 未設定
- **デバッグモード**: F3キーでデバッグ表示のON/OFF切り替え

---

## アーキテクチャ設計

### 全体アーキテクチャ

```
┌─────────────────────────────────────────────────────────┐
│                   Program (Entry Point)                  │
│              - DIコンテナの初期化                         │
│              - MainGameの起動                            │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                   DI Container                           │
│  ServiceContainer + ServiceRegistration                  │
│  - すべてのサービスとマネージャーを管理                    │
│  - Singleton/Transientライフタイム                        │
└─────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  MainGame   │    │ SystemMgr   │    │ StageManager│
│  (Core)     │───▶│  (Systems)  │    │  (Stages)   │
└─────────────┘    └─────────────┘    └─────────────┘
        │                   │                   │
        │           ┌───────┴────────┐         │
        │           ▼                ▼         ▼
        │   ┌─────────────┐  ┌─────────────┐  │
        │   │InputSystem  │  │RenderSystem │  │
        │   └─────────────┘  └─────────────┘  │
        │                                      │
        ▼                                      ▼
┌─────────────────────┐            ┌─────────────────┐
│   Player System     │            │ Window System   │
│  - PlayerForm       │            │ - WindowManager │
│  - PlayerPhysics    │            │ - WindowFactory │
│  - PlayerInput      │            │ - GameWindow    │
│  - PlayerInteract   │            │ - Strategies    │
│  - PlayerState      │            └─────────────────┘
└─────────────────────┘
```

### アーキテクチャパターン
1. **依存性注入 (Dependency Injection)**
   - カスタムDIコンテナ (`ServiceContainer`) がすべての依存関係を管理
   - コンストラクタインジェクションパターン
   - Singleton/Transientライフタイム対応

2. **コンポーネントベースアーキテクチャ**
   - Playerシステムが5つのコンポーネントに分離
   - 各コンポーネントが単一責任を持つ

3. **ストラテジーパターン**
   - ウィンドウの挙動を戦略クラスで実装
   - 7種類の戦略（Normal, Resizable, Movable, Deletable, Minimizable, TextDisplay）

4. **ファクトリーパターン**
   - `WindowFactory`: ウィンドウの生成
   - `PlayerFormFactory`: プレイヤーの生成
   - `ButtonFactory`: UIボタンの生成

5. **システムベースアーキテクチャ**
   - `SystemManager`がゲームシステムのライフサイクルを管理
   - 優先度順実行（Input → Physics → Rendering）

---

## コアシステム詳細

### 1. 依存性注入システム (`DI/`)

#### ServiceContainer
カスタムDIコンテナの実装。

**機能**:
- Singletonライフタイム（アプリケーション全体で1つのインスタンス）
- Transientライフタイム（呼び出しごとに新しいインスタンス）
- 循環依存の検出と防止
- コンストラクタインジェクション

**重要な実装ノート**:
```csharp
// 登録
container.RegisterSingleton<IWindowManager, WindowManager>();
container.RegisterTransient<IPlayerPhysics, PlayerPhysics>();

// 解決
var windowManager = container.Resolve<IWindowManager>();
```

#### ServiceRegistration
すべてのサービス登録を一元管理。

**登録カテゴリ**:
1. **コアサービス**: Logger, ErrorHandler
2. **マネージャー**: WindowManager, StageManager, etc.
3. **プレイヤーコンポーネント**: PlayerPhysics, PlayerInput, etc.
4. **ゲーム設定**: GameSettings
5. **ユーティリティサービス**: InputService, GameTimeService, etc.
6. **システム管理**: SystemManager, InputSystem, RenderingSystem

**Fallbackパターン**:
```csharp
// 後方互換性のために、DIできない場合は静的参照にフォールバック
settings = gameSettings?.Window ?? GameSettings.Current.Window;
```

### 2. ゲームループとシステム管理 (`Core/`)

#### MainGame (IMainGame)
ゲーム全体を統括するメインコントローラー。

**責務**:
- ゲームの初期化とメインループ
- システム管理（SystemManager経由）
- プレイヤーの生成と管理
- ステージ管理（StageManager経由）
- ゲームの一時停止/再開

**主要メソッド**:
```csharp
Task InitializeAsync()              // 非同期初期化
Task RunGameLoopAsync()             // メインゲームループ
void InitializePlayer(Point start)  // プレイヤー生成
void PauseGame()                    // 一時停止
void ResumeGame()                   // 再開
void ToggleDebugMode()              // デバッグモード切り替え
```

**ゲームループの流れ**:
```csharp
while (ゲーム実行中)
{
    gameTimeService.Update();                          // 時間更新
    await systemManager.UpdateAllAsync(deltaTime);     // システム更新
    await UpdateAsync();                               // レガシー更新
    await Task.Delay(sleepTime);                       // フレームレート調整
}
```

#### SystemManager (ISystemManager)
ゲームシステムのライフサイクル管理。

**機能**:
- システムの登録と初期化
- 優先度順の更新実行
- 一括一時停止/再開
- システムのシャットダウン

**システム優先度**:
```csharp
Input    = 100  // 最初に実行（入力処理）
Physics  = 200  // 未使用（PlayerPhysicsが代替）
Rendering= 400  // 最後に実行（描画）
```

#### StageManager (IStageManager)
ステージシステムの管理。

**機能**:
- 15以上のステージ定義
- ステージの遷移とロード
- ウィンドウレイアウトの構築
- ゴール判定

**ステージ構成要素**:
- ウィンドウ配置（位置、サイズ、種類）
- プレイヤー開始位置
- ゴール位置
- 不可侵領域（NoEntryZone）
- UIボタン

### 3. ウィンドウ管理システム (`Windows/`)

#### WindowManager (IWindowManager)
すべてのウィンドウを統括管理。

**機能**:
- ウィンドウの登録と管理
- Z-order（重なり順）管理
- 親子関係の管理
- 衝突判定のサポート
- ウィンドウの一括表示/非表示

**主要メソッド**:
```csharp
void RegisterWindow(GameWindow window)
List<GameWindow> GetIntersectingWindows(Rectangle bounds)
void SetPlayer(PlayerForm player)
List<Button> GetAllButtons()
int GetWindowZIndex(GameWindow window)
```

#### GameWindow
ウィンドウの基底クラス。

**プロパティ**:
```csharp
Rectangle CollisionBounds    // 衝突判定用の境界
Rectangle AdjustedBounds     // スケール適用後の境界
Rectangle DisplayBounds      // 表示用の境界
IWindowStrategy Strategy     // 挙動戦略
List<IEffectTarget> Children // 子要素リスト
```

**重要な実装詳細**:
- 3種類の境界を使い分け（Collision, Adjusted, Display）
- 親ウィンドウとの相対座標系
- リサイズ時の子要素のスケーリング

#### WindowFactory (IWindowFactory)
ウィンドウの生成を担当。

**WindowType列挙型**:
```csharp
NormalBlack         // 通常ウィンドウ（黒）
NormalWhite         // 通常ウィンドウ（白）
Resizable           // リサイズ可能ウィンドウ
Movable             // 移動可能ウィンドウ
Deletable           // 削除可能ウィンドウ（Deleteキー）
Minimizable         // 最小化可能ウィンドウ
TextDisplay         // テキスト表示ウィンドウ
NoEntry             // 不可侵ウィンドウ
ResizableNoEntry    // リサイズ可能 + 不可侵ウィンドウ 🆕
MovableNoEntry      // 移動可能 + 不可侵ウィンドウ 🆕
MinimizableNoEntry  // 最小化可能 + 不可侵ウィンドウ 🆕
NormalBlackNoEntry  // 通常ウィンドウ（黒）+ 不可侵ウィンドウ 🆕
NormalWhiteNoEntry  // 通常ウィンドウ（白）+ 不可侵ウィンドウ 🆕
```

**生成例**:
```csharp
var window = windowFactory.CreateWindow(
    WindowType.Movable,
    new Point(100, 100),
    new Size(200, 150)
);
```

#### ウィンドウストラテジー (IWindowStrategy)
ウィンドウの挙動を定義する戦略パターン実装。

**基底クラス**: `BaseWindowStrategy`
```csharp
// 共通フィールド（DI対応）
protected readonly IInputService? inputService;
protected readonly IGameSettings? gameSettings;
protected readonly INoEntryZoneManager? noEntryZoneManager;
protected readonly WindowSettings settings;

// Fallbackパターン
settings = gameSettings?.Window ?? GameSettings.Current.Window;
```

**戦略クラス一覧**:

1. **NormalWindowStrategy**
   - 静的なプラットフォーム
   - 特別な挙動なし

2. **ResizableWindowStrategy** (WindowStrategies.cs:145-500)
   - **基本動作**: マウスドラッグでリサイズ可能

   - **処理フロー**:
     - `OnMouseDown`: リサイズ開始、originalSize/originalSizesを記録
     - `OnMouseUp`: リサイズ終了、キャッシュクリア
     - `Update`: マウス位置からCalculateNewSize → GetValidSize → スケール適用

   - **CalculateNewSize** (WindowStrategies.cs:182-200):
     ```csharp
     Size CalculateNewSize(GameWindow window, Point mousePos)
     {
         int deltaX = mousePos.X - lastMousePos.X;
         int deltaY = mousePos.Y - lastMousePos.Y;
         int newWidth = Math.Max(settings.MinimumSize.Width,
                                 Math.Min(originalSize.Width + deltaX, settings.MaximumSize.Width));
         int newHeight = Math.Max(settings.MinimumSize.Height,
                                  Math.Min(originalSize.Height + deltaY, settings.MaximumSize.Height));
         return new Size(newWidth, newHeight);
     }
     ```

   - **衝突回避**: `NoEntryZoneManager.GetValidSize(currentBounds, proposedSize, window)`
     - 不可侵領域との衝突を検出してサイズを制約
     - X軸とY軸を独立して処理（Y軸チェック時にX軸調整後の幅を使用）

   - **子要素のスケーリング** (WindowStrategies.cs:448-463):
     ```csharp
     RecordOriginalSizesRecursive(window);  // 再帰的に元サイズ記録
     SizeF scale = new(newWidth / originalWidth, newHeight / originalHeight);
     foreach (var child in window.Children)
     {
         Size childOriginal = originalSizes[child];
         Size childNew = new((int)(childOriginal.Width * scale.Width),
                             (int)(childOriginal.Height * scale.Height));
         child.Size = childNew;
         if (child is GameWindow childWindow)
             childWindow.OnParentResize(scale);  // 再帰伝播
     }
     ```

   - **パフォーマンス最適化**:
     - 早期リターン: サイズ変化なし or 変化量2px未満
     - 循環更新防止: `currentlyUpdatingハッシュセット`
     - メモリ管理: リサイズ終了時に`originalSizes.Clear()`

   - **最小/最大サイズ制限**: settings.MinimumSize (100x100), MaximumSize (800x600)

3. **MovableWindowStrategy** (WindowStrategies.cs:501-600)
   - **基本動作**: マウスドラッグで移動可能

   - **処理フロー**:
     - `OnMouseDown`: ドラッグ開始、lastMousePosとlastValidPositionを記録
     - `OnMouseUp`: ドラッグ終了
     - `Update`: マウス位置からCalculateMovement → GetValidPosition → 位置適用

   - **CalculateMovement** (WindowStrategies.cs:548-570):
     ```csharp
     Vector2 CalculateMovement(GameWindow window, Point mousePos)
     {
         int deltaX = (int)((mousePos.X - lastMousePos.X) * settings.DragSpeed);
         int deltaY = (int)((mousePos.Y - lastMousePos.Y) * settings.DragSpeed);

         Point proposedLocation = new(window.Location.X + deltaX, window.Location.Y + deltaY);
         Rectangle proposedBounds = new(proposedLocation, window.Size);

         return new Vector2(deltaX, deltaY);
     }
     ```

   - **衝突回避**: `NoEntryZoneManager.GetValidPosition(currentBounds, proposedBounds, window)`
     - 不可侵領域・不可侵境界・通常ウィンドウとの衝突を検出
     - Z-order + Region考慮（隠れているウィンドウは無視）
     - X軸とY軸を独立して調整

   - **方向別ブロック管理** (WindowStrategies.cs:508-511):
     ```csharp
     private bool isBlockedRight = false;
     private bool isBlockedLeft = false;
     private bool isBlockedDown = false;
     private bool isBlockedUp = false;
     ```
     - 衝突方向を記録して移動を制限
     - ブロック解除条件: 反対方向への移動

   - **DragSpeed適用**: settings.DragSpeed (デフォルト1.0)
     - マウス移動量に乗算して速度調整
     - 0.5で遅く、2.0で速くなる

4. **DeletableWindowStrategy**
   - Deleteキーで削除可能
   - 削除時に子要素も解放

5. **MinimizableWindowStrategy**
   - ウィンドウの最小化/復元が可能
   - 最小化時はNoEntryZoneを削除
   - 復元時はNoEntryZoneを再作成

6. **TextDisplayWindowStrategy**
   - テキストを中央表示
   - 静的な情報表示用

7. **NoEntryWindowStrategy**
   - 静的な不可侵ウィンドウ
   - **境界生成**: ウィンドウの外周5px、4辺（上下左右）の矩形
   - プレイヤーや他のウィンドウが境界にぶつかる
   - 最小化時は不可侵領域を削除、復元時に再作成
   - **視覚表現**: 赤いアウトライン（2px幅）、背景色Color.DimGray
   - **カーソル**: Cursors.No
   - **IsNoEntryプロパティ**: `override bool IsNoEntry => true`

**NoEntry系戦略の共通仕組み**:

#### 不可侵境界の生成（NoEntryZoneManager.GetNoEntryBoundaryRectangles）
```csharp
// 外周5pxの4辺を矩形として生成
const int BOUNDARY_WIDTH = 5;
Rectangle bounds = window.CollisionBounds;

List<Rectangle> boundaries = new()
{
    // 上辺: (X, Y, Width, 5)
    new Rectangle(bounds.X, bounds.Y, bounds.Width, BOUNDARY_WIDTH),
    // 下辺: (X, Bottom-5, Width, 5)
    new Rectangle(bounds.X, bounds.Bottom - BOUNDARY_WIDTH, bounds.Width, BOUNDARY_WIDTH),
    // 左辺: (X, Y, 5, Height)
    new Rectangle(bounds.X, bounds.Y, BOUNDARY_WIDTH, bounds.Height),
    // 右辺: (Right-5, Y, 5, Height)
    new Rectangle(bounds.Right - BOUNDARY_WIDTH, bounds.Y, BOUNDARY_WIDTH, bounds.Height)
};
```

#### Z-order + Region考慮の境界判定
- CheckNoEntryBoundaryCollision: 境界矩形ごとにZ-order判定
- より前面の不可侵ウィンドウに隠れている部分は判定なし
- Region.Excludeで隠れ部分を除外、IsEmptyで可視性確認

8. **ResizableNoEntryWindowStrategy** 🆕
   - **継承**: ResizableWindowStrategyの全機能 + 不可侵境界
   - **マウスドラッグでリサイズ可能**: 上記ResizableWindowStrategyと同じ処理
   - **境界の動的更新**: リサイズ時に境界矩形を再計算
   - **視覚表現**: 赤いアウトライン + リサイズマーク（白/灰色切替）
   - **背景色**: Color.LightGreen
   - **親子制約**: 親が不可侵の場合、子が境界を超えるリサイズを制限

9. **MovableNoEntryWindowStrategy** 🆕
   - **継承**: MovableWindowStrategyの全機能 + 不可侵境界
   - **マウスドラッグで移動可能**: 上記MovableWindowStrategyと同じ処理
   - **境界の追従**: 移動時に境界矩形も自動的に更新
   - **視覚表現**: 赤いアウトライン + 移動マーク（4方向矢印）
   - **背景色**: Color.LightBlue
   - **親子制約**: 親が不可侵の場合、子が境界を超える移動を制限

10. **MinimizableNoEntryWindowStrategy** 🆕
    - **継承**: MinimizableWindowStrategyの全機能 + 不可侵境界
    - **最小化/復元**: ウィンドウの状態切り替え
    - **境界のライフサイクル管理**:
      - 最小化時: `RemoveNoEntryZonesForWindow(window)` 呼び出し
      - 復元時: `UpdateNoEntryZonesForWindow(window)` 呼び出し
    - **視覚表現**: 赤いアウトライン + 最小化マーク（下向き矢印）
    - **背景色**: Color.LightPink

### 4. プレイヤーシステム (`Player/`)

プレイヤーシステムはコンポーネントベースアーキテクチャを採用。

#### PlayerForm
プレイヤーの視覚的表現とコンポーネント統合。

**責務**:
- 各コンポーネントの統合と調整
- 描画とアニメーション
- 境界の管理（Display, Collision, Render）

**コンポーネント構成**:
```csharp
private readonly IPlayerPhysics physics;
private readonly IPlayerInputHandler inputHandler;
private readonly IPlayerWindowInteraction windowInteraction;
private readonly IPlayerStateMachine stateMachine;
```

**更新フロー**:
```csharp
async Task UpdateAsync(float deltaTime)
{
    inputHandler.HandleInput(physics, OnJumpTriggered);
    physics.ApplyGravity(deltaTime);
    var movement = physics.CalculateMovement(deltaTime);
    var adjustedMovement = windowInteraction.AdjustMovement(movement, bounds);
    // 位置更新、衝突判定、状態遷移...
}
```

#### PlayerPhysics (IPlayerPhysics)
物理演算エンジン。

**責務**:
- 重力の適用
- 垂直速度の管理
- 移動量の計算
- 接地判定（Sweep Bounds方式）
- ジャンプ処理

**主要メソッド**:
```csharp
void ApplyGravity(float deltaTime)
void Jump()
Vector2 CalculateMovement(float deltaTime)
void CheckGrounded(Rectangle bounds, GameWindow? parentWindow, float deltaTime)
void SetGrounded(bool grounded, int? groundY = null)
```

**詳細な物理演算アルゴリズム**:

#### 重力システム (PlayerPhysics.cs:62-72)
```csharp
public void ApplyGravity(float deltaTime)
{
    if (!isGrounded)
    {
        verticalVelocity += settings.Gravity * deltaTime;  // 980 * deltaTime
    }
    else
    {
        verticalVelocity = 0;  // 接地時は速度リセット
    }
}
```
- **Gravity値**: 980（ピクセル/秒²）
- **落下速度**: 無制限に増加（Sweep Boundsで対応）
- **接地時**: 速度を0にリセット

#### ジャンプメカニズム (PlayerPhysics.cs:74-78)
```csharp
public void Jump()
{
    verticalVelocity = -settings.JumpForce;  // -450
    isGrounded = false;  // 空中状態に遷移
}
```
- **JumpForce値**: 450（ピクセル/秒）
- **連続ジャンプ防止**: CheckGroundedで`verticalVelocity < 0`時は判定スキップ（PlayerPhysics.cs:86-91）

#### Sweep Bounds方式の接地判定 (PlayerPhysics.cs:83-232)
```
判定範囲の計算:
1. currentFeetBounds: プレイヤーの足元判定エリア
   Rectangle(bounds.X, bounds.Bottom - 10, bounds.Width, GroundCheckHeight(5))

2. maxVerticalStep: 1フレームでの最大移動距離
   max(|verticalVelocity * deltaTime|, 20f)
   高速落下時もすり抜けを防ぐため、最低20pxを確保

3. sweepBounds: 現在位置と移動後位置を含む矩形
   Rectangle(
       currentFeetBounds.X,
       min(currentFeetBounds.Y, currentFeetBounds.Y + verticalVelocity * deltaTime) - 5,
       currentFeetBounds.Width,
       maxVerticalStep + currentFeetBounds.Height + 10
   )
```

**判定優先順位**（上から順にチェック、最初にヒットで確定）:

1. **静的NoEntryZone** (PlayerPhysics.cs:112-123)
   ```csharp
   if (bounds.Bottom >= zone.Bounds.Top &&
       bounds.Bottom <= zone.Bounds.Top + 5 &&  // 5px許容範囲
       bounds.Right > zone.Bounds.Left &&
       bounds.Left < zone.Bounds.Right)
   {
       SetGrounded(true, zone.Bounds.Top);
       OnGrounded?.Invoke(zone.Bounds.Top);  // コールバック実行
       return;
   }
   ```

2. **不可侵ウィンドウ境界（Z-order + Region考慮）** (PlayerPhysics.cs:125-145)
   ```csharp
   if (CheckAnyNoEntryBoundaryCollision(sweepBounds, out collidingWindow, out collisionRect))
   {
       // 下辺（地面）判定のみ: プレイヤーの足元が境界の上辺付近
       if (bounds.Bottom >= rect.Top &&
           bounds.Bottom <= rect.Top + 5 &&  // 5px許容範囲
           bounds.Right > rect.Left &&
           bounds.Left < rect.Right)
       {
           SetGrounded(true, rect.Top);
           OnGrounded?.Invoke(rect.Top);
           return;
       }
   }
   ```

3. **UIボタン** (PlayerPhysics.cs:147-171)
   - ボタンの上辺のみ判定
   - 5px許容範囲

4. **デスクトップアイコン** (PlayerPhysics.cs:173-196)
   ```csharp
   var nearbyIcons = desktopIcons
       .Where(icon => icon.Bounds.IntersectsWith(currentFeetBounds))
       .Take(5)  // パフォーマンス最適化: 最大5個
       .ToList();
   ```
   - Z-order考慮（アイコンがウィンドウに隠されていないか）
   - 上辺のみ判定

5. **ゲームウィンドウ（Z-order降順）** (PlayerPhysics.cs:198-228)
   ```csharp
   var intersectingWindows = windowManager
       .GetIntersectingWindows(sweepBounds)
       .OrderByDescending(w => windowManager.GetWindowZIndex(w));  // 前面優先

   foreach (var window in intersectingWindows)
   {
       // 不可侵ウィンドウはスキップ（既にステップ2で処理済み）
       if (window.IsNoEntryWindow) continue;

       // 上辺判定
       if (bounds.Bottom >= windowBounds.Top &&
           bounds.Bottom <= windowBounds.Top + 5 &&
           bounds.Right > windowBounds.Left &&
           bounds.Left < windowBounds.Right)
       {
           SetGrounded(true, windowBounds.Top);
           OnGrounded?.Invoke(windowBounds.Top);
           return;
       }
   }
   ```

6. **メインフォームの底** (PlayerPhysics.cs:230-232)
   - 最終的なフォールバック
   - `SetGrounded(true, MainGame.Instance.Bottom)`

#### OnGroundedコールバック (PlayerPhysics.cs:81)
```csharp
public Action<int>? OnGrounded { get; set; }
```
- **役割**: SetGrounded呼び出し時に地面のY座標を通知
- **使用例**: PlayerFormが受け取り、AdjustToGround(groundY)で位置を微調整

**重要な実装詳細**:
```csharp
// TODO: 入力処理はPlayerInputHandlerに移動すべき（責務分離） - 優先度:保留
// 判断理由（2025-01-11）:
// - アーキテクチャ安定化優先（最近の大規模リファクタリング直後）
// - AI制御・リプレイ機能の予定なし（YAGNI原則）
```

#### PlayerInputHandler (IPlayerInputHandler)
入力処理を担当。

**責務**:
- キーボード入力の検出（WASD, Arrow keys, Space）
- ジャンプトリガー
- プレイヤーの向きの管理

**主要メソッド**:
```csharp
bool ShouldJump()
bool IsMovingLeft()
bool IsMovingRight()
void HandleInput(IPlayerPhysics physics, Action onJumpTriggered)
string UpdateFacing(string currentFacing)
```

#### PlayerWindowInteraction (IPlayerWindowInteraction)
ウィンドウ間の遷移と相互作用。

**責務**:
- 親ウィンドウの管理と遷移
- 移動可能領域の計算
- ウィンドウとの衝突判定
- 無効な位置の調整
- 最小化/復元の処理

**主要メソッド**:
```csharp
GameWindow? GetParentWindow()
void SetParentWindow(GameWindow? window)
Rectangle GetMovableRegion()
Vector2 AdjustMovement(Vector2 movement, Rectangle bounds)
void HandleParentMinimized()
void HandleParentRestored()
```

**移動可能領域の計算**:
- 親ウィンドウの内部境界
- タイトルバーを除外
- クライアント領域のみ

#### PlayerStateMachine (IPlayerStateMachine)
状態管理を担当。

**状態一覧**:
```csharp
Normal    // 通常状態
Jumping   // ジャンプ中
Falling   // 落下中
Grounded  // 接地中
```

**状態遷移**:
```csharp
void UpdateState(bool isGrounded, float verticalVelocity)
{
    if (verticalVelocity < 0) State = Jumping;
    else if (!isGrounded) State = Falling;
    else State = Grounded;
}
```

#### PlayerFormFactory (IPlayerFormFactory)
プレイヤーの生成と依存性注入。

**生成フロー**:
```csharp
PlayerForm CreatePlayer(Point startPosition)
{
    // 1. DIコンテナから各コンポーネントを解決
    var physics = container.Resolve<IPlayerPhysics>();
    var inputHandler = container.Resolve<IPlayerInputHandler>();
    var windowInteraction = new PlayerWindowInteraction(...);
    var stateMachine = container.Resolve<IPlayerStateMachine>();

    // 2. PlayerFormを生成
    var player = new PlayerForm(startPosition, physics, inputHandler,
                                 windowInteraction, stateMachine, ...);

    // 3. コールバックの設定
    physics.OnGrounded = (groundY) => player.AdjustToGround(groundY);

    return player;
}
```

### 5. ゲームシステム (`Systems/`)

#### InputSystem
グローバル入力処理システム。

**優先度**: 100（最初に実行）

**責務**:
- グローバルキー入力の処理
- デバッグモード切り替え（F3キー）
- 設定画面呼び出し（F1キー）
- 緊急終了（Shift+Escape）

**実装例**:
```csharp
protected override async Task OnUpdateAsync(float deltaTime)
{
    // デバッグモード切り替え
    if (inputService.IsKeyDown(Keys.F3))
    {
        mainGame.ToggleDebugMode();
        logger.LogDebug($"Debug mode toggled (now: {mainGame.IsDebugMode})", SystemName);
        await Task.Delay(200);  // 連続トグル防止
    }

    // 緊急終了
    if (inputService.IsKeyDown(Keys.Escape) && inputService.IsKeyDown(Keys.LShiftKey))
    {
        Application.Exit();
    }
}
```

#### RenderingSystem
レンダリングパイプライン。

**優先度**: 400（最後に実行）

**責務**:
- グラフィックスバッファの管理
- ゲーム要素の描画（ゴール、ウィンドウ、不可侵領域）
- UI要素の描画（デバッグ情報、通知、パフォーマンス統計）
- FPSカウンター

**描画順序**:
1. バッファクリア
2. ゴールの描画
3. 不可侵領域の描画（デバッグモード時）
4. ウィンドウの描画
5. デバッグ情報の描画
6. 通知の描画
7. パフォーマンス統計の描画
8. バッファの表示

### 6. サービス層 (`Services/`)

#### GameTimeService (IGameTimeService)
時間管理とデルタタイム計算。

**機能**:
- フレーム独立な時間追跡
- 一時停止/再開サポート
- デルタタイムの計算

**プロパティ**:
```csharp
float DeltaTime { get; }      // 前フレームからの経過時間
float TotalTime { get; }      // 総経過時間
bool IsPaused { get; }        // 一時停止状態
```

**置き換え前**: `GameTime`静的クラス

#### InputService (IInputService)
キーボード入力検出。

**機能**:
- グローバルキー状態の追跡
- Windows Formsとの統合

**主要メソッド**:
```csharp
bool IsKeyDown(Keys key)
void UpdateKeyState(Keys key, bool isDown)
```

**置き換え前**: `Input`静的クラス

#### NotificationService (INotificationService)
UI通知システム。

**機能**:
- 設定変更通知
- フェードアウトアニメーション
- 通知の自動消去

**主要メソッド**:
```csharp
void ShowNotification(string message)
void Update(float deltaTime)
void Draw(Graphics g, Point position)
```

**置き換え前**: `SettingsNotification`静的クラス

#### CollisionService (ICollisionService)
統一的な衝突判定API。

**機能**:
- 位置・サイズの検証
- 静的NoEntryZoneとの衝突判定
- 不可侵ウィンドウ境界との衝突判定
- 通常ウィンドウとの衝突判定
- Z-order考慮の判定

**主要メソッド**:
```csharp
Rectangle ValidatePosition(Rectangle currentBounds, Rectangle proposedBounds, CollisionOptions options)
Size ValidateSize(Rectangle currentBounds, Size proposedSize, CollisionOptions options)
```

**CollisionOptions**:
```csharp
public class CollisionOptions
{
    public bool CheckNoEntryZones { get; set; }         // 静的NoEntryZoneチェック
    public bool CheckNoEntryBoundaries { get; set; }    // 不可侵ウィンドウ境界チェック
    public bool CheckNormalWindows { get; set; }        // 通常ウィンドウチェック
    public GameWindow? ExcludeWindow { get; set; }      // 除外ウィンドウ
}
```

**CollisionFlags（ビルダーパターン）**:
```csharp
var options = new CollisionFlags()
    .WithNoEntryZones()
    .WithNoEntryBoundaries()
    .ExcludingWindow(window)
    .Build();
```

**内部構造**:
- `CollisionValidator`: 位置・サイズ検証の実装（260行）
- `NoEntryBoundaryCollider`: 境界判定
- `ZOrderCollisionHelper`: Z-order優先度管理
- `ZOrderVisibilityService`: 可視性判定

#### CollisionValidator
位置・サイズの検証ロジックを提供。

**責務**:
- X/Y軸独立な移動検証
- 最近接境界の計算
- 複数衝突時の優先順位処理

**主要メソッド**:
```csharp
Rectangle ValidatePosition(Rectangle currentBounds, Rectangle proposedBounds, CollisionOptions options)
Size ValidateSize(Rectangle currentBounds, Size proposedSize, CollisionOptions options)
```

**検証フロー**:
1. 静的NoEntryZoneとの衝突チェック
2. 不可侵ウィンドウ境界との衝突チェック
3. 通常ウィンドウとの衝突チェック
4. 最も近い有効な位置/サイズを計算

### 7. マネージャー層 (`Managers/`)

#### WindowHierarchyManager (IWindowHierarchyManager)
ウィンドウの親子関係を管理。

**機能**:
- 親子関係の構築と維持
- 子ウィンドウの自動移動/スケーリング
- 階層の検証

#### WindowZOrderManager (IWindowZOrderManager)
ウィンドウの重なり順を管理。

**機能**:
- Z-orderの追跡
- 前面/背面への移動
- 重なり順に基づいた判定
- 視覚的なZ-order（Win32ネイティブ）と内部リストの同期

**重要な動作**:
- ウィンドウをクリックすると、そのウィンドウとその子孫が最前面に移動
- 親の有無に関わらず、クリックされたウィンドウを最前面に配置
- 内部リストとネイティブZ-orderを常に一致させることで、衝突判定の正確性を保証

#### WindowCollisionDetector (IWindowCollisionDetector)
ウィンドウ間の衝突判定。

**機能**:
- AABB（Axis-Aligned Bounding Box）衝突判定
- 範囲内のウィンドウ検出
- 最近接ウィンドウの検索

#### NoEntryZoneManager (INoEntryZoneManager)
不可侵領域の管理。

**リファクタリング（2025年1月）**:
- **962行 → 250行**（74%削減）
- 境界判定、Z-order処理、検証ロジックをCollisionServiceに分離
- 静的NoEntryZone管理のみに責務を限定

**機能**:
- 静的NoEntryZoneの登録と管理
- 不可侵ウィンドウの登録と管理
- 基本的な衝突判定（詳細な検証はCollisionServiceに委譲）

**Z-order考慮の境界判定（2025-01-27追加）**:
- 不可侵ウィンドウの境界判定でRegion方式を採用
- 他のウィンドウに隠れている部分は衝突判定なし
- GDI+ Regionを使用した正確な可視性判定
- NoEntryZoneオブジェクト生成を廃止（パフォーマンス改善）

**通常ウィンドウの可視性チェック（2025-01-27追加）**:
- 不可侵ウィンドウに隠れている通常ウィンドウは衝突判定をスキップ
- IsWindowVisibleFromNoEntryメソッドで可視性を判定
- IntersectsWithAnyZone、GetValidPositionで適用
- 見えている部分のみが衝突判定に影響

**リサイズ時のX/Y軸独立性（2025-01-27修正）**:
- GetValidSizeのX軸とY軸の衝突判定が完全に独立
- Y軸チェック時にX軸で調整された幅を使用
- 横方向の衝突が縦方向のサイズに影響しない

**主要メソッド**:
```csharp
bool IntersectsWithAnyZone(Rectangle bounds, GameWindow? excludeWindow = null)
Rectangle GetValidPosition(Rectangle current, Rectangle proposed, GameWindow? excludeWindow = null)
Size GetValidSize(Rectangle current, Size proposed, GameWindow? excludeWindow = null)
void UpdateNoEntryZonesForWindow(GameWindow window)
void RemoveNoEntryZonesForWindow(GameWindow window)

// Z-order対応の新規メソッド
List<Rectangle> GetNoEntryBoundaryRectangles(GameWindow window)
bool CheckNoEntryBoundaryCollision(GameWindow window, Rectangle checkBounds, out Rectangle? collisionRect)
bool CheckAnyNoEntryBoundaryCollision(Rectangle checkBounds, out GameWindow? collidingWindow, out Rectangle? collisionRect)
bool IsWindowVisibleFromNoEntry(GameWindow normalWindow, Rectangle checkBounds, GameWindow? excludeWindow)
```

#### IntersectsWithAnyZoneの判定フロー (NoEntryZoneManager.cs:57-110)
```
3段階チェック:
1. 静的NoEntryZoneとの交差チェック
   - zones.Any(zone => zone.Bounds.IntersectsWith(bounds))
   - 固定された不可侵領域

2. 不可侵ウィンドウ境界判定（Z-order + Region考慮）
   - CheckAnyNoEntryBoundaryCollision(bounds, out _, out _, excludeWindow)
   - 隠れている境界部分は判定なし

3. excludeWindowが不可侵の場合、通常ウィンドウとも衝突判定
   - IsWindowVisibleFromNoEntry: 可視性チェック
   - 隠れている通常ウィンドウはスキップ
   - bounds.IntersectsWith(window.CollisionBounds)

戻り値: いずれか1つでも衝突すればtrue
```

#### GetValidPositionの調整アルゴリズム (NoEntryZoneManager.cs:112-407)
```
X軸とY軸を独立して処理（最も制約が厳しい調整を採用）:

[静的NoEntryZone]
1. X軸移動チェック:
   xMovement = (proposedX, currentY, proposedW, currentH)
   if xMovement.IntersectsWith(zone.Bounds):
       candidateX = 移動方向に応じて計算
       bestAdjustedX = min(|candidateX - currentX|)

2. Y軸移動チェック:
   yMovement = (bestAdjustedX ?? proposedX, proposedY, proposedW, proposedH)
   if yMovement.IntersectsWith(zone.Bounds):
       candidateY = 移動方向に応じて計算
       bestAdjustedY = min(|candidateY - currentY|)

[不可侵ウィンドウ境界（Z-order + Region考慮）]
3. X軸境界チェック:
   if CheckNoEntryBoundaryCollision(window, xMovement, out xRect):
       candidateX = xRect位置から計算（左から右 or 右から左）
       bestAdjustedX = min(移動距離)

4. Y軸境界チェック:
   yMovement = (bestAdjustedX ?? proposedX, proposedY, proposedW, proposedH)
   if CheckNoEntryBoundaryCollision(window, yMovement, out yRect):
       candidateY = yRect位置から計算
       bestAdjustedY = min(移動距離)

[通常ウィンドウ（excludeWindowが不可侵の場合）]
5. X軸通常ウィンドウチェック:
   if IsWindowVisibleFromNoEntry(window, xMovement, excludeWindow):
       if xMovement.IntersectsWith(windowBounds):
           candidateX = エッジ位置から計算
           bestAdjustedX = min(移動距離)

6. Y軸通常ウィンドウチェック:
   （同様のロジック）

最終調整: (bestAdjustedX ?? proposedX, bestAdjustedY ?? proposedY)
```

#### GetValidSizeの制約適用ロジック (NoEntryZoneManager.cs:408-660)
```
拡大方向判定:
- isGrowingWidth = proposedWidth > currentWidth
- isGrowingHeight = proposedHeight > currentHeight

X/Y軸を独立して処理（最も制約が厳しい = 最小のサイズを採用）:

[静的NoEntryZone]
1. X方向拡大チェック（isGrowingWidthの場合のみ）:
   xResize = (currentX, currentY, proposedW, currentH)
   if xResize.IntersectsWith(zone.Bounds):
       candidateWidth = zone.Bounds.X - currentX
       minWidth = min(candidateWidth)

2. Y方向拡大チェック（isGrowingHeightの場合のみ）:
   yResize = (currentX, currentY, minWidth ?? proposedW, proposedH)
   重要: X軸で調整された幅を使用
   if yResize.IntersectsWith(zone.Bounds):
       candidateHeight = zone.Bounds.Y - currentY
       minHeight = min(candidateHeight)

[不可侵ウィンドウ境界]
3. X方向境界チェック:
   if CheckNoEntryBoundaryCollision(window, xResize, out xRect):
       candidateWidth = xRect.Value.X - currentX
       minWidth = min(candidateWidth)

4. Y方向境界チェック:
   yResize = (currentX, currentY, minWidth ?? proposedW, proposedH)
   （同様のロジック）

[通常ウィンドウ（excludeWindowが不可侵の場合）]
5-6. 通常ウィンドウとの衝突チェック（可視性判定付き）

settings制約適用:
- minWidth >= settings.MinimumSize.Width
- maxWidth <= settings.MaximumSize.Width
- 高さも同様

最終サイズ: (minWidth ?? proposedW, minHeight ?? proposedH)
```

#### IsWindowVisibleFromNoEntryの可視性判定 (NoEntryZoneManager.cs:886-956)
```
Region方式アルゴリズム:

1. normalWindowとcheckBoundsの交差部分を計算
   intersection = Rectangle.Intersect(checkBounds, windowBounds)
   if !checkBounds.IntersectsWith(windowBounds): return false

2. intersection矩形のRegion作成
   using (Region visibleRegion = new Region(intersection))

3. Z-orderループ（normalWindowIndexより前面の不可侵ウィンドウのみ）:
   for (i = normalWindowIndex + 1; i < windowsList.Count; i++)
   {
       var coveringWindow = windowsList[i];
       // 条件: 不可侵ウィンドウ、excludeWindowではない、最小化されていない
       if (coveringWindow.IsNoEntryWindow && !最小化)
       {
           visibleRegion.Exclude(coveringWindow.CollisionBounds);
       }
   }

4. 可視性判定:
   using (var dummyGraphics = Graphics.FromHwnd(IntPtr.Zero))
   {
       return !visibleRegion.IsEmpty(dummyGraphics);
   }

結果: visibleRegionが空でなければ見えている = 衝突判定有効
```

#### DesktopIconManager
デスクトップアイコンとの統合。

**機能**:
- デスクトップアイコンの検出
- 衝突境界の作成
- アイコンの可視性判定（Z-order考慮）
- パフォーマンス最適化（キャッシング）

---

## ゲーム設定とデータ

### GameSettings (IGameSettings)
ゲームの全設定を管理。

**設定カテゴリ**:

#### PlayerSettings
```csharp
float MovementSpeed { get; }        // 移動速度: 300
float Gravity { get; }              // 重力: 980
float JumpForce { get; }            // ジャンプ力: 450
int GroundCheckHeight { get; }      // 接地判定の高さ: 5
```

#### WindowSettings
```csharp
Size MinimumSize { get; }           // 最小サイズ: 100x100
Size MaximumSize { get; }           // 最大サイズ: 800x600
float DragSpeed { get; }            // ドラッグ速度: 1.0
```

#### GameplaySettings
```csharp
int TargetFPS { get; }              // 目標FPS: 60
bool EnableDebugMode { get; }       // デバッグモード有効: false
```

**設定ファイル**: `config/settings.json`

### ステージ定義
各ステージは以下の要素で構成：

```csharp
public class StageData
{
    public int StageNumber { get; set; }
    public Point PlayerStart { get; set; }
    public Point GoalPosition { get; set; }
    public List<WindowData> Windows { get; set; }
    public List<NoEntryZoneData> NoEntryZones { get; set; }
    public List<ButtonData> Buttons { get; set; }
}

public class WindowData
{
    public WindowType Type { get; set; }
    public Point Position { get; set; }
    public Size Size { get; set; }
    public string? Text { get; set; }  // TextDisplay用
}
```

---

## 重要な実装詳細

### ウィンドウの境界システム

GameWindowは3種類の境界プロパティを持ち、用途によって使い分けられます。

**境界の種類**:

1. **CollisionBounds** (GameWindow.cs:40-45)
   - **用途**: 衝突判定用の境界
   - **特徴**: タイトルバーを含む完全な矩形
   - **計算式**:
   ```csharp
   public Rectangle CollisionBounds => new(
       AdjustedBounds.X,
       Location.Y,  // ウィンドウの実際のY座標
       AdjustedBounds.Size.Width,
       AdjustedBounds.Size.Height + (RectangleToScreen(ClientRectangle).Y - Location.Y)
   );
   ```
   - **使用例**: プレイヤーの接地判定、ウィンドウ間衝突判定

2. **AdjustedBounds** (GameWindow.cs:21)
   - **用途**: スケール適用後の境界
   - **特徴**: 親ウィンドウのリサイズによるスケーリングが反映
   - **使用例**: 親子関係の包含判定、描画位置

3. **FullBounds / DisplayBounds** (GameWindow.cs:28)
   - **用途**: 表示用の境界
   - **計算式**: `new Rectangle(Location, Size)`
   - **使用例**: ウィンドウの基本的な位置とサイズ

**使い分けの原則**:
```csharp
// 親子関係の包含判定
if (parent.AdjustedBounds.Contains(child.AdjustedBounds))
{
    // 子が親に完全に含まれている
}

// プレイヤーの衝突判定
if (player.Bounds.IntersectsWith(window.CollisionBounds))
{
    // タイトルバーを含む完全な境界で判定
}

// ウィンドウの移動
window.Location = new Point(x, y);  // FullBoundsが更新される
```

**タイトルバーの高さ計算**:
```csharp
int titleBarHeight = RectangleToScreen(ClientRectangle).Y - Location.Y;
// 通常約30-40px（Windows標準）
```

### ウィンドウの親子関係

**親ウィンドウの影響**:
1. **移動**: 親が移動すると子も追従
2. **スケーリング**: 親がリサイズされると子も比例してスケーリング
3. **最小化**: 親が最小化されると子も非表示
4. **Z-order**: 親子関係はZ-orderに影響

#### 親子関係の確立（WindowHierarchyManager.CheckPotentialParentWindow）

**アルゴリズム** (WindowHierarchyManager.cs:39-122):
```
1. 現在の親子関係の検証:
   if !IsWindowContainedWithinBounds(parent.AdjustedBounds, child.AdjustedBounds):
       parent.RemoveChild(child)
       RemoveParentChildRelation(child)

2. 子ウィンドウとの関係チェック:
   foreach child in operatedWindow.Children:
       if !IsWindowContainedWithinBounds(operatedWindow.AdjustedBounds, child.AdjustedBounds):
           operatedWindow.RemoveChild(child)
           RemoveParentChildRelation(child)

3. 既存の親子グループ取得:
   existingGroup = { operatedWindow } + GetAllDescendants(operatedWindow)

4. 親候補検索（Z-order降順）:
   candidates = allWindows
       .Where(w => !existingGroup.Contains(w) && w != operatedWindow &&
                   IsWindowInFront(operatedWindow, w))  // operatedWindowが前面
       .OrderByDescending(w => GetWindowZIndex(w))

5. 最適な親の選択:
   foreach candidate in candidates:
       if IsWindowContainedWithinBounds(candidate.AdjustedBounds, operatedWindow.AdjustedBounds):
           if bestParent == null || IsWindowInFront(candidate, currentParent):
               bestParent = candidate

6. 親設定:
   if bestParent != null && bestParent != currentParent:
       bestParent.AddChild(operatedWindow)
       AddParentChildRelation(operatedWindow, bestParent)
```

#### 親ウィンドウの移動時の処理

**OnParentMove** (GameWindow内部):
```csharp
public void OnParentMove(Point delta)
{
    // 1. 親の移動量を子に適用
    this.Location = new Point(this.Location.X + delta.X, this.Location.Y + delta.Y);

    // 2. 再帰的に全子孫に伝播
    foreach (var child in Children.OfType<GameWindow>())
    {
        child.OnParentMove(delta);
    }
}
```

#### 親ウィンドウのリサイズ時の処理

**OnParentResize** (GameWindow内部 + ResizableWindowStrategy.cs:448-463):
```csharp
public void OnParentResize(SizeF scale)
{
    // 1. 元サイズから新サイズを計算
    var newSize = new Size(
        (int)(originalSize.Width * scale.Width),
        (int)(originalSize.Height * scale.Height)
    );
    this.Size = newSize;

    // 2. 位置もスケーリング（親内の相対位置を維持）
    var newLocation = new Point(
        (int)(originalLocation.X * scale.Width),
        (int)(originalLocation.Y * scale.Height)
    );
    this.Location = newLocation;

    // 3. 再帰的に全子孫に伝播（累積スケール）
    foreach (var child in Children.OfType<GameWindow>())
    {
        child.OnParentResize(scale);
    }
}

// 循環更新防止（ResizableWindowStrategy）:
private readonly HashSet<IEffectTarget> currentlyUpdating = new HashSet<IEffectTarget>();

if (!currentlyUpdating.Add(child))
{
    continue;  // 既に更新中の場合はスキップ
}
// 更新処理...
currentlyUpdating.Remove(child);
```

#### 座標系変換

**グローバル座標 ↔ ローカル座標**:
```csharp
// グローバル座標: デスクトップ座標系（画面全体）
// ローカル座標: 親ウィンドウ内の相対座標

// ローカル → グローバル:
public Point GetGlobalPosition()
{
    Point global = this.Location;
    GameWindow? current = this.Parent;
    while (current != null)
    {
        global.X += current.Location.X;
        global.Y += current.Location.Y;
        current = current.Parent;
    }
    return global;
}

// グローバル → ローカル:
public Point ConvertToLocal(Point globalPos, GameWindow parent)
{
    Point parentGlobal = parent.GetGlobalPosition();
    return new Point(
        globalPos.X - parentGlobal.X,
        globalPos.Y - parentGlobal.Y
    );
}

// ウィンドウ遷移時の変換:
Point globalPos = GetGlobalPosition();
SetParent(newParent);
Point localPos = ConvertToLocal(globalPos, newParent);
SetPosition(localPos);
```

**IsWindowContainedWithinBounds** (WindowHierarchyManager.cs:138-143):
```csharp
// 完全包含判定
private static bool IsWindowContainedWithinBounds(Rectangle container, Rectangle contained)
{
    return contained.Left >= container.Left &&
           contained.Top >= container.Top &&
           contained.Right <= container.Right &&
           contained.Bottom <= container.Bottom;
}
```

### プレイヤーのウィンドウ遷移

#### HandleWindowTransitionsのロジック (PlayerWindowInteraction.cs:42-60)
```
遷移判定の2パターン:

1. currentParent == null（デスクトップ上）:
   newWindow = GetWindowFullyContaining(newBounds)
   // 完全包含判定: プレイヤー全体がウィンドウ内
   if newWindow != null:
       SetParent(newWindow)

2. currentParent != null（ウィンドウ上）:
   newWindow = GetTopWindowAt(newBounds, currentParent)
   // 5点チェック + Z-order判定
   if newWindow != null && newWindow != currentParent:
       SetParent(newWindow)
```

#### GetTopWindowAtの5点チェック (WindowCollisionDetector.cs:48-142)
```
チェックポイント（優先度順）:
1. bounds.Left, Bottom（左下）  ← 最優先（足元）
2. bounds.Right, Bottom（右下） ← 最優先（足元）
3. bounds.Left, Top（左上）
4. bounds.Right, Top（右上）
5. bounds.Center（中心）

アルゴリズム:
1. 各点でZ-order降順にウィンドウ検索:
   for point in checkPoints:
       for window in windowsList.Reverse():  // 前面から検索
           if IsPointWithinBounds(window.AdjustedBounds, point):
               windowPoints[window].Add(point)
               break  // 最初にヒットしたウィンドウで確定

2. currentParent上のポイント数チェック:
   if currentWindowPoints.Any():
       // 足元の2点を含む候補を探す
       candidates = windowPoints
           .Where(w => w != currentWindow &&
                      (w.Value.Contains(checkPoints[0]) || w.Value.Contains(checkPoints[1])))
           .OrderBy(w => w.Key.AdjustedBounds.Bottom)  // 底辺が低い順
           .ThenByDescending(w => GetWindowZIndex(w.Key))  // Z-order優先

       if candidates.Any():
           bestWindow = candidates.First()
           if bestWindow.BottomEdge <= currentWindow.BottomEdge:
               return bestWindow.Window

       return currentWindow  // 現在のウィンドウを維持
   else:
       // 足元の2点を含む候補から選択
       （同様のロジック）
```

**遷移時の処理 (PlayerWindowInteraction.SetParent)**:
```csharp
public void SetParent(GameWindow? newParent)
{
    // 1. 現在の親から切り離し
    if (currentParent != null)
    {
        lastValidParent = currentParent;
        if (playerForm != null)
            currentParent.RemoveChild(playerForm);
    }

    // 2. 新しい親に登録
    currentParent = newParent;
    if (currentParent != null && playerForm != null)
        currentParent.AddChild(playerForm);

    // 3. PlayerFormのParentプロパティ更新
    onParentChanged?.Invoke(newParent);
}
```

**座標系変換の実際の流れ**:
```csharp
// PlayerForm.UpdateAsync内での遷移処理:
1. 移動前の親: windowA
2. プレイヤーの新しい位置を計算
3. HandleWindowTransitions(newBounds, currentBounds)
   - GetTopWindowAt → windowBを検出
   - SetParent(windowB)
     - windowA.RemoveChild(player)
     - windowB.AddChild(player)
4. 座標系はAddChild/RemoveChildで自動的に調整
```

#### 移動可能領域の計算 (PlayerWindowInteraction.GetValidRegion)
```
currentParent != null の場合:
1. CalculateMovableRegion(currentParent):
   - 親のクライアント領域をRegion化
   - タイトルバー高さを除外:
     titleBarHeight = parent.RectangleToScreen(parent.ClientRectangle).Y - parent.Location.Y
   - 子要素（ボタン等）の領域を除外:
     foreach child in parent.Children:
         movableRegion.Exclude(child.Bounds)

2. 親が最小化されている場合:
   return 空Region

currentParent == null の場合:
- メインフォーム全体（デスクトップ）
```

### 衝突判定の最適化

**Sweep Bounds方式**:
高速落下時の判定漏れを防ぐ。

```csharp
// 現在の足元の境界
Rectangle currentFeetBounds = new Rectangle(
    bounds.X,
    bounds.Bottom - 10,
    bounds.Width,
    groundCheckHeight
);

// 移動経路全体をカバーするSweep Bounds
float maxVerticalStep = Math.Max(Math.Abs(verticalVelocity * deltaTime), 20f);
Rectangle sweepBounds = new Rectangle(
    currentFeetBounds.X,
    Math.Min(currentFeetBounds.Y, currentFeetBounds.Y + (int)(verticalVelocity * deltaTime)) - 5,
    currentFeetBounds.Width,
    (int)maxVerticalStep + currentFeetBounds.Height + 10
);
```

### Z-order衝突判定システム

ゲーム全体のZ-order管理と衝突判定の統合システム。

#### WindowZOrderManagerのZ-order管理 (WindowZOrderManager.cs)

**内部リスト構造**:
```csharp
private readonly List<GameWindow> windowsList = new();
// インデックスが大きいほど前面（末尾が最前面）
```

**GetWindowZIndex** (WindowZOrderManager.cs:27-33):
```csharp
public int GetWindowZIndex(GameWindow window, IReadOnlyList<GameWindow> allWindows)
{
    return windowsList.IndexOf(window);
    // 戻り値: リスト内のインデックス（0=最背面、Count-1=最前面）
}
```

**BringWindowToFront** (WindowZOrderManager.cs:51-82):
```csharp
// 2025-01-26修正: 親の有無に関わらず統一的に処理
public void BringWindowToFront(GameWindow window, IReadOnlyList<GameWindow> allWindows)
{
    // 1. ウィンドウと全子孫をグループ化
    var windowGroup = new List<GameWindow> { window };
    windowGroup.AddRange(window.GetAllDescendants());

    // 2. 内部リストから削除
    foreach (var groupWindow in windowGroup)
    {
        windowsList.Remove(groupWindow);
    }

    // 3. 末尾に追加（最前面化）
    windowsList.AddRange(windowGroup);

    // 4. Win32 APIのZ-orderと同期
    // BringToFront()でネイティブZ-orderも更新
}
```

**重要な動作**:
- ウィンドウをクリックすると、そのウィンドウと全子孫が最前面に移動
- 親の有無に関わらず、クリックされたウィンドウが最前面
- 内部リストとネイティブZ-orderを常に一致させる → 衝突判定の正確性を保証

#### WindowCollisionDetectorの判定 (WindowCollisionDetector.cs)

**GetIntersectingWindows** (WindowCollisionDetector.cs:19-28):
```csharp
public List<GameWindow> GetIntersectingWindows(Rectangle bounds, IReadOnlyList<GameWindow> allWindows)
{
    return allWindows
        .Where(w => w.CollisionBounds.IntersectsWith(bounds))
        .OrderByDescending(w => zOrderManager.GetWindowZIndex(w, allWindows))
        .ToList();
    // 戻り値: Z-order降順（前面が先頭）のリスト
}
```

**AABB（Axis-Aligned Bounding Box）判定**:
```csharp
// Rectangle.IntersectsWithの内部動作:
bool IntersectsWith(Rectangle rect)
{
    return rect.X < this.Right &&
           this.X < rect.Right &&
           rect.Y < this.Bottom &&
           this.Y < rect.Bottom;
}
```

#### Region方式の可視性判定

**不可侵ウィンドウ境界の可視性判定**:
```csharp
// CheckNoEntryBoundaryCollision内部（NoEntryZoneManager.cs:735-860）:
foreach (var boundary in GetNoEntryBoundaryRectangles(window))
{
    using (Region boundaryRegion = new Region(boundary))
    {
        // より前面の不可侵ウィンドウで隠れている部分を除外
        for (int i = windowIndex + 1; i < windowsList.Count; i++)
        {
            var coveringWindow = windowsList[i];
            if (coveringWindow.IsNoEntryWindow && !coveringWindow.IsMinimized)
            {
                boundaryRegion.Exclude(coveringWindow.CollisionBounds);
            }
        }

        // 可視性判定
        using (var g = Graphics.FromHwnd(IntPtr.Zero))
        {
            if (!boundaryRegion.IsEmpty(g))
            {
                // 見えている境界部分がある → 衝突判定有効
                return (true, boundary);
            }
        }
    }
}
```

**通常ウィンドウの可視性判定**:
```csharp
// IsWindowVisibleFromNoEntry（NoEntryZoneManager.cs:886-956）:
Rectangle intersection = Rectangle.Intersect(checkBounds, normalWindow.CollisionBounds);
using (Region visibleRegion = new Region(intersection))
{
    // 前面の不可侵ウィンドウで隠れている部分を除外
    for (int i = normalWindowIndex + 1; i < windowsList.Count; i++)
    {
        var coveringWindow = windowsList[i];
        if (coveringWindow.IsNoEntryWindow)
        {
            visibleRegion.Exclude(coveringWindow.CollisionBounds);
        }
    }

    return !visibleRegion.IsEmpty(dummyGraphics);
}
```

**GDI+ Region API**:
- `Region.Intersect(Rectangle)`: 交差領域を作成
- `Region.Exclude(Rectangle)`: 指定領域を除外
- `Region.IsEmpty(Graphics)`: 空判定（すべて除外された場合true）

**Z-order考慮の衝突判定フロー**:
```
1. 衝突候補取得:
   intersectingWindows = GetIntersectingWindows(bounds)
   // Z-order降順でソート済み

2. 前面から順に判定:
   foreach window in intersectingWindows:
       if IsColliding(bounds, window):
           if 不可侵ウィンドウ:
               if CheckNoEntryBoundaryCollision成功:
                   return window  // 衝突確定
           else:
               if IsWindowVisibleFromNoEntry成功:
                   return window  // 衝突確定
           // 隠れている場合は次へ

3. 結果:
   - 最前面の可視ウィンドウに衝突
   - 隠れているウィンドウは無視される
```

### デスクトップ統合

**デスクトップアイコンの検出**:
```csharp
// Win32 APIを使用してデスクトップアイコンの位置を取得
var icons = DesktopIconHelper.Instance.GetDesktopIcons();

// 各アイコンに衝突境界を作成
foreach (var icon in icons)
{
    // アイコンがウィンドウに隠されていないかチェック
    if (IsIconVisibleAtPosition(icon, playerBounds))
    {
        // 衝突判定実行
        if (playerBounds.IntersectsWith(icon.Bounds))
        {
            // プレイヤーはアイコンの上に立っている
        }
    }
}
```

**パフォーマンス最適化**:
- アイコン情報のキャッシング（15秒）
- 近接アイコンのみを判定（Take(5)）
- ウィンドウとの重なり判定の最適化

### パフォーマンス監視

**PerformanceMonitor (IPerformanceMonitor)**:
```csharp
using (performanceMonitor.BeginScope("Player Update"))
{
    await player.UpdateAsync(deltaTime);
}

// スコープ終了時に自動的に計測時間を記録
```

**測定項目**:
- Total Update: 全体の更新時間
- Player Update: プレイヤー更新時間
- Window Update: ウィンドウ更新時間
- Total Render: 全体の描画時間

---

## 開発ワークフロー

### 新しいウィンドウタイプの追加

1. **WindowType列挙型に追加**
```csharp
public enum WindowType
{
    // 既存の型...
    NewType,  // 新しいウィンドウタイプ
}
```

2. **ストラテジークラスの実装**
```csharp
public class NewTypeWindowStrategy : BaseWindowStrategy
{
    public NewTypeWindowStrategy(
        IInputService? inputService = null,
        IGameSettings? gameSettings = null,
        INoEntryZoneManager? noEntryZoneManager = null)
        : base(inputService, gameSettings, noEntryZoneManager)
    {
    }

    public override void Update(GameWindow window, float deltaTime)
    {
        // 挙動の実装
    }

    public override void DrawStrategyMark(Graphics g, Rectangle bounds, bool isHovered)
    {
        // マークの描画
    }

    protected override Cursor GetStrategyCursor() => Cursors.Hand;
}
```

3. **WindowFactoryに追加**
```csharp
public GameWindow CreateWindow(WindowType type, Point location, Size size, ...)
{
    IWindowStrategy strategy = type switch
    {
        // 既存の型...
        WindowType.NewType => new NewTypeWindowStrategy(inputService, gameSettings, noEntryZoneManager),
        _ => throw new ArgumentException("Invalid window type", nameof(type))
    };
    // ...
}
```

4. **色の設定**
```csharp
window.BackColor = type switch
{
    // 既存の型...
    WindowType.NewType => Color.LightCoral,
    _ => Color.White
};
```

### 新しいステージの追加

1. **StageManager.InitializeStages()にデータ追加**
```csharp
stages.Add(new StageData
{
    StageNumber = 16,
    PlayerStart = new Point(100, 100),
    GoalPosition = new Point(700, 500),
    Windows = new List<WindowData>
    {
        new WindowData { Type = WindowType.Normal, Position = new Point(50, 300), Size = new Size(200, 50) },
        new WindowData { Type = WindowType.Movable, Position = new Point(300, 200), Size = new Size(150, 100) },
        // ...
    },
    NoEntryZones = new List<NoEntryZoneData>
    {
        new NoEntryZoneData { Position = new Point(0, 0), Size = new Size(50, 600) },
        // ...
    },
    Buttons = new List<ButtonData>
    {
        new ButtonData { Text = "Button", Position = new Point(400, 300) },
        // ...
    }
});
```

2. **ステージのテスト**
```bash
# アプリケーション起動
dotnet run --project MultiWindowActionGame

# ステージ16に到達するまでプレイ
# または直接ステージ番号を指定（開発用機能が必要）
```

### DIシステムの使用

**新しいサービスの追加**:

1. **インターフェースの定義** (`Interfaces/`)
```csharp
public interface INewService
{
    void DoSomething();
}
```

2. **実装クラスの作成**
```csharp
public class NewService : INewService
{
    private readonly IDependency dependency;

    public NewService(IDependency dependency)
    {
        this.dependency = dependency ?? throw new ArgumentNullException(nameof(dependency));
    }

    public void DoSomething()
    {
        // 実装
    }
}
```

3. **ServiceRegistrationに登録**
```csharp
private static void RegisterUtilityServices(IServiceContainer container)
{
    // 既存の登録...
    container.RegisterSingleton<INewService, NewService>();
}
```

4. **コンストラクタインジェクションで使用**
```csharp
public class ConsumerClass
{
    private readonly INewService newService;

    public ConsumerClass(INewService newService)
    {
        this.newService = newService ?? throw new ArgumentNullException(nameof(newService));
    }

    public void UseService()
    {
        newService.DoSomething();
    }
}
```

**重要**: 新しい静的参照（`.Current`プロパティなど）は作成しない。必ずDIを使用。

---

## デバッグとトラブルシューティング

### デバッグモード

**起動方法**: ゲーム実行中にF3キーを押す

**表示情報**:
- プレイヤー位置（X, Y）
- プレイヤー速度（垂直速度）
- 接地状態（Grounded/Airborne）
- 現在の親ウィンドウ
- FPS（Frames Per Second）
- 各更新処理の時間
- ゲーム設定値

**デバッグ用の描画**:
- プレイヤーの衝突境界（赤枠）
- プレイヤーの接地判定エリア（緑枠）
- ウィンドウの衝突境界（青枠）
- 不可侵領域（半透明の赤）
- Z-order情報

### よくある問題と解決策

#### 1. プレイヤーが床をすり抜ける

**原因**:
- 高速落下時にSweep Boundsが不足
- Z-orderの判定ミス
- 親ウィンドウの座標系変換エラー

**解決策**:
```csharp
// Sweep Boundsの範囲を拡大
float maxVerticalStep = Math.Max(Math.Abs(verticalVelocity * deltaTime), 20f);

// Z-order順にソート
var intersectingWindows = windowManager
    .GetIntersectingWindows(sweepBounds)
    .OrderByDescending(w => windowManager.GetWindowZIndex(w));
```

#### 2. ウィンドウのリサイズ時に子要素の位置がずれる

**原因**:
- スケール計算の精度不足
- 元サイズの記録ミス
- 循環参照による無限ループ

**解決策**:
```csharp
// 元サイズを正確に記録
private readonly Dictionary<IEffectTarget, Size> originalSizes = new();

// 循環更新を防ぐ
private readonly HashSet<IEffectTarget> currentlyUpdating = new HashSet<IEffectTarget>();

// より精密な元サイズ計算
var currentSize = child.GetOriginalSize();
originalSizes[child] = new Size(
    Math.Max(1, (int)Math.Round(currentSize.Width / scale.Width, MidpointRounding.AwayFromZero)),
    Math.Max(1, (int)Math.Round(currentSize.Height / scale.Height, MidpointRounding.AwayFromZero))
);
```

#### 3. ゲームが重い（FPSが低い）

**原因**:
- デスクトップアイコン判定の負荷
- 過剰な描画処理
- 不要なウィンドウ更新

**解決策**:
```csharp
// デスクトップアイコンの判定を最適化
var nearbyIcons = desktopIcons.Where(icon =>
    icon.Bounds.IntersectsWith(currentFeetBounds)
).Take(5).ToList();  // 最大5個まで

// パフォーマンス監視
using (performanceMonitor.BeginScope("Heavy Operation"))
{
    // 重い処理
}
```

#### 4. DIコンテナで循環依存エラー

**原因**:
- A → B → A のような循環依存

**解決策**:
```csharp
// 循環依存を避けるためにインターフェースを分割
// または、一方をファクトリーパターンで遅延生成

// 悪い例
public class A { public A(B b) { } }
public class B { public B(A a) { } }

// 良い例
public class A { public A(IB b) { } }
public class B { public B(IFactory<A> factory) { } }
```

### ログとエラーハンドリング

**Logger (ILogger)**:
```csharp
logger.LogInfo("Information message", "SystemName");
logger.LogDebug("Debug message", "SystemName");
logger.LogWarning("Warning message", "ContextInfo", "SystemName");
logger.LogError("Error message", exception, "SystemName");
```

**ErrorHandler (IErrorHandler)**:
```csharp
try
{
    // 危険な処理
}
catch (Exception ex)
{
    errorHandler.HandleError(
        "Error processing input",
        ErrorSeverity.Medium,
        ex,
        "SystemName"
    );
}
```

**ログファイル**: `logs/game.log`

---

## アーキテクチャの進化履歴

### Phase 1-6: サービス・システム統合（2024年12月-2025年1月）

**目的**: 静的クラスからDI対応サービスへの完全移行

**移行されたサービス**:
1. `GameTime` → `GameTimeService` (IGameTimeService)
2. `Input` → `InputService` (IInputService)
3. `SettingsNotification` → `NotificationService` (INotificationService)

**導入されたシステム**:
- `SystemManager`: ゲームシステムのライフサイクル管理
- `InputSystem`: グローバル入力処理（優先度100）
- `RenderingSystem`: レンダリングパイプライン（優先度400）

**削除されたシステム**:
- `PhysicsSystem`: `PlayerPhysics`と完全に重複していたため削除

**成果**:
- 静的クラス3つを完全削除
- DIアーキテクチャへの完全移行
- システムベースの更新フローの確立

### Phase 6.1: InputSystemデバッグモード切り替え統合（2025年1月）

**実装内容**:
- `IMainGame`インターフェースに`ToggleDebugMode()`メソッド追加
- `MainGame`クラスに実装
- `InputSystem`がDI経由で`IMainGame`を受け取り、F3キーでデバッグモード切り替え

**成果**:
- F3キーによるデバッグモードの動的切り替えが可能に
- InputSystemとMainGameの疎結合化
- 静的メソッド呼び出しからDI経由の呼び出しへ移行

**実装ファイル**:
- `IMainGame.cs`: インターフェース拡張
- `MainGame.cs`: メソッド実装（MainGame.cs:241）
- `InputSystem.cs`: DI統合とF3キー処理（InputSystem.cs:43）

### Phase 7: WindowStrategies完全DI化（2025年1月）

**実装内容**:
全7種類のウィンドウ戦略クラスをDI対応化：
- `BaseWindowStrategy`: 基底クラスにDI対応コンストラクタ追加
  - `IInputService`, `IGameSettings`, `INoEntryZoneManager`をフィールドで保持
  - Fallbackパターン採用（後方互換性維持）

**Fallbackパターン**:
```csharp
// DIできない場合は静的参照にフォールバック
settings = gameSettings?.Window ?? GameSettings.Current.Window;
var zoneManager = noEntryZoneManager ?? NoEntryZoneManager.Current;
```

**対応した戦略クラス**:
1. `NormalWindowStrategy`: 通常ウィンドウ
2. `ResizableWindowStrategy`: リサイズ可能ウィンドウ
3. `MovableWindowStrategy`: 移動可能ウィンドウ
4. `DeletableWindowStrategy`: 削除可能ウィンドウ
5. `MinimizableWindowStrategy`: 最小化可能ウィンドウ
6. `TextDisplayWindowStrategy`: テキスト表示ウィンドウ

**WindowFactory統合**:
- `WindowFactory`に`IInputService`と`INoEntryZoneManager`を注入
- 全戦略生成時にDI経由で依存を渡す

**置換された静的参照（5箇所）**:
1. BaseWindowStrategy.cs:42 - `GameSettings.Current.Window`
2. ResizableWindowStrategy.cs:196 - `NoEntryZoneManager.Current`
3. ResizableWindowStrategy.cs:352 - `NoEntryZoneManager.Current`
4. MovableWindowStrategy.cs:538 - `NoEntryZoneManager.Current`
5. MovableWindowStrategy.cs:565 - `NoEntryZoneManager.Current`

**成果**:
- ウィンドウ戦略の完全DI対応
- 静的参照5箇所をFallbackパターンで置換
- テスタビリティとモジュール性の向上
- 後方互換性の維持

### Phase 8-9: アーキテクチャ議論と設計判断（2025年1月）

**議論されたTODO**:

#### C1: PlayerPhysics入力処理責務分離 → **保留**

**現状**: PlayerPhysics.cs:43-56で入力→移動量変換を実行
```csharp
// TODO: 入力処理はPlayerInputHandlerに移動すべき（責務分離） - 優先度:保留
if (inputService.IsKeyDown(Keys.A) || inputService.IsKeyDown(Keys.Left))
{
    movement.X -= settings.MovementSpeed * deltaTime;
}
```

**保留理由**:
1. アーキテクチャ安定化優先（最近の大規模リファクタリング直後）
2. AI制御・リプレイ機能の予定なし（YAGNI原則）
3. ROI低（実装コスト2-4時間に対して具体的メリットなし）
4. テスト不足環境での大規模変更リスク

**再検討条件**:
- Player関連機能が1ヶ月以上安定稼働
- AI制御機能の実装が決定
- リプレイ機能の実装が決定
- 自動テスト環境の整備完了

**推奨設計案（将来の参考）**:
```csharp
// PlayerInputHandler拡張案
public interface IPlayerInputHandler
{
    float GetHorizontalMovement(float deltaTime);
}

// PlayerPhysics
public Vector2 CalculateMovement(float horizontalMovement, float deltaTime)
{
    return new Vector2(horizontalMovement, verticalVelocity * deltaTime);
}
```

#### C2: DebugDisplay.csのDI化 → **実装不要**

**現状**: DebugDisplay.cs:20で静的参照使用
```csharp
// TODO: GameSettings.Current - 優先度:低（実装不要）
var settings = GameSettings.Current;
```

**実装不要理由**:
1. デバッグ機能のため本番環境で問題にならない
2. ROI低（30分実装に対して具体的メリットなし）
3. Phase 1-7で主要な静的参照は完全削除済み（DI化達成率88%以上）
4. KISS原則（不要な抽象化を避ける）
5. 限られた開発リソースをユーザー価値の高い機能に集中

**代替対応**:
- TODOコメントに実装不要理由と優先度を明記
- 将来的なUI/レンダリング設計見直し時に再検討

### DI化プロジェクト最終成果（2025年1月11日完了）

**達成率**:
- **DI化達成率**: 88%以上（主要機能100%）
- **削除した静的参照**: 8箇所以上
- **残存する静的参照**: 1箇所（DebugDisplay.csのみ、デバッグ機能）

**完了したPhase**:
- Phase 1-6: サービス・システム統合（GameTimeService, InputService, NotificationService）
- Phase 6.1: InputSystemデバッグモード切り替え
- Phase 7: WindowStrategies DI化
- Phase 8-9: 優先度C群の設計議論と方針決定

**コミット数**: 4件
1. Phase 6.1実装
2. Phase 7実装
3. Phase 8-9議論完了
4. CLAUDE.md更新

**総所要時間**: 約2.5時間

**ビルド状態**: 0エラー、17警告（既存の警告のみ）

**設計原則の適用**:
- **YAGNI**: 不要な機能は実装しない（PlayerPhysics入力分離保留）
- **KISS**: シンプルに保つ（DebugDisplay DI化不要）
- **DRY**: 静的参照を削除し、DI経由に統一
- **アーキテクチャ安定化**: 大規模変更後の安定期間確保を優先

### Player責務分離リファクタリング（2024年）

**Before**: PlayerForm.cs - 1217行（すべての責務が集約）

**After**: コンポーネントベースアーキテクチャ
- PlayerForm.cs: 約700行（統合とレンダリング）
- PlayerPhysics.cs: 約350行（物理演算）
- PlayerInputHandler.cs: 約60行（入力処理）
- PlayerWindowInteraction.cs: 約200行（ウィンドウ相互作用）
- PlayerStateMachine.cs: 約100行（状態管理）

**成果**:
- 単一責任原則の適用
- テスタビリティの向上
- コンポーネント間の疎結合化
- 床すり抜けバグの修正（Sweep Bounds導入）
- ウィンドウ境界での接地判定改善

**削除された機能**:
- Legacy State（旧状態管理システム）

### Z-order同期修正（2025年1月26日）

**問題**:
親子ウィンドウと無関係なウィンドウが重なった際、子ウィンドウをクリックすると視覚的なZ-orderと内部リストのZ-orderが不一致になり、見えていないウィンドウに着地する不具合が発生。

**発生シナリオ**:
1. 初期状態: `[親ウィンドウ, 子ウィンドウ, 他のウィンドウ]`
2. 親をクリック → `[他のウィンドウ, 親ウィンドウ, 子ウィンドウ]` ✅ 正常
3. 子をクリック → **視覚**: 子が最前面、**内部リスト**: 変わらず ❌
4. プレイヤーが「他のウィンドウ」に着地（見えていないのに）

**原因**:
`WindowZOrderManager.BringWindowToFront`メソッドが、親を持つウィンドウに対して特別処理を行い、兄弟間の順序のみ変更していた。その結果：
- Win32 APIによる視覚的なZ-orderは更新される
- 内部リスト（衝突判定で使用）は更新されない
- 視覚と判定の不一致が発生

**修正内容**:
WindowZOrderManager.cs:51-61を簡素化（約40行→10行）:
```csharp
// 修正前: 親がある場合は兄弟間の順序のみ調整
if (window.Parent != null) { /* 複雑なロジック */ }
else { /* 最前面に移動 */ }

// 修正後: 親の有無に関わらず統一的に処理
var windowGroup = new List<GameWindow> { window };
windowGroup.AddRange(window.GetAllDescendants());
windowsList.Remove(groupWindow);
windowsList.AddRange(windowGroup);  // 最前面に移動
```

**成果**:
- ✅ 子ウィンドウをクリック → 子だけが最前面に（親は現在位置のまま）
- ✅ 視覚的なZ-orderと内部リストが完全一致
- ✅ 衝突判定が正しく動作（見えているウィンドウにのみ着地）
- ✅ コードの簡素化とメンテナンス性向上

**影響範囲**:
- WindowZOrderManager.BringWindowToFrontメソッドのみ
- 既存の動作（親なしウィンドウ）には影響なし
- 親子ウィンドウの動作が期待通りに変更

**コミット**: `9cd9cbb` (2025年1月26日)

### 衝突判定統一化リファクタリング（2025年1月）

**背景**:
NoEntryZoneManagerが962行の巨大クラスとなり、複数の責務（静的領域管理、境界判定、Z-order処理、衝突検証）を持っていた。

**実装内容**:

**段階1: サービス層分離（7つの新規クラス作成）**
- `NoEntryBoundaryCollider`: 不可侵ウィンドウの境界判定（Region方式）
- `ZOrderVisibilityService`: Z-order考慮の可視性判定
- `ZOrderCollisionHelper`: Z-order優先度管理
- `CollisionValidator`: 位置・サイズ検証ロジック（260行）
- `CollisionService`: 統一的な衝突判定API（ICollisionService）
- `CollisionOptions`: 衝突判定の設定オプション
- `CollisionFlags`: オプションのビルダーパターン

**段階2: CollisionService統合**
- WindowStrategiesに4箇所でCollisionService導入
  - ResizableWindowStrategy.UpdateResize (line 225)
  - ResizableWindowStrategy.CalculateNewSize (line 415)
  - MovableWindowStrategy.CalculateMovement (line 640)
  - MovableWindowStrategy.CheckCollision (line 680)
- WindowFactoryにICollisionService注入
- ServiceRegistrationに登録

**NoEntryZoneManagerリファクタリング結果**:
- **962行 → 250行**（74%削減、712行削除）
- 責務を明確化（静的NoEntryZone管理のみ）
- 境界判定、Z-order処理、検証ロジックを分離

**成果**:
- ✅ 単一責任原則の適用
- ✅ テスタビリティ向上（各サービスを個別にテスト可能）
- ✅ 保守性向上（関心事の分離）
- ✅ 拡張性向上（新しい衝突判定ルールの追加が容易）
- ✅ 重複コード約100行削除
- ✅ DI対応の統一的なAPI

**バグ修正**:
1. 不可侵ウィンドウが通常ウィンドウと衝突しない問題を解決
   - CollisionOptionsのCheckNormalWindowsフラグを動的に設定
2. 不可侵ウィンドウが通常ウィンドウに高速移動時に空中停止する問題を解決
   - CollisionValidator.ValidatePositionにX/Y軸独立検証を実装
   - 通常ウィンドウに対しても境界まで正確に移動可能に

**コミット数**: 3件
1. 段階1: サービス層分離
2. 段階2: CollisionService統合
3. バグ修正2件（通常ウィンドウ衝突関連）

**実装期間**: 約3時間

---

## 参考資料

### プロジェクト内ドキュメント
- `config/settings.json`: ゲーム設定ファイル

### コード規約
1. **命名規則**:
   - クラス: PascalCase（例: `PlayerPhysics`）
   - メソッド: PascalCase（例: `CalculateMovement`）
   - フィールド: camelCase（例: `verticalVelocity`）
   - プライベートフィールド: camelCase（例: `isGrounded`）
   - インターフェース: I + PascalCase（例: `IPlayerPhysics`）

2. **コメント規約**:
   - 日本語と英語混在OK
   - TODOコメントには優先度と判断理由を明記
   ```csharp
   // TODO: 機能名 - 優先度: 高/中/低/保留/実装不要
   // 判断理由（日付）:
   // - 理由1
   // - 理由2
   ```

3. **DIパターン**:
   - コンストラクタインジェクション必須
   - null check必須（ArgumentNullException）
   - Fallbackパターン使用時はコメント明記

4. **非同期パターン**:
   - I/O操作は`async/await`使用
   - メソッド名は`~Async`サフィックス
   - ConfigureAwait(false)は使用しない（WinFormsのため）

5. **エラーハンドリング**:
   - try-catchは最小限に
   - ErrorHandlerを使用
   - ログ出力必須

---

## 制約事項と既知の問題

### システム要件
- **OS**: Windows 10/11（Win32 API依存）
- **.NET**: .NET 6.0以上
- **権限**: デスクトップアイコン検出には特定の権限が必要
- **画面**: 1920x1080以上推奨（マルチウィンドウ表示のため）

### 既知の問題（17警告）
1. **CS0168**: 未使用の例外変数（try-catchブロック）
   - 影響: なし（意図的にエラーを無視）
   - 対応: 必要に応じて`_`でディスカード

2. **CS8604**: Null参照の可能性
   - 影響: 低（実行時には常に非nullが保証されている）
   - 対応: null許容参照型の適切な適用

3. **NETSDK1138**: .NET 6.0サポート終了警告
   - 影響: なし（セキュリティ更新のみ）
   - 対応: .NET 8.0への移行検討

### 技術的負債
1. **自動テストの欠如**
   - 手動テストのみ
   - リグレッションリスク

2. **Legacy/ディレクトリの存在**
   - 旧実装が残存
   - 将来的に削除予定

3. **デスクトップアイコン検出の不安定性**
   - Win32 API依存
   - 一部環境で動作しない可能性

---

## 今後の展望

### 近い将来（1-3ヶ月）
- [ ] Phase 6.1-7の動作確認と安定化
- [ ] 既存の17警告の解消
- [ ] パフォーマンス最適化
- [ ] 追加ステージの実装

### 中期（3-6ヶ月）
- [ ] 自動テストフレームワークの導入
- [ ] .NET 8.0への移行
- [ ] Legacy/ディレクトリの完全削除
- [ ] UI/UX改善

### 長期（6ヶ月以上）
- [ ] PlayerPhysics入力処理分離（条件満たした場合）
- [ ] AI制御プレイヤー（検討中）
- [ ] リプレイ機能（検討中）
- [ ] マルチプレイヤー対応（未定）

---

## 更新履歴

- **2025-01-27**: CLAUDE.mdに内部挙動詳細仕様を追加 🆕
  - **ウィンドウの境界システム**セクション追加
    - CollisionBounds、AdjustedBounds、FullBounds/DisplayBoundsの定義と使い分け
    - タイトルバーの高さ計算
  - **各WindowStrategyの詳細挙動**を大幅拡張
    - ResizableWindowStrategy: CalculateNewSize、子要素スケーリング、パフォーマンス最適化
    - MovableWindowStrategy: CalculateMovement、方向別ブロック管理、DragSpeed適用
    - NoEntry系戦略: 不可侵境界生成（外周5px、4辺）、Z-order + Region判定
  - **NoEntryZoneManagerの詳細アルゴリズム**を追加
    - IntersectsWithAnyZoneの3段階チェックフロー
    - GetValidPositionのX/Y軸独立調整アルゴリズム
    - GetValidSizeの制約適用ロジック（X軸調整後の幅をY軸で使用）
    - IsWindowVisibleFromNoEntryのRegion方式可視性判定
  - **PlayerPhysicsの物理演算詳細**を追加
    - 重力システム（Gravity: 980px/s²）
    - ジャンプメカニズム（JumpForce: 450px/s）
    - Sweep Bounds方式の接地判定（6段階優先順位）
    - OnGroundedコールバックの役割
  - **親子関係の詳細メカニズム**を追加
    - CheckPotentialParentWindowの確立アルゴリズム（6ステップ）
    - OnParentMoveの移動時処理（再帰伝播）
    - OnParentResizeのリサイズ時処理（スケール累積、循環更新防止）
    - 座標系変換（グローバル ↔ ローカル）
  - **ウィンドウ間遷移の詳細**を追加
    - HandleWindowTransitionsの2パターン判定
    - GetTopWindowAtの5点チェックアルゴリズム
    - SetParentの遷移時処理
    - 移動可能領域の計算（CalculateMovableRegion）
  - **Z-order衝突判定システム**セクション追加
    - WindowZOrderManagerのZ-order管理（BringWindowToFront）
    - WindowCollisionDetectorの判定（GetIntersectingWindows、AABB）
    - Region方式の可視性判定（不可侵境界、通常ウィンドウ）
    - GDI+ Region API（Intersect、Exclude、IsEmpty）
    - Z-order考慮の衝突判定フロー
  - コード参照を各所に追加（ファイル名:行番号形式）
- **2025-01-27**: 不可侵ウィンドウの改善と最適化 🆕
  - WindowType追加: NormalBlackNoEntry, NormalWhiteNoEntry（通常ウィンドウの不可侵バリエーション）
  - 不可侵ウィンドウのマーク常時表示修正（ホバー時のみ表示 → 常時表示、色はホバー状態で変化）
  - リサイズ時のX/Y軸独立性修正
    - GetValidSizeでY軸チェック時にX軸調整後の幅を使用
    - 横方向の衝突が縦方向のサイズに影響しない問題を解決
  - Z-order考慮の通常ウィンドウ衝突判定実装
    - IsWindowVisibleFromNoEntryメソッド追加（Region方式）
    - 不可侵ウィンドウに隠れている通常ウィンドウは衝突判定をスキップ
    - IntersectsWithAnyZoneとGetValidPositionに可視性チェック適用
    - 見えている部分のみが衝突判定に影響
  - GetValidPositionの最適化（不要なforeachループ削除）
  - コミット: b8ef878, 06ac984, 83c9cf3, f4c1337, 1bec7ce, d660bf2
- **2025-01-27**: 不可侵機能を持つウィンドウバリエーション実装 🆕
  - WindowType追加: ResizableNoEntry, MovableNoEntry, MinimizableNoEntry
  - 既存ストラテジーを継承した専用ストラテジークラス実装
    - ResizableNoEntryWindowStrategy（リサイズ + 不可侵）
    - MovableNoEntryWindowStrategy（移動 + 不可侵）
    - MinimizableNoEntryWindowStrategy（最小化 + 不可侵）
  - リサイズ/移動/最小化機能と不可侵境界機能の組み合わせ
  - 赤いアウトライン + 各種マークで視覚的に識別
  - Z-order + Region考慮の境界判定をすべてのバリエーションに適用
  - BaseWindowStrategyにIsNoEntryプロパティ追加
  - Stage 2にサンプル配置
- **2025-01-27**: 不可侵ゲームウィンドウ（NoEntry）実装完了 + Z-order対応実装
  - WindowType.NoEntry追加
  - NoEntryWindowStrategy実装（静的ウィンドウ、境界が不可侵領域）
  - NoEntryZoneManagerにZ-order対応の境界判定実装（Region方式）
    - NoEntryZoneオブジェクト生成を廃止（パフォーマンス改善）
    - 境界線ベース判定 + GDI+ Region使用の正確な部分隠れ判定
    - 他のウィンドウに隠れている部分は衝突判定なし
    - 新規メソッド: GetNoEntryBoundaryRectangles, CheckNoEntryBoundaryCollision, CheckAnyNoEntryBoundaryCollision
  - GameWindowに最小化/復元時の不可侵領域制御追加
  - 赤いアウトラインで視覚表現
  - MovableWindowとResizableWindowに親子制約機能追加（親が不可侵ウィンドウの場合、境界を超えられない）
- **2025-01-26**: Z-order同期修正（親子ウィンドウの衝突判定バグ解決）
- **2025-01-11**: CLAUDE.md全面改訂（日本語化、詳細仕様追加）
- **2025-01-11**: Phase 8-9完了（設計議論と方針決定）
- **2025-01-11**: Phase 7完了（WindowStrategies DI化）
- **2025-01-11**: Phase 6.1完了（InputSystemデバッグモード切り替え）
- **2025-01-**: Phase 1-6完了（サービス・システム統合）
- **2024**: Player責務分離リファクタリング完了
- **2024**: DI化プロジェクト開始

---

*このドキュメントはプロジェクトの進化と共に更新されます。*
