# WindowAction 設計仕様書 v2.0（破棄済み）

> **この仕様書はアーカイブです。ここに記載された設計は採用されませんでした。**
> 「完全自己完結で1.44MB以下」という要件を確実に満たすため、
> C# .NET / NativeAOT + SDL2 + SkiaSharpによるクロスプラットフォーム化ではなく、
> `native/`配下のC/Win32ネイティブ実装（Windows専用、依存DLLなしの単一exe）を
> 採用する方針に確定しています。詳細は `CLAUDE.md` を参照してください。
> 以下は当時の設計検討の記録として残しています。
>
> 現行実装を全面的に分析し、設計を改善した上でゼロから再構築するための仕様書。  
> 現行: C# .NET 6 / WinForms (Windows専用)  
> 新設計: C# .NET 9 / SDL2 + SkiaSharp / NativeAOT / クロスプラットフォーム

---

## 目次

1. [ゲームコンセプト](#1-ゲームコンセプト)
2. [技術スタック](#2-技術スタック)
3. [プロジェクト構成](#3-プロジェクト構成)
4. [共通データ型](#4-共通データ型)
5. [DIシステム](#5-diシステム)
6. [ゲームループ](#6-ゲームループ)
7. [入力システム](#7-入力システム)
8. [ウィンドウシステム](#8-ウィンドウシステム)
   - 8.9 [カスタムウィンドウクローム](#89-カスタムウィンドウクローム)
   - 8.10 [ウィンドウ外観カスタマイズ（WindowAppearance）](#810-ウィンドウ外観カスタマイズwindowappearance)
   - 8.11 [ウィンドウ変換・反転システム（WindowTransform）](#811-ウィンドウ変換反転システムwindowtransform)
   - 8.12 [座標変換パイプライン](#812-座標変換パイプライン)
9. [物理・衝突システム](#9-物理衝突システム)
10. [プレイヤーシステム](#10-プレイヤーシステム)
11. [ステージシステム](#11-ステージシステム)
12. [描画システム](#12-描画システム)
13. [UIシステム](#13-uiシステム)
14. [デスクトップ統合](#14-デスクトップ統合)
15. [設定システム](#15-設定システム)
16. [デバッグシステム](#16-デバッグシステム)
17. [ロギング・エラーハンドリング](#17-ロギングエラーハンドリング)
18. [現行設計からの改善点](#18-現行設計からの改善点)

---

## 1. ゲームコンセプト

### 1.1 概要

**WindowAction** はOSのデスクトップ環境を舞台にした2Dアクションパズルゲーム。  
プレイヤーは複数のOSウィンドウを足場として跳び移り、ゴールを目指す。  
ウィンドウのリサイズ・移動・最小化といったOSの操作そのものがパズルの鍵になる。

### 1.2 ゲームルール

- プレイヤーは左右移動・ジャンプで移動する
- 各ゲームウィンドウの上面が足場になる
- 不可侵ウィンドウ（NoEntry）の外周5pxは壁・天井・床として機能する
- ゴール（"G"マーク）に触れるとステージクリア
- デスクトップアイコンも足場として機能するステージがある
- ウィンドウを操作（移動・リサイズ・最小化）してルートを切り開く

### 1.3 ゲーム状態

```
┌─────────┐     Start      ┌─────────┐    Clear    ┌──────────┐
│  Title  │ ─────────────► │ Playing │ ──────────► │ StageClear│
└─────────┘                └─────────┘             └──────────┘
     ▲                          │ Retry                  │ Next
     │    ToTitle               ▼                        ▼
     └──────────────── ┌─────────────────┐       ┌──────────┐
                       │ (同ステージ再起) │       │ Playing  │
                       └─────────────────┘       └──────────┘
                                                       │ Stage22クリア
                                                       ▼
                                                 ┌──────────┐
                                                 │ GameClear │
                                                 └──────────┘
```

### 1.4 操作

| キー | 動作 |
|------|------|
| A / 左矢印 | 左移動 |
| D / 右矢印 | 右移動 |
| W / 上矢印 / Space | ジャンプ |
| F3 | デバッグ表示切替 |
| F1 | 設定画面表示 |
| Shift + Esc | 緊急終了 |

---

## 2. 技術スタック

### 2.1 ランタイム・言語

| 項目 | 選択 | 理由 |
|------|------|------|
| 言語 | C# 13 | |
| ランタイム | .NET 9 | NativeAOT対応、LTS |
| 発行形式 | NativeAOT | ランタイム依存なし、数MB〜十数MB |

### 2.2 ライブラリ

| 役割 | ライブラリ | NuGetパッケージ |
|------|-----------|----------------|
| ウィンドウ管理・入力 | SDL2 (via Silk.NET) | `Silk.NET.SDL` |
| 2D描画 | SkiaSharp | `SkiaSharp` |
| 数値型 | System.Numerics | 標準ライブラリ |
| JSON | System.Text.Json | 標準ライブラリ |
| ネイティブ互換 | 各OSのP/Invoke | プラットフォーム別 |

### 2.3 対象プラットフォーム

| OS | ウィンドウ管理 | デスクトップ統合 |
|----|--------------|----------------|
| Windows 10/11 | SDL2 + Win32 | 完全対応 (Shell API) |
| macOS 12+ | SDL2 + AppKit | 部分対応 (NSWorkspace) |
| Linux (X11) | SDL2 + X11 | 限定対応 (stub) |

---

## 3. プロジェクト構成

```
WindowAction/
├── src/
│   ├── Core/                      # プラットフォーム非依存コアロジック
│   │   ├── DI/                    # DIコンテナ
│   │   ├── Loop/                  # ゲームループ
│   │   ├── Systems/               # システム管理
│   │   ├── Events/                # イベントバス
│   │   └── Math/                  # 数値型・ユーティリティ
│   │
│   ├── Game/                      # ゲームロジック本体
│   │   ├── Player/                # プレイヤーシステム
│   │   ├── Windows/               # ゲームウィンドウシステム
│   │   ├── Physics/               # 物理演算
│   │   ├── Collision/             # 衝突判定
│   │   ├── Stages/                # ステージシステム
│   │   ├── UI/                    # UIコンポーネント
│   │   └── Settings/              # ゲーム設定
│   │
│   ├── Rendering/                 # 描画エンジン（SkiaSharp）
│   │   ├── IRenderer.cs
│   │   ├── SkiaRenderer.cs
│   │   ├── Animation/
│   │   └── Debug/
│   │
│   ├── Platform/                  # プラットフォーム抽象層
│   │   ├── Abstractions/          # インターフェース定義
│   │   ├── Windows/               # Windows実装
│   │   ├── MacOS/                 # macOS実装
│   │   └── Linux/                 # Linux実装（stub含む）
│   │
│   └── Program.cs                 # エントリーポイント
│
├── stages/                        # ステージ定義JSON
│   ├── stage_00_title.json
│   ├── stage_01.json
│   ⋮
│   └── stage_22_gameclear.json
│
├── assets/
│   └── fonts/
│       ├── prstart.ttf
│       └── prstartk.ttf
│
└── config/
    └── settings.json
```

---

## 4. 共通データ型

WinFormsの型（Rectangle, Point, Size, Color）に依存しないプラットフォーム共通の型を定義する。

### 4.1 基本型

```csharp
// 2D矩形（整数座標）
public readonly record struct Rect(int X, int Y, int Width, int Height)
{
    public int Left   => X;
    public int Top    => Y;
    public int Right  => X + Width;
    public int Bottom => Y + Height;
    public Vec2i Location => new(X, Y);
    public Vec2i Size     => new(Width, Height);
    public Vec2i Center   => new(X + Width / 2, Y + Height / 2);

    public bool Contains(Vec2i point)
        => point.X >= Left && point.X < Right && point.Y >= Top && point.Y < Bottom;

    public bool IntersectsWith(Rect other)
        => Left < other.Right && Right > other.Left
        && Top < other.Bottom && Bottom > other.Top;

    public bool Contains(Rect other)
        => other.Left >= Left && other.Right <= Right
        && other.Top >= Top && other.Bottom <= Bottom;

    public static Rect Intersect(Rect a, Rect b) { /* ... */ }
    public static Rect Union(Rect a, Rect b) { /* ... */ }
    public static readonly Rect Empty = new(0, 0, 0, 0);
    public bool IsEmpty => Width <= 0 || Height <= 0;
}

// 2D整数ベクトル（位置・サイズ共用）
public readonly record struct Vec2i(int X, int Y)
{
    public static Vec2i Zero  => new(0, 0);
    public static Vec2i operator +(Vec2i a, Vec2i b) => new(a.X + b.X, a.Y + b.Y);
    public static Vec2i operator -(Vec2i a, Vec2i b) => new(a.X - b.X, a.Y - b.Y);
}

// 2D浮動小数点ベクトル（物理演算用）
public readonly record struct Vec2f(float X, float Y)
{
    public static Vec2f Zero => new(0, 0);
    public static Vec2f operator +(Vec2f a, Vec2f b) => new(a.X + b.X, a.Y + b.Y);
    public static Vec2f operator *(Vec2f v, float s)  => new(v.X * s, v.Y * s);
    public Vec2i ToVec2i() => new((int)X, (int)Y);
    public float Length => MathF.Sqrt(X * X + Y * Y);
}

// RGBA色
public readonly record struct GameColor(byte R, byte G, byte B, byte A = 255)
{
    public static GameColor White      => new(255, 255, 255);
    public static GameColor Black      => new(0, 0, 0);
    public static GameColor Red        => new(255, 0, 0);
    public static GameColor Gold       => new(255, 215, 0);
    public static GameColor Transparent => new(0, 0, 0, 0);
    public GameColor WithAlpha(byte a) => new(R, G, B, a);
    public float Brightness => (R * 0.299f + G * 0.587f + B * 0.114f) / 255f;
}
```

### 4.2 ゲーム固有型

```csharp
// ウィンドウIDの型エイリアス（Guid）
public readonly record struct WindowId(Guid Value)
{
    public static WindowId New() => new(Guid.NewGuid());
    public static WindowId Empty => new(Guid.Empty);
}

// ステージ番号
public readonly record struct StageIndex(int Value)
{
    public bool IsTitle    => Value == 0;
    public bool IsGameClear => Value == 22;
}
```

### 4.3 WindowTransform（制限なしリサイズ・反転対応）

UnconstrainedResizeウィンドウのための変換構造体。  
論理サイズ（LogicalSize）は負値を取ることができ、負の場合はウィンドウが反転する。

```csharp
public readonly record struct WindowTransform
{
    // アンカー点（リサイズの固定点）= 常に左上の視覚的起点
    public Vec2i Anchor { get; init; }

    // 論理サイズ（マイナス可）
    //   Width  < 0 → FlipX（左右反転）
    //   Height < 0 → FlipY（上下反転）
    public Vec2i LogicalSize { get; init; }

    // 常に正のサイズ
    public Vec2i AbsoluteSize => new(Math.Abs(LogicalSize.X), Math.Abs(LogicalSize.Y));

    // 反転フラグ
    public bool FlipX => LogicalSize.X < 0;
    public bool FlipY => LogicalSize.Y < 0;

    // 視覚的な左上隅座標（FlipX/FlipYを考慮）
    public Vec2i VisualTopLeft => new(
        FlipX ? Anchor.X + LogicalSize.X : Anchor.X,   // LogicalSize.X が負なので加算で左へ
        FlipY ? Anchor.Y + LogicalSize.Y : Anchor.Y
    );

    // 衝突判定用の矩形（常に正の幅・高さ）
    public Rect CollisionRect => new(VisualTopLeft.X, VisualTopLeft.Y, AbsoluteSize.X, AbsoluteSize.Y);

    // クライアント領域（タイトルバーを除く）の矩形
    // FlipY時はタイトルバーが下端になるため上端が ClientTop
    public Rect ClientRect(int titleBarHeight) => FlipY
        ? new(VisualTopLeft.X, VisualTopLeft.Y, AbsoluteSize.X, AbsoluteSize.Y - titleBarHeight)
        : new(VisualTopLeft.X, VisualTopLeft.Y + titleBarHeight, AbsoluteSize.X, AbsoluteSize.Y - titleBarHeight);

    // タイトルバーの矩形
    public Rect TitleBarRect(int titleBarHeight) => FlipY
        ? new(VisualTopLeft.X, VisualTopLeft.Y + AbsoluteSize.Y - titleBarHeight, AbsoluteSize.X, titleBarHeight)
        : new(VisualTopLeft.X, VisualTopLeft.Y, AbsoluteSize.X, titleBarHeight);

    // SkiaSharp描画用の変換行列（クライアント領域への描画に適用）
    //   FlipX: スケール(-1, 1) + 横移動(ClientRect.Width)
    //   FlipY: スケール(1, -1) + 縦移動(ClientRect.Height)
    public (Vec2f Scale, Vec2i Translate) GetContentTransform(int titleBarHeight)
    {
        var client = ClientRect(titleBarHeight);
        float sx = FlipX ? -1f : 1f;
        float sy = FlipY ? -1f : 1f;
        int tx = FlipX ? client.Width  : 0;
        int ty = FlipY ? client.Height : 0;
        return (new Vec2f(sx, sy), new Vec2i(tx, ty));
    }
}
```

**反転時の視覚的レイアウト:**

```
通常（FlipX=false, FlipY=false）:
╔══════════════════════╗  ← VisualTopLeft = Anchor
║ [Title]        [－×] ║  ← タイトルバー（上端）
╠══════════════════════╣
║  クライアント領域     ║
╚══════════════════════╝

FlipX のみ（Width < 0）:
╔══════════════════════╗  ← VisualTopLeft = Anchor + (LogicalWidth, 0)
║ [×－]       [eltiT] ║  ← ボタンが左上、テキスト左右反転
╠══════════════════════╣
║  コンテンツ左右反転   ║
╚══════════════════════╝

FlipY のみ（Height < 0）:
╔══════════════════════╗  ← VisualTopLeft = Anchor + (0, LogicalHeight)
║  コンテンツ上下反転   ║
╠══════════════════════╣
║ [－×]        [eltit] ║  ← タイトルバーが下端、テキスト上下反転
╚══════════════════════╝

FlipX + FlipY（Width < 0 かつ Height < 0）:
╔══════════════════════╗  ← VisualTopLeft = Anchor + (LogicalWidth, LogicalHeight)
║  コンテンツ上下左右反転║
╠══════════════════════╣
║ [×－]      [eltiT]  ║  ← タイトルバーが下端＋左右反転
╚══════════════════════╝
```

---

## 5. DIシステム

### 5.1 設計方針

- NativeAOT対応のため **リフレクション禁止**
- コンストラクタインジェクションのみ
- ファクトリー関数（`Func<T>`）による遅延生成
- Singleton / Transient の2種ライフタイム

### 5.2 インターフェース

```csharp
public interface IServiceContainer
{
    void RegisterSingleton<TInterface, TImpl>() where TImpl : TInterface;
    void RegisterSingleton<T>(T instance);
    void RegisterSingleton<T>(Func<IServiceContainer, T> factory);
    void RegisterTransient<TInterface, TImpl>() where TImpl : TInterface;
    void RegisterTransient<T>(Func<IServiceContainer, T> factory);

    T Resolve<T>();
    bool IsRegistered<T>();
}
```

### 5.3 実装（リフレクション不使用）

```csharp
public sealed class ServiceContainer : IServiceContainer
{
    // すべての登録はFactoryで管理（リフレクション廃止）
    private readonly Dictionary<Type, Func<IServiceContainer, object>> _factories = new();
    private readonly Dictionary<Type, object> _singletons = new();
    private readonly HashSet<Type> _resolving = new();  // 循環依存検出

    public void RegisterSingleton<T>(Func<IServiceContainer, T> factory) where T : notnull
    {
        _factories[typeof(T)] = c => factory(c)!;
    }

    public T Resolve<T>()
    {
        var type = typeof(T);
        if (_singletons.TryGetValue(type, out var existing))
            return (T)existing;

        if (_resolving.Contains(type))
            throw new InvalidOperationException($"Circular dependency: {type.Name}");

        _resolving.Add(type);
        try
        {
            var instance = (T)_factories[type](this);
            if (IsSingleton(type))
                _singletons[type] = instance;
            return instance;
        }
        finally { _resolving.Remove(type); }
    }
}
```

### 5.4 登録（ServiceRegistration）

すべての依存関係をここに明示的に登録する。コンストラクタで型が変わるたびにここも更新する。

```csharp
public static class ServiceRegistration
{
    public static void Register(IServiceContainer c)
    {
        // インフラ
        c.RegisterSingleton<ILogger>(    _ => new FileLogger("logs/game.log"));
        c.RegisterSingleton<IErrorHandler>(c => new ErrorHandler(c.Resolve<ILogger>()));
        c.RegisterSingleton<IEventBus>(  _ => new EventBus());

        // プラットフォーム
        c.RegisterSingleton<IPlatformWindowSystem>(_ => PlatformFactory.CreateWindowSystem());
        c.RegisterSingleton<IDesktopIntegration>(  _ => PlatformFactory.CreateDesktopIntegration());
        c.RegisterSingleton<IInputProvider>(       _ => PlatformFactory.CreateInputProvider());

        // 描画
        c.RegisterSingleton<IRenderer>(c =>
            new SkiaRenderer(c.Resolve<IPlatformWindowSystem>()));

        // ゲームサービス
        c.RegisterSingleton<IGameSettings>(   c => new GameSettings(c.Resolve<IEventBus>()));
        c.RegisterSingleton<IGameTimeService>(_ => new GameTimeService());
        c.RegisterSingleton<IInputService>(   c => new InputService(c.Resolve<IInputProvider>()));

        // ウィンドウ
        c.RegisterSingleton<IZOrderManager>(        _ => new ZOrderManager());
        c.RegisterSingleton<IWindowHierarchyManager>(_ => new WindowHierarchyManager());
        c.RegisterSingleton<INoEntryZoneManager>(   c => new NoEntryZoneManager(
            c.Resolve<IZOrderManager>()));
        c.RegisterSingleton<ICollisionService>(     c => new CollisionService(
            c.Resolve<INoEntryZoneManager>(),
            c.Resolve<IZOrderManager>()));
        c.RegisterSingleton<IWindowManager>(        c => new WindowManager(
            c.Resolve<IZOrderManager>(),
            c.Resolve<IWindowHierarchyManager>(),
            c.Resolve<ICollisionService>()));
        c.RegisterSingleton<IWindowFactory>(        c => new WindowFactory(
            c.Resolve<IWindowManager>(),
            c.Resolve<ICollisionService>(),
            c.Resolve<IGameSettings>()));

        // プレイヤー
        c.RegisterTransient<IPlayerPhysics>(  c => new PlayerPhysics(
            c.Resolve<IGameSettings>(),
            c.Resolve<IInputService>()));
        c.RegisterTransient<IPlayerInput>(    c => new PlayerInput(c.Resolve<IInputService>()));
        c.RegisterSingleton<IPlayerFactory>(  c => new PlayerFactory(
            c,
            c.Resolve<IGameSettings>(),
            c.Resolve<IWindowManager>(),
            c.Resolve<INoEntryZoneManager>()));

        // ゲームコア
        c.RegisterSingleton<IStageManager>(c => new StageManager(
            c.Resolve<IWindowFactory>(),
            c.Resolve<IWindowManager>(),
            c.Resolve<INoEntryZoneManager>(),
            c.Resolve<IPlayerFactory>(),
            c.Resolve<IEventBus>()));
        c.RegisterSingleton<ISystemManager>(_ => new SystemManager());
        c.RegisterSingleton<IMainGame>(     c => new MainGame(
            c.Resolve<ISystemManager>(),
            c.Resolve<IStageManager>(),
            c.Resolve<IGameTimeService>(),
            c.Resolve<IEventBus>(),
            c.Resolve<IGameSettings>()));

        // システム登録
        c.RegisterSingleton<InputSystem>(c => new InputSystem(
            c.Resolve<IInputService>(),
            c.Resolve<IMainGame>(),
            c.Resolve<IStageManager>()));
        c.RegisterSingleton<PhysicsSystem>(c => new PhysicsSystem(
            c.Resolve<IStageManager>()));
        c.RegisterSingleton<RenderingSystem>(c => new RenderingSystem(
            c.Resolve<IRenderer>(),
            c.Resolve<IWindowManager>(),
            c.Resolve<IStageManager>(),
            c.Resolve<INoEntryZoneManager>(),
            c.Resolve<IGameTimeService>()));
    }
}
```

---

## 6. ゲームループ

### 6.1 設計（改善点：固定物理タイムステップ）

現行: 可変デルタタイムで物理演算  
**新設計: 固定タイムステップ物理演算（120Hz）+ 可変レンダリング（最大60fps）**

固定タイムステップにより物理演算が決定論的になり、フレームレートに依存しない安定した動作を実現する。

```
PHYSICS_STEP = 1.0f / 120f   // 物理更新間隔 8.33ms
TARGET_FRAME = 1.0f / 60f    // 描画更新間隔 16.67ms

accumulator = 0
while (running)
{
    rawDeltaTime = clock.ElapsedAndReset()
    deltaTime    = clamp(rawDeltaTime, 0, MAX_DELTA_TIME=0.1)

    // 入力システム更新（毎フレーム）
    inputSystem.Update()

    // 固定タイムステップ物理更新
    accumulator += deltaTime
    while (accumulator >= PHYSICS_STEP)
    {
        physicsSystem.FixedUpdate(PHYSICS_STEP)
        accumulator -= PHYSICS_STEP
    }

    // 補間係数（現在フレームの進行度）
    float alpha = accumulator / PHYSICS_STEP

    // 描画（補間あり）
    renderingSystem.Render(alpha)
}
```

### 6.2 GameSystemPriority

| 優先度 | システム | 実行タイミング |
|--------|---------|--------------|
| 100 | InputSystem | 毎フレーム（最初） |
| 200 | PhysicsSystem | 固定タイムステップ |
| 300 | WindowSystem | 固定タイムステップ後 |
| 400 | RenderingSystem | 毎フレーム（最後） |

### 6.3 IGameSystem

```csharp
public interface IGameSystem
{
    string Name { get; }
    int Priority { get; }
    bool IsEnabled { get; set; }

    void Initialize();
    void Update(float deltaTime);       // 毎フレーム呼び出し
    void FixedUpdate(float fixedDelta); // 固定タイムステップ呼び出し（物理用）
    void Shutdown();
}
```

### 6.4 IMainGame

```csharp
public interface IMainGame
{
    GameState State { get; }
    bool IsDebugMode { get; }

    void Run();         // ゲームループ開始（ブロッキング）
    void Pause();
    void Resume();
    void Quit();
    void ToggleDebug();
}

public enum GameState { Title, Playing, Paused, StageClear, GameClear }
```

### 6.5 イベントバス（改善点：オブザーバーパターンを統一）

現行: オブザーバーパターンが各所でバラバラ  
**新設計: 型安全なイベントバスに統一**

```csharp
public interface IEventBus
{
    void Subscribe<TEvent>(Action<TEvent> handler) where TEvent : IGameEvent;
    void Unsubscribe<TEvent>(Action<TEvent> handler) where TEvent : IGameEvent;
    void Publish<TEvent>(TEvent e) where TEvent : IGameEvent;
}

// イベント定義例
public record WindowMovedEvent(WindowId Id, Vec2i Delta) : IGameEvent;
public record WindowResizedEvent(WindowId Id, Vec2i OldLogicalSize, Vec2i NewLogicalSize) : IGameEvent;
public record WindowDeletedEvent(WindowId Id) : IGameEvent;
public record WindowMinimizedEvent(WindowId Id) : IGameEvent;
public record WindowRestoredEvent(WindowId Id) : IGameEvent;
public record WindowFlippedEvent(WindowId Id, bool FlipXChanged, bool FlipYChanged) : IGameEvent;
public record WindowGravityChangedEvent(WindowId Id) : IGameEvent;  // FlipY変化時に発行
public record WindowAppearanceChangedEvent(WindowId Id, WindowAppearance NewAppearance) : IGameEvent;
public record PlayerParentChangedEvent(GameWindow? OldParent, GameWindow? NewParent) : IGameEvent;
public record StageStartedEvent(StageIndex Stage) : IGameEvent;
public record GoalReachedEvent(StageIndex Stage) : IGameEvent;
public record SettingsChangedEvent(string PropertyName, object OldValue, object NewValue) : IGameEvent;
```

---

## 7. 入力システム

### 7.1 プラットフォーム抽象

```csharp
// プラットフォーム依存層（SDL2実装など）
public interface IInputProvider
{
    void Update();                          // SDL2イベントポーリング
    bool IsKeyDown(GameKey key);
    bool IsKeyPressed(GameKey key);         // このフレームで押された
    bool IsKeyReleased(GameKey key);        // このフレームで離された
    bool IsMouseButtonDown(MouseButton btn);
    Vec2i GetMousePosition();               // スクリーン座標
    Vec2i GetMouseDelta();                  // 前フレームからの差分
}

// ゲームロジック層（プラットフォーム非依存）
public interface IInputService
{
    bool IsKeyDown(GameKey key);
    bool IsKeyPressed(GameKey key);
    bool IsKeyReleased(GameKey key);
    bool IsMouseButtonDown(MouseButton btn);
    Vec2i GetMousePosition();
    Vec2i GetMouseDelta();
}
```

### 7.2 GameKey列挙型

```csharp
public enum GameKey
{
    // 移動
    Left, Right, Up, Down,
    A, D, W, S, Space,

    // システム
    F1, F3, F11,
    LeftShift, Escape,

    // ウィンドウ操作
    Delete,
}

public enum MouseButton { Left, Right, Middle }
```

---

## 8. ウィンドウシステム

### 8.1 プラットフォーム抽象ウィンドウ

WinFormsのFormに依存しない、OS非依存のウィンドウインターフェース。

```csharp
// プラットフォーム層（SDL2でウィンドウを作成・管理）
public interface IPlatformWindow : IDisposable
{
    WindowId Id { get; }

    Vec2i Position { get; set; }
    Vec2i Size     { get; set; }
    bool  Visible  { get; set; }
    bool  IsMinimized { get; }

    void BringToFront();
    void Minimize();
    void Restore();

    // 描画面へのアクセス（SkiaSharpがここに描画）
    nint GetSurfaceHandle();

    // 入力イベント
    event Action<Vec2i>  MouseMoved;
    event Action<Vec2i>  MouseButtonDown;
    event Action<Vec2i>  MouseButtonUp;
    event Action<GameKey> KeyDown;
}

// プラットフォームウィンドウシステム
public interface IPlatformWindowSystem : IDisposable
{
    IPlatformWindow CreateWindow(Vec2i position, Vec2i size, string? title = null);
    void DestroyWindow(WindowId id);
    IReadOnlyList<IPlatformWindow> GetAllWindows();
    void PumpEvents();  // SDL2イベントを処理
    int GetNativeZIndex(WindowId id);  // OSのZ-order
    void SetNativeZOrder(WindowId id, WindowId insertAfter);  // Z-order変更
}
```

### 8.2 GameWindow（ゲームロジック層）

```csharp
public class GameWindow
{
    public WindowId Id { get; }
    public IPlatformWindow PlatformWindow { get; }
    public IWindowStrategy Strategy { get; }
    public WindowType Type { get; }

    // 外観カスタマイズ（ゲームが描画するタイトルバー・背景・枠線の設定）
    public WindowAppearance Appearance { get; set; }

    // 変換情報（制限なしリサイズウィンドウのみ使用、それ以外はデフォルト値）
    public WindowTransform Transform { get; internal set; }

    // 境界（改善：2種類に整理）
    // CollisionBounds: タイトルバーを含む外形全体（衝突判定用）
    // VisualBounds:    クライアント領域のみ（描画・親子判定用）
    public Rect CollisionBounds { get; }  // = Transform.CollisionRect
    public Rect VisualBounds    { get; }  // = Transform.ClientRect(TitleBarHeight)

    // 反転状態（UnconstrainedResizeウィンドウのみ true になる）
    public bool FlipX => Transform.FlipX;
    public bool FlipY => Transform.FlipY;

    // 親子関係
    public GameWindow?               Parent   { get; internal set; }
    public IReadOnlyList<GameWindow> Children { get; }

    // 状態
    public bool IsMinimized   { get; }
    public bool IsNoEntry     { get; }  // Strategy.IsNoEntry のキャッシュ
    public bool IsVisible     { get; }

    // タイトルバー高さ（全ウィンドウで共通: 30px）
    public const int TitleBarHeight = 30;

    // イベント（EventBus経由で発行）
    internal void NotifyMoved(Vec2i delta) { /* IEventBus.Publish */ }
    internal void NotifyResized(Vec2i logicalOldSize, Vec2i logicalNewSize) { /* ... */ }
    internal void NotifyFlipped(bool flipX, bool flipY) { /* ... */ }
}
```

### 8.3 境界の定義

すべてのゲームウィンドウは **SDL2ボーダーレスウィンドウ** として作成され、  
タイトルバー・枠線・ボタンはすべてゲームが描画する（OSのウィンドウ装飾は使用しない）。

**通常ウィンドウ（FlipY=false）:**
```
╔══════════════════════════╗  ← CollisionBounds.Top = VisualTopLeft.Y
║ [Title]          [－][×]║  ← ゲーム描画のタイトルバー（30px）
╠══════════════════════════╣  ← VisualBounds.Top（クライアント領域開始）
║                          ║
║    クライアント領域       ║
║                          ║
╚══════════════════════════╝  ← CollisionBounds.Bottom = VisualBounds.Bottom

CollisionBounds = Transform.CollisionRect
VisualBounds    = Transform.ClientRect(TitleBarHeight=30)
```

**FlipYウィンドウ（Height < 0）:**
```
╔══════════════════════════╗  ← CollisionBounds.Top = VisualTopLeft.Y
║                          ║
║  クライアント領域（上下反転）║
║                          ║
╠══════════════════════════╣  ← タイトルバーが下端へ
║ [×][－]          [eltit]║  ← ボタン左下・テキスト上下反転
╚══════════════════════════╝  ← CollisionBounds.Bottom

CollisionBounds = Transform.CollisionRect（FlipY考慮済みの正の矩形）
VisualBounds    = Transform.ClientRect(TitleBarHeight=30)
```

**用途の使い分け：**
- 衝突判定、接地判定 → CollisionBounds（常に正の矩形）
- 親子関係の包含判定 → VisualBounds
- 親ウィンドウ内の移動可能領域 → VisualBounds（タイトルバーを除く）
- コンテンツの反転描画 → Transform.GetContentTransform() で取得した行列を適用

### 8.4 WindowType と Strategy

```csharp
public enum WindowType
{
    NormalBlack,            // 静的プラットフォーム（黒背景）
    NormalWhite,            // 静的プラットフォーム（白背景）
    Resizable,              // マウスドラッグでリサイズ可能（MIN/MAX制限あり）
    Movable,                // マウスドラッグで移動可能
    Deletable,              // Deleteキーで削除
    Minimizable,            // クリックで最小化/復元
    TextDisplay,            // テキスト表示（静的）

    // UnconstrainedResize系（サイズ制限なし、マイナスサイズで反転）
    UnconstrainedResize,        // 制限なしリサイズ（マイナスサイズ可能、反転あり）
    UnconstrainedResizeNoEntry, // 制限なしリサイズ + 不可侵

    // NoEntry系（外周5pxが不可侵境界となる）
    NoEntry,                // 静的不可侵ウィンドウ
    ResizableNoEntry,       // リサイズ可能 + 不可侵
    MovableNoEntry,         // 移動可能 + 不可侵
    MinimizableNoEntry,     // 最小化可能 + 不可侵
    NormalBlackNoEntry,     // 静的（黒）+ 不可侵
    NormalWhiteNoEntry,     // 静的（白）+ 不可侵
}
```

```csharp
public interface IWindowStrategy
{
    bool IsNoEntry { get; }

    // 外観カスタマイズ（Strategyごとにデフォルト値を持つ）
    // ステージJSONの "appearance" フィールドで上書き可能
    WindowAppearance DefaultAppearance { get; }

    // 入力処理
    void OnMouseDown(GameWindow window, Vec2i localPos);
    void OnMouseUp(GameWindow window, Vec2i localPos);
    void OnMouseMove(GameWindow window, Vec2i localPos, Vec2i delta);
    void OnKeyDown(GameWindow window, GameKey key);

    // 更新（毎フレーム）
    void Update(GameWindow window, float deltaTime);

    // 描画（StrategyマークのみI。タイトルバー・背景はRenderingSystemが描画）
    void DrawStrategyMark(IRenderer renderer, Rect clientBounds, bool isHovered);
    CursorType GetCursor(Vec2i localPos);
}
```

### 8.5 Strategy詳細仕様

#### NormalWindowStrategy
- 入力: すべて無視
- 更新: なし
- マーク: なし、カーソル: Default

#### ResizableWindowStrategy

```
状態:
  isResizing: bool
  dragStart: Vec2i       // ドラッグ開始時のマウス位置
  originalSize: Vec2i    // ドラッグ開始時のウィンドウサイズ

OnMouseDown:
  isResizing = true
  dragStart = mousePos（スクリーン座標）
  originalSize = window.VisualBounds.Size
  マウスキャプチャ開始

OnMouseMove (isResizing時):
  delta = mousePos - dragStart
  proposedSize = clamp(originalSize + delta, MIN_SIZE, MAX_SIZE)
  validSize = collisionService.ValidateSize(window, proposedSize)
  window.Size = validSize
  子要素をスケーリング（比率維持）
  EventBus.Publish(WindowResizedEvent)

OnMouseUp:
  isResizing = false
  マウスキャプチャ解除

子要素スケーリング（ResizableWindowStrategy）:
  // Resizable はサイズが常に正なので、FlipX/FlipY を考慮しない簡略版を使用
  // ただし位置もスケーリングする（旧実装はサイズのみだったが、新実装では位置も変更）
  oldClientRect = 変更前の VisualBounds（タイトルバー除くクライアント領域）
  newClientRect = 変更後の VisualBounds
  scale = newClientRect.Size / oldClientRect.Size  // 常に正

  foreach child in window.Children (再帰なし):
    // クライアント領域左上からの相対位置（Resizable は FlipX/FlipY なし）
    relPos  = child.ScreenPos - oldClientRect.TopLeft
    newPos  = relPos  * scale
    newSize = child.Size * scale
    child.ScreenPos = newClientRect.TopLeft + newPos
    child.Size      = newSize
    // GameWindow の場合はさらに孫へ再帰
```

> **現行実装との差分:** 旧 WinForms 実装では子のサイズのみスケーリングし、位置は固定だった。  
> 新設計では位置もスケーリングすることで、親の内側に収まることを保証する。

**パフォーマンス最適化:**
- 変化量2px未満は早期リターン（細かすぎる更新を省略）
- サイズ変化なしは即リターン
- 循環更新防止: `HashSet<WindowId>` で処理中の子をスキップ

**制限:**
- MIN_SIZE = (100, 100)
- MAX_SIZE = 設定値（デフォルト800×600）

#### MovableWindowStrategy

```
状態:
  isDragging: bool
  lastMousePos: Vec2i
  isBlockedLeft/Right/Up/Down: bool  // 方向別衝突フラグ

OnMouseDown:
  isDragging = true
  lastMousePos = mousePos

OnMouseMove (isDragging時):
  delta = mousePos - lastMousePos
  proposedBounds = window.CollisionBounds + delta
  validBounds = collisionService.ValidatePosition(window, proposedBounds)
  adjustedDelta = validBounds.Location - window.CollisionBounds.Location
  window.Position += adjustedDelta
  子要素も同量移動
  EventBus.Publish(WindowMovedEvent)
  UpdateBlockFlags()  // 衝突方向を記録

衝突フラグ更新:
  各方向に1px移動した仮想位置が衝突するかをチェック
  衝突する方向 → isBlocked* = true
  ブロック解除: 反対方向への移動を検出した時

OnMouseUp:
  isDragging = false
  全isBlockedフラグ = false
```

#### DeletableWindowStrategy

```
OnKeyDown (Delete):
  すべての子孫を列挙
  各子孫を windowManager から削除
  自身を windowManager から削除
  PlatformWindow.Destroy()
  EventBus.Publish(WindowDeletedEvent)
```

#### MinimizableWindowStrategy

```
OnMouseDown:
  window.Minimize() または Restore()

最小化処理:
  子ウィンドウを一時的に非表示（Visible=false）
  不可侵領域を削除（IsNoEntry=trueの場合）
  PlatformWindow.Minimize()
  EventBus.Publish(WindowMinimizedEvent)

復元処理:
  PlatformWindow.Restore()
  子ウィンドウを再表示
  不可侵領域を再生成（IsNoEntry=trueの場合）
  親子関係を再判定
  EventBus.Publish(WindowRestoredEvent)
```

#### TextDisplayWindowStrategy

```
コンストラクタ:
  text: string  // 表示テキスト

DrawStrategyMark（テキスト描画）:
  中央にテキストを描画（PressStartフォント、白色）
```

#### NoEntryWindowStrategy

```
IsNoEntry = true
入力: すべて無視（静的）
カーソル: 禁止カーソル (Cursors.No相当)

初期化時:
  noEntryZoneManager.RegisterNoEntryWindow(window)

外周境界（不可侵領域）:
  上辺: Rect(X,   Y,            Width, 5)
  下辺: Rect(X,   Bottom-5,     Width, 5)
  左辺: Rect(X,   Y,            5,     Height)
  右辺: Rect(Right-5, Y,        5,     Height)
```

#### 複合NoEntry系Strategy

ResizableNoEntry / MovableNoEntry / MinimizableNoEntry:
- 対応する基本Strategyを内部に持つ（継承ではなく合成で実装）
- `IsNoEntry = true`
- 基本Strategyの全処理に加え、NoEntryZoneManagerの更新を行う

```csharp
public sealed class ResizableNoEntryStrategy(
    ResizableWindowStrategy inner,
    INoEntryZoneManager noEntryZoneManager) : IWindowStrategy
{
    public bool IsNoEntry => true;

    public void OnMouseMove(GameWindow w, Vec2i pos, Vec2i delta)
    {
        inner.OnMouseMove(w, pos, delta);
        noEntryZoneManager.UpdateBoundsForWindow(w);  // リサイズ後に境界更新
    }
    // 他のメソッドも inner に委譲
}
```

#### UnconstrainedResizeWindowStrategy

サイズ制限なし（最小・最大なし）でリサイズ可能。マイナスサイズになると反転が発生する。

```
状態:
  isResizing: bool
  dragStart: Vec2i        // ドラッグ開始時のマウス位置（スクリーン座標）
  originalLogicalSize: Vec2i  // ドラッグ開始時の論理サイズ（負値あり）

OnMouseDown:
  isResizing = true
  dragStart = mousePos
  originalLogicalSize = window.Transform.LogicalSize

OnMouseMove (isResizing時):
  delta = mousePos - dragStart
  newLogical = originalLogicalSize + delta  // 制限なし（負値許容）
  
  if newLogical == window.Transform.LogicalSize: return  // 変化なし
  
  // 変更前の状態を保存（子の更新に使用）
  oldFlipX = window.Transform.FlipX
  oldFlipY = window.Transform.FlipY
  oldClientRect = window.Transform.ClientRect(TitleBarHeight)
  
  // Transformを更新
  window.Transform = window.Transform with { LogicalSize = newLogical }
  newClientRect = window.Transform.ClientRect(TitleBarHeight)
  
  // SDL2ウィンドウの実際の位置・サイズを更新
  window.PlatformWindow.Position = window.Transform.VisualTopLeft
  window.PlatformWindow.Size = window.Transform.AbsoluteSize
  
  // 子要素の位置・サイズをコンテンツ空間基準で更新（スケール＋反転を一括処理）
  UpdateChildrenInContentSpace(window, oldClientRect, newClientRect,
                               window.Transform.FlipX, window.Transform.FlipY)
  
  // 反転状態が変わったイベントを発行
  if window.Transform.FlipX != oldFlipX || window.Transform.FlipY != oldFlipY:
      bool flipXChanged = window.Transform.FlipX != oldFlipX
      bool flipYChanged = window.Transform.FlipY != oldFlipY
      EventBus.Publish(WindowFlippedEvent(window.Id, flipXChanged, flipYChanged))
      if flipYChanged:
          EventBus.Publish(WindowGravityChangedEvent(window.Id))
  
  EventBus.Publish(WindowResizedEvent(window.Id, originalLogicalSize, newLogical))

OnMouseUp:
  isResizing = false
```

**UpdateChildrenInContentSpace（子要素の一括更新）:**

コンテンツ空間（Content Space）を介した統一処理。  
スケーリングと反転を別々に処理せず、一つの変換パイプラインで扱うことで整合性を保証する。

```
コンテンツ空間の定義:
  原点 = 親の ClientRect 左上（タイトルバーを除いたクライアント領域の左上）
  X 軸 = 右向き（常に正、FlipX の影響を受けない）
  Y 軸 = 下向き（常に正、FlipY の影響を受けない）
  → 子が親の内側にある限り、contentPos は常に 0〜clientSize の正の値

ContentFromScreen(clientRect, flipX, flipY, childScreenPos, childSize):
  // スクリーン座標 → コンテンツ空間座標
  if flipX:
    contentX = clientRect.Right - (childScreenPos.X + childSize.X)
  else:
    contentX = childScreenPos.X - clientRect.Left
  if flipY:
    contentY = clientRect.Bottom - (childScreenPos.Y + childSize.Y)
  else:
    contentY = childScreenPos.Y - clientRect.Top
  return (contentX, contentY)

ScreenFromContent(clientRect, flipX, flipY, contentPos, contentSize):
  // コンテンツ空間座標 → スクリーン座標
  if flipX:
    screenX = clientRect.Right - (contentPos.X + contentSize.X)
  else:
    screenX = clientRect.Left + contentPos.X
  if flipY:
    screenY = clientRect.Bottom - (contentPos.Y + contentSize.Y)
  else:
    screenY = clientRect.Top + contentPos.Y
  return (screenX, screenY)

UpdateChildrenInContentSpace(window, oldClientRect, newClientRect, newFlipX, newFlipY):
  // 旧クライアントサイズが 0 の場合はスケール不可能なのでスキップ
  if oldClientRect.Width == 0 || oldClientRect.Height == 0: return

  scale = Vec2f(
    (float)newClientRect.Width  / oldClientRect.Width,   // 常に正
    (float)newClientRect.Height / oldClientRect.Height   // 常に正
  )

  foreach child in window.Children (再帰なし、直接の子のみ):
    // ① 子の現在位置を旧コンテンツ空間に変換
    //    （旧 FlipX/FlipY は window.Transform 更新前に保存済み）
    contentPos  = ContentFromScreen(oldClientRect, oldFlipX, oldFlipY,
                                    child.ScreenPos, child.Size)
    contentSize = child.Size

    // ② コンテンツ空間内でスケーリング（絶対サイズの比率、常に正のスケール）
    newContentPos  = Vec2i(contentPos.X  * scale.X, contentPos.Y  * scale.Y)
    newContentSize = Vec2i(contentSize.X * scale.X, contentSize.Y * scale.Y)
    newContentSize = Vec2i(max(1, newContentSize.X), max(1, newContentSize.Y))  // 最小1px

    // ③ 新コンテンツ空間からスクリーン座標に変換
    //    （新しい FlipX/FlipY を使用 → 反転が変わった場合は自動的に位置が鏡映される）
    newScreenPos = ScreenFromContent(newClientRect, newFlipX, newFlipY,
                                     newContentPos, newContentSize)

    child.ScreenPos = newScreenPos
    child.Size      = newContentSize

    // 子が GameWindow の場合は SDL2 ウィンドウも更新
    if child is GameWindow childWindow:
      childWindow.PlatformWindow.Position = newScreenPos
      childWindow.PlatformWindow.Size     = newContentSize
      // 子も同じアルゴリズムで孫を更新（再帰）
      UpdateChildrenInContentSpace(childWindow, childWindow.oldClientRect, ...)
```

**具体例（FlipX 発生ケース）:**

```
初期状態:
  Anchor=(100,100), LogicalSize=(200,150), TitleBarH=30
  ClientRect = (X=100, Y=130, W=200, H=120)
  FlipX=false, FlipY=false

  子ウィンドウ A: ScreenPos=(150,150), Size=(80,40)
    contentPos = (150-100, 150-130) = (50, 20)  // 正の値

  子ウィンドウ B: ScreenPos=(240,170), Size=(50,30)
    contentPos = (240-100, 170-130) = (140, 40)  // 正の値（右側にある）

ドラッグ → LogicalSize=(-50,150) [FlipX 発生]:
  VisualTopLeft = (50, 100)
  ClientRect_new = (X=50, Y=130, W=50, H=120)
  newFlipX=true, newFlipY=false

  scale = (50/200, 120/120) = (0.25, 1.0)

  子 A の更新:
    contentPos = (50, 20)
    newContentPos  = (50*0.25, 20*1.0) = (12.5, 20)
    newContentSize = (80*0.25, 40*1.0) = (20, 40)
    // FlipX=true なので screenX = ClientRect.Right - (12.5+20) = 100 - 32.5 = 67
    newScreenPos = (67, 150)
    → 視覚範囲 X=50〜100 内 ✓

  子 B の更新:
    contentPos = (140, 40)
    newContentPos  = (140*0.25, 40*1.0) = (35, 40)
    newContentSize = (50*0.25, 30*1.0) = (12, 30)
    // FlipX=true なので screenX = 100 - (35+12) = 53
    newScreenPos = (53, 170)
    → B は元々右側にあったが、FlipX後は左寄りの位置に ✓
      （コンテンツ空間では同じ相対位置 → スクリーンでは鏡映） ✓
```

**UnconstrainedResizeNoEntryStrategy:**
- UnconstrainedResizeWindowStrategy を内部に持つ合成パターン
- `IsNoEntry = true`
- OnMouseMove後に `noEntryZoneManager.UpdateBoundsForWindow(window)` を追加呼び出し

### 8.6 Z-order管理

#### ZOrderManager

```csharp
public interface IZOrderManager
{
    void Register(GameWindow window);
    void Unregister(GameWindow window);

    // ウィンドウとその全子孫を最前面に移動
    void BringToFront(GameWindow window);

    int GetZIndex(GameWindow window);  // 0=最背面、Count-1=最前面
    bool IsInFront(GameWindow a, GameWindow b);  // aがbより前面か

    IReadOnlyList<GameWindow> GetOrderedWindows();  // 背面→前面順

    // Win32/SDL2のネイティブZ-orderと同期
    void SyncNativeZOrder();
}
```

**BringToFront の実装:**
```
1. window と全子孫をグループ化
2. 内部リストから削除
3. リスト末尾に追加（末尾 = 最前面）
4. SyncNativeZOrder() でOS側のZ-orderと同期
```

**重要:** 内部リストとOSのZ-orderを常に一致させる。  
これにより衝突判定（Z-order考慮の可視性判定）が正確に動作する。

### 8.7 親子関係管理

#### WindowHierarchyManager

```csharp
public interface IWindowHierarchyManager
{
    void CheckAndUpdateHierarchy(GameWindow movedWindow, IReadOnlyList<GameWindow> allWindows);
    void DissolveRelationship(GameWindow window);
    GameWindow? GetParent(GameWindow window);
    IReadOnlyList<GameWindow> GetAllDescendants(GameWindow window);
}
```

**親子関係確立アルゴリズム（CheckAndUpdateHierarchy）:**

```
1. movedWindowの現在の親子関係を検証
   - 親がいる場合: VisualBoundsで完全包含チェック
     → 含まれていない: 親子関係を解除
   - 子がいる場合: 各子について同様にチェック
     → 含まれていない: 子を切り離し

2. 新しい親候補を探す
   - 条件: movedWindowより背面、かつmovedWindowを完全に含む
   - ZOrder降順でスキャン（前面に近い方を優先）
   - 最初に見つかった候補を新しい親に設定

3. 新しい子候補を探す
   - 条件: movedWindowより前面、かつmovedWindowに完全に含まれる
   - ZOrder昇順でスキャン

親子関係の効果:
- 親が移動 → 子も同量移動
- 親がリサイズ → 子もスケーリング
- 親が最小化 → 子も非表示
```

**完全包含判定（VisualBoundsベース）:**
```
Contains(container, contained):
  contained.Left   >= container.Left   &&
  contained.Top    >= container.Top    &&
  contained.Right  <= container.Right  &&
  contained.Bottom <= container.Bottom
```

### 8.8 NoEntry境界システム

#### INoEntryZoneManager

```csharp
public interface INoEntryZoneManager
{
    // 静的ゾーン（ステージに直接配置される矩形領域）
    void AddStaticZone(Rect bounds);
    void RemoveStaticZone(Rect bounds);
    void ClearStaticZones();

    // 不可侵ウィンドウ（外周5pxが境界となる）
    void RegisterNoEntryWindow(GameWindow window);
    void UnregisterNoEntryWindow(GameWindow window);
    void UpdateBoundsForWindow(GameWindow window);  // 移動・リサイズ後に呼ぶ

    // 衝突チェック
    bool IntersectsAny(Rect bounds, GameWindow? exclude = null);
    Rect ValidatePosition(Rect current, Rect proposed, GameWindow? exclude = null);
    Vec2i ValidateSize(Rect current, Vec2i proposedSize, GameWindow? exclude = null);

    // 描画用
    IReadOnlyList<Rect> GetStaticZoneBounds();
    IReadOnlyList<(GameWindow Window, Rect[] Boundaries)> GetWindowBoundaries();
}
```

**境界矩形の取得（GetWindowBoundaries）:**
```
各不可侵ウィンドウについて4辺の境界Rectを返す:
  bounds = window.CollisionBounds
  [
    (bounds.X, bounds.Y,        bounds.Width, 5),   // 上辺
    (bounds.X, bounds.Bottom-5, bounds.Width, 5),   // 下辺
    (bounds.X, bounds.Y,        5, bounds.Height),  // 左辺
    (bounds.Right-5, bounds.Y,  5, bounds.Height),  // 右辺
  ]
```

**Z-order対応の可視性判定:**
```
IsBoundaryVisible(window, boundaryRect, checkBounds):
  1. checkBoundsとboundaryRectの交差部分を計算
  2. 交差領域をRegionとして作成
  3. windowより前面の不可侵ウィンドウが交差領域を隠しているか
     → Region.Exclude()で除外
  4. Regionが空でなければ可視 → 衝突判定有効
```

**GetValidPosition（X/Y軸独立検証）:**
```
[X軸検証]
xMovement = Rect(proposed.X, current.Y, current.Width, current.Height)
各NoEntry対象と交差チェック
最小移動距離の調整位置を bestAdjustedX に記録

[Y軸検証]
yMovement = Rect(bestAdjustedX ?? proposed.X, proposed.Y, width, current.Height)
同様に bestAdjustedY を計算

返す位置: (bestAdjustedX ?? proposed.X, bestAdjustedY ?? proposed.Y)
```

### 8.9 カスタムウィンドウクローム

**設計方針:** すべてのゲームウィンドウは **SDL2ボーダーレスウィンドウ** として生成される。  
OS標準のタイトルバー・枠線・ボタンは一切使用せず、ゲームが全体を描画する。  
これにより FlipY 時のタイトルバー下移動が可能になる。

#### クローム構成要素

```
通常状態（FlipY=false）:
[タイトルバー 30px]
  ├─ タイトルテキスト（中央配置、WindowAppearance.TitleText）
  └─ ボタン（右側）
       ├─ 最小化ボタン "－"（Minimizable/MinimizableNoEntryのみ表示）
       └─ 閉じるボタン "×"（将来拡張用、現行は非表示）

FlipY 状態（Height < 0）:
  ├─ クライアント領域（上下反転コンテンツ）
[タイトルバー 30px] ← 下端
  ├─ タイトルテキスト（上下反転して描画）
  └─ ボタン（左側 ← FlipYなら左右も入れ替わる）
```

#### ヒットテスト（マウス→ウィンドウ操作の判定）

```
マウス座標 → ウィンドウのどの部分か:

1. titleBarRect = Transform.TitleBarRect(TitleBarHeight)
   → タイトルバー内: ドラッグ移動の起点（Movableの場合）
   → ボタン領域内: ボタン処理

2. clientRect = Transform.ClientRect(TitleBarHeight)
   → クライアント内: Strategy.OnMouseDown / OnMouseMove

ローカル座標変換（FlipX/FlipYを考慮）:
  screenPos → ウィンドウ相対座標 → FlipX/FlipYの逆変換 → 論理クライアント座標
  （マウス座標も反転行列で変換してからStrategyに渡す）
```

#### SDL2ウィンドウ設定

```csharp
// 全ゲームウィンドウ共通の生成設定
IPlatformWindow CreateGameWindow(Vec2i position, Vec2i size)
{
    return platformWindowSystem.CreateWindow(position, size, title: null,
        flags: BorderlessFlag | NoDecorationsFlag);
}
```

### 8.10 ウィンドウ外観カスタマイズ（WindowAppearance）

各ウィンドウの見た目を自由にカスタマイズできる設定レコード。  
Strategyはデフォルト値を `DefaultAppearance` として定義し、ステージJSONで個別上書き可能。

```csharp
public sealed record WindowAppearance
{
    // 背景
    public GameColor BackgroundColor { get; init; } = new(30, 30, 30);

    // タイトルバー
    public GameColor TitleBarColor   { get; init; } = new(50, 50, 50);
    public GameColor TitleTextColor  { get; init; } = GameColor.White;
    public string    TitleText       { get; init; } = "";
    public bool      ShowTitleBar    { get; init; } = true;

    // 枠線
    public GameColor BorderColor     { get; init; } = new(100, 100, 100);
    public int       BorderWidth     { get; init; } = 1;
    public bool      ShowBorder      { get; init; } = true;

    // ボタン（クローム内のミニマイズ等）
    public GameColor ButtonBackground { get; init; } = new(80, 80, 80);
    public GameColor ButtonHover      { get; init; } = new(120, 120, 120);
    public GameColor ButtonText       { get; init; } = GameColor.White;
}
```

**各 Strategy のデフォルト Appearance:**

| WindowType | BackgroundColor | TitleBarColor | 補足 |
|------------|----------------|--------------|------|
| NormalBlack | Black (0,0,0) | (50,50,50) | |
| NormalWhite | White (255,255,255) | (200,200,200) | TitleTextColor=Black |
| Resizable | (30,30,30) | (50,50,50) | StrategyMark: リサイズ矢印 |
| Movable | (30,30,30) | (50,50,50) | StrategyMark: 移動矢印 |
| Deletable | (30,30,30) | (50,50,50) | StrategyMark: Delete表示 |
| Minimizable | (30,30,30) | (50,50,50) | StrategyMark: 最小化アイコン |
| TextDisplay | (30,30,30) | (50,50,50) | 中央にTitleTextを大きく描画 |
| UnconstrainedResize | (20,20,20) | (60,40,40) | StrategyMark: 双方向矢印 |
| NoEntry | DimGray (105,105,105) | (80,80,80) | BorderColor=Red |
| ResizableNoEntry | (40,80,40) | (40,60,40) | BorderColor=Red |
| MovableNoEntry | (40,40,80) | (40,40,60) | BorderColor=Red |
| MinimizableNoEntry | (80,40,40) | (60,40,40) | BorderColor=Red |

**ステージJSONでの上書き例:**
```json
{
  "type": "NormalBlack",
  "position": { "x": 100, "y": 200 },
  "size": { "w": 300, "h": 100 },
  "appearance": {
    "backgroundColor": "#1A3A5C",
    "titleBarColor":   "#0D2040",
    "titleText":       "Platform A",
    "borderColor":     "#4080C0",
    "borderWidth":     2
  }
}
```

**色の JSON フォーマット:** `"#RRGGBB"` または `"#RRGGBBAA"` の16進数文字列。

### 8.11 ウィンドウ変換・反転システム（WindowTransform）

セクション 4.3 で定義した `WindowTransform` のウィンドウシステムにおける扱い方。

#### 通常ウィンドウの Transform

通常の固定サイズ・移動ウィンドウでは `WindowTransform` はシンプルな形式を持つ:
```csharp
// 通常ウィンドウ: LogicalSize は常に正
Transform = new WindowTransform
{
    Anchor = position,
    LogicalSize = size  // width > 0, height > 0
}
// FlipX = false, FlipY = false
// CollisionBounds = Rect(position, size)
// VisualBounds = Rect(position.X, position.Y + 30, size.X, size.Y - 30)
```

#### UnconstrainedResize 専用の動作

```
リサイズ中の状態変化例（右端を左へドラッグしてFlipX発生）:

初期: Anchor=(100,100), LogicalSize=(200, 150)
      → VisualTopLeft=(100,100), Size=(200,150), FlipX=false

ドラッグ -50px: LogicalSize=(150, 150), FlipX=false → サイズ縮小
ドラッグ -200px: LogicalSize=(0, 150) → FlipX=false（幅0 = 線）
ドラッグ -250px: LogicalSize=(-50, 150)
      → FlipX=true
      → VisualTopLeft=(100 + (-50), 100) = (50, 100)
      → AbsoluteSize=(50, 150)
      → SDL2ウィンドウを (50,100) に移動し、サイズ (50,150) に変更
      → コンテンツ描画時に水平反転変換を適用
```

#### イベント定義

```csharp
public record WindowFlippedEvent(WindowId Id, bool FlipXChanged, bool FlipYChanged) : IGameEvent;
public record WindowGravityChangedEvent(WindowId Id) : IGameEvent;
```

### 8.12 座標変換パイプライン

反転ウィンドウ内でのコンテンツ描画と入力処理のための変換パイプライン。

#### 描画変換（SkiaSharp）

```csharp
void DrawWindowContent(GameWindow window, IRenderer renderer)
{
    var (scale, translate) = window.Transform.GetContentTransform(GameWindow.TitleBarHeight);
    var clientOrigin = window.Transform.ClientRect(GameWindow.TitleBarHeight).Location;

    renderer.PushTransform(scale, clientOrigin + translate);
    // 以降の描画はすべてクライアント座標系で行う
    // FlipX の場合 X 軸が反転、FlipY の場合 Y 軸が反転
    DrawWindowChildren(window, renderer);
    renderer.PopTransform();
}
```

#### マウス座標の逆変換

StrategyがOnMouseDownを受け取る際、マウス座標をクライアント論理座標に変換する:

```csharp
Vec2i ScreenToClientLogical(GameWindow window, Vec2i screenPos)
{
    var client = window.Transform.ClientRect(GameWindow.TitleBarHeight);
    // クライアント相対座標
    int rx = screenPos.X - client.X;
    int ry = screenPos.Y - client.Y;
    // FlipX/FlipY の逆変換
    if (window.Transform.FlipX) rx = client.Width  - rx;
    if (window.Transform.FlipY) ry = client.Height - ry;
    return new Vec2i(rx, ry);
}
```

#### 子ウィンドウの座標系

子ウィンドウ（`GameWindow`）は実際の SDL2 ウィンドウであり、常にスクリーン座標で位置を持つ。  
FlipX/FlipY の変換行列はゲームが Canvas に描画する内容（StrategyMark等）にのみ適用される。

**子ウィンドウの位置は `UpdateChildrenInContentSpace` によって管理される（セクション8.5参照）:**
- 親がリサイズまたは反転するたびに、子のスクリーン座標をコンテンツ空間経由で再計算する
- FlipX/FlipY が変化しても、コンテンツ空間内での子の相対位置は変わらず、スクリーン座標だけが変わる
- これにより、子ウィンドウは常に親のクライアント領域の内側にとどまることが保証される

```
重要: 子 GameWindow はコンテンツ空間内で管理されているが、
      スクリーン座標を持つ実ウィンドウである。
      → Canvas の変換行列（PushTransform）は子 GameWindow には適用しない
      → 子 GameWindow は自身のSDL2ウィンドウとして独立して描画する
```

**プレイヤーの扱い:**  
プレイヤーは独立したSDL2ウィンドウを持たない仮想エンティティなので、  
描画は親ウィンドウの Canvas 上に行い、FlipX/FlipY の変換行列を適用する。

---

## 9. 物理・衝突システム

### 9.1 物理定数（GameSettings.PhysicsSettings）

```csharp
public record PhysicsSettings
{
    public float Gravity         = 1000.0f;  // px/秒²（正 = 下向き）
    public float JumpForce       = 700.0f;   // px/秒（重力方向に対して逆向きの初速）
    public float MoveSpeed       = 400.0f;   // px/秒（水平移動速度）
    public int   GroundTolerance = 5;        // 接地判定の許容px
    public int   GroundCheckH    = 15;       // 接地判定エリアの高さ
    public float MaxFallSpeed    = 2000.0f;  // 最大落下速度（絶対値）
}
```

### 9.5 重力反転システム（GravityMultiplier）

プレイヤーが `FlipY=true` の `UnconstrainedResize` ウィンドウ（またはその子孫）の中にいる場合、  
重力の向きが反転する。

#### GravityMultiplier の計算

```csharp
int CountFlipYAncestors(GameWindow? parent)
{
    int count = 0;
    while (parent != null)
    {
        if (parent.FlipY) count++;
        parent = parent.Parent;
    }
    return count;
}

// GravityMultiplier = (-1)^(FlipY祖先の数)
float GravityMultiplier(GameWindow? parentChain)
    => CountFlipYAncestors(parentChain) % 2 == 0 ? 1f : -1f;
```

| FlipY祖先数 | GravityMultiplier | 重力方向 |
|-------------|-------------------|---------|
| 0（通常） | +1 | 下向き（Y増加） |
| 1（FlipYウィンドウ内） | -1 | 上向き（Y減少） |
| 2（FlipYがネスト） | +1 | 下向き（Y増加） |

#### 重力・ジャンプへの適用

```
ApplyGravity(fixedDelta, gravityMultiplier):
  effectiveGravity = Gravity * gravityMultiplier  // 正 or 負
  if !isGrounded:
    verticalVelocity += effectiveGravity * fixedDelta
    // 速度の絶対値を MaxFallSpeed で制限
    verticalVelocity = clamp(verticalVelocity, -MaxFallSpeed, +MaxFallSpeed)

Jump(gravityMultiplier):
  if isGrounded:
    // ジャンプ = 重力方向と逆向きの初速
    verticalVelocity = -JumpForce * gravityMultiplier
    isGrounded = false
```

#### 重力反転時の接地判定（CheckGrounded）

重力が反転（GravityMultiplier < 0）の場合、「地面」は上になる:

```
gravityMultiplier < 0（上向き重力）の場合:
  プレイヤーの「天井」= player.Collision.Top（Y最小値）が接触判定基準になる
  SweepBoundsも上方向に拡張:
    feet = Rect(player.X, player.Y, player.Width, GroundCheckH)  ← 上端を使用
    maxStep = max(|verticalVelocity * fixedDelta|, 20f)
    sweep = Rect(feet.X, feet.Y - maxStep - 5, feet.Width, maxStep + feet.Height + 10)

  接地判定: player.Top <= surface.Bottom && player.Top >= surface.Bottom - Tolerance
    （通常と上下逆）

gravityMultiplier >= 0（通常の下向き重力）の場合:
  従来通り player.Bottom が基準
```

#### 重力変更イベントの処理

```csharp
// IPlayerPhysics に gravityMultiplier を毎フレーム渡す
eventBus.Subscribe<WindowFlippedEvent>(e => {
    // プレイヤーの親チェーンを再計算して gravityMultiplier を更新
    physics.GravityMultiplier = ComputeGravityMultiplier(player.CurrentParent);
});
eventBus.Subscribe<PlayerParentChangedEvent>(e => {
    physics.GravityMultiplier = ComputeGravityMultiplier(player.CurrentParent);
});
```

### 9.2 PlayerPhysics

```csharp
public interface IPlayerPhysics
{
    float VerticalVelocity   { get; }
    bool  IsGrounded         { get; }
    float GravityMultiplier  { get; set; }  // +1 or -1（重力方向）

    void ApplyGravity(float fixedDelta);
    void Jump();
    void SetGrounded(bool grounded, int? groundY = null);
    Vec2f CalculateMovement(float fixedDelta);  // 水平+垂直の移動量

    // 接地コールバック（ファクトリーで設定）
    Action<int>? OnGrounded { get; set; }
}
```

**重力適用:**
```
ApplyGravity(fixedDelta):
  effectiveGravity = Gravity * GravityMultiplier  // GravityMultiplier = ±1
  if !isGrounded:
    verticalVelocity = clamp(
        verticalVelocity + effectiveGravity * fixedDelta,
        -MaxFallSpeed,
        +MaxFallSpeed
    )
  else:
    verticalVelocity = 0
```

**ジャンプ:**
```
Jump():
  if isGrounded:
    // JumpForce は常に重力の逆方向
    verticalVelocity = -JumpForce * GravityMultiplier
    isGrounded = false
```

### 9.3 接地判定（CheckGrounded）

SweepBounds方式。高速落下時の床すり抜けを防ぐ。

**SweepBounds計算（GravityMultiplier対応）:**
```
if GravityMultiplier >= 0:  // 通常（下向き重力）
  feet = Rect(
      player.Collision.X,
      player.Collision.Bottom - GroundCheckH,   // プレイヤー下端
      player.Collision.Width,
      GroundCheckH
  )
  maxStep = max(|verticalVelocity * fixedDelta|, 20f)
  sweep = Rect(
      feet.X,
      min(feet.Y, feet.Y + verticalVelocity * fixedDelta) - 5,
      feet.Width,
      maxStep + feet.Height + 10
  )

else:  // 反転（上向き重力）
  feet = Rect(
      player.Collision.X,
      player.Collision.Top,                     // プレイヤー上端
      player.Collision.Width,
      GroundCheckH
  )
  maxStep = max(|verticalVelocity * fixedDelta|, 20f)
  sweep = Rect(
      feet.X,
      min(feet.Y + verticalVelocity * fixedDelta, feet.Y) - 5,
      feet.Width,
      maxStep + feet.Height + 10
  )
```

**接地判定優先順位（6段階）:**

GravityMultiplier >= 0（通常）の接地条件 = `surface.Top` に対して `player.Bottom` が近い  
GravityMultiplier < 0（反転）の接地条件 = `surface.Bottom` に対して `player.Top` が近い

| 優先度 | 判定対象 | 通常条件 | 反転時の条件 |
|--------|---------|---------|-------------|
| 1 | 静的NoEntryZone | bottom >= zone.Top && bottom <= zone.Top + Tolerance | top <= zone.Bottom && top >= zone.Bottom - Tolerance |
| 2 | 不可侵ウィンドウ境界（Z-order対応） | 境界下辺に対して同様 | 境界上辺に対して同様 |
| 3 | ゲームUIボタン | ボタン上辺に対して同様 | ボタン下辺に対して同様 |
| 4 | デスクトップアイコン（EnableDesktopIconsのみ） | アイコン上辺（最大5個） | アイコン下辺（最大5個） |
| 5 | 通常ゲームウィンドウ | Z-order降順、前面優先 | 同様（上辺↔下辺） |
| 6 | スクリーン端 | player.Bottom >= screenHeight | player.Top <= 0 |

### 9.4 CollisionService（位置・サイズ検証）

```csharp
public interface ICollisionService
{
    // 移動後の有効位置を返す（衝突で押し戻す）
    Rect ValidatePosition(
        Rect current,
        Rect proposed,
        CollisionOptions options);

    // リサイズ後の有効サイズを返す
    Vec2i ValidateSize(
        Rect current,
        Vec2i proposedSize,
        CollisionOptions options);

    // 衝突しているか（bool）
    bool CheckCollision(Rect bounds, CollisionOptions options);
}

public sealed record CollisionOptions
{
    public GameWindow? ExcludeWindow    = null;
    public bool ExcludeChildren         = false;
    public bool CheckNoEntryZones       = true;
    public bool CheckNoEntryBoundaries  = true;
    public bool CheckNormalWindows      = false;
    public bool CheckButtons            = false;
    public bool CheckPlayer             = false;
    public bool UseZOrderFiltering      = true;
}

// ビルダーパターン（改善：メソッドチェーン）
public static class CollisionOptionsBuilder
{
    public static CollisionOptions ForNoEntryWindow(GameWindow self) => new()
    {
        ExcludeWindow = self,
        ExcludeChildren = true,
        CheckNormalWindows = true,
        CheckButtons = true,
    };

    public static CollisionOptions ForNormalWindow(GameWindow self) => new()
    {
        ExcludeWindow = self,
        ExcludeChildren = true,
    };
}
```

**ValidatePositionアルゴリズム（X/Y軸独立）:**

```
[X軸]
xSweep = SweepX(current, proposed)
各衝突対象と交差チェック:
  衝突時: candidateX を計算（衝突面から押し戻した位置）
  最小移動距離の候補を bestX に保存

[Y軸]
ySweep = SweepY(bestX位置, proposed)
同様に bestY を計算

戻り値: Rect(bestX ?? proposed.X, bestY ?? proposed.Y, width, height)
```

**SweepBounds計算（轴別）:**
```
SweepX(current, proposed):
  minX = min(current.Left, proposed.Left)
  maxX = max(current.Right, proposed.Right)
  → Rect(minX, current.Y, maxX-minX, current.Height)

SweepY(current, proposed):
  minY = min(current.Top, proposed.Top)
  maxY = max(current.Bottom, proposed.Bottom)
  → Rect(proposed.X, minY, proposed.Width, maxY-minY)
```

**CollisionFilter.ShouldSkip（ウィンドウスキップ判定）:**
```
skip if:
  window == excludeWindow                         → 自分自身
  excludeChildren && window is descendant of excludeWindow → 子孫
  window.Parent == excludeWindow（ValidatePosition時）    → 親
  window.IsMinimized                              → 最小化中
```

---

## 10. プレイヤーシステム

### 10.1 Player エンティティ

```csharp
// プレイヤーの境界（2種類に整理。現行の3種から削減）
public class PlayerBounds
{
    public Rect Collision { get; }  // 衝突判定・接地判定基準
    public Rect Visual    { get; }  // 描画領域（Collisionと同サイズ）

    public static readonly Vec2i DefaultSize = new(60, 60);

    public PlayerBounds(Vec2i position)
    {
        Collision = new Rect(position.X, position.Y, DefaultSize.X, DefaultSize.Y);
        Visual    = Collision;  // 現行のRENDER_RATIO=1.0に相当
    }
}
```

> **改善点:** 現行の3種境界（collision/render/display）を2種（collision/visual）に整理。  
> displayBoundsはWinFormsのForm領域を指す概念で、プラットフォーム非依存の新設計では不要。

### 10.2 プレイヤー状態機械

```csharp
public enum PlayerState { Idle, Running, Jumping, Falling, Landing }

// 状態遷移
Idle    → Running  : 水平入力あり && 接地
Idle    → Jumping  : ジャンプ入力 && 接地
Idle    → Falling  : 接地喪失

Running → Idle     : 水平入力なし && 接地
Running → Jumping  : ジャンプ入力 && 接地
Running → Falling  : 接地喪失

Jumping → Falling  : verticalVelocity > 0（頂点通過）
Falling → Landing  : 接地した瞬間
Landing → Idle     : Landingアニメーション完了（150ms）
```

### 10.3 アニメーションシステム

```csharp
public interface IPlayerAnimation
{
    Vec2f CurrentScale { get; }  // (scaleX, scaleY) を描画時に適用

    void SetState(PlayerState state);
    void Update(float deltaTime);
}
```

**状態別スケール変化:**

| 状態 | scaleX | scaleY | 補足 |
|------|--------|--------|------|
| Idle | 1.0 | 1.0 〜 1.05（呼吸） | 正弦波でゆっくり変化 |
| Running | 1.0 | 0.8 〜 1.2（屈伸） | 速度に応じた変化 |
| Jumping | 0.8 〜 1.0 | 1.0 〜 1.5 | StretchAndBack曲線 |
| Falling | 0.8 | 1.3 | Jumping最大値を維持 |
| Landing | 1.2 〜 1.5 | 0.6 〜 0.8 | EaseOutElastic（150ms） |

**AnimationCurves（数式）:**
```csharp
// ジャンプの伸び（0〜1の進行度）
static float StretchAndBack(float t)
    => 1.0f + MathF.Sin(t * MathF.PI) * 0.5f;

// 着地のバウンス
static float EaseOutElastic(float t)
{
    const float c4 = 2 * MathF.PI / 3;
    return MathF.Pow(2, -10 * t) * MathF.Sin((10 * t - 0.75f) * c4) + 1;
}

// 補間
static float Lerp(float a, float b, float t) => a + (b - a) * t;
```

**天井接近時のスケール制限:**
```
upMargin = player.Collision.Y - parentWindow.VisualBounds.Y
if upMargin < DefaultSize.Y * 0.5f:
    scaleLimit = 1.0f + 0.5f * (upMargin / (DefaultSize.Y * 0.5f))
    targetScaleY = min(targetScaleY, scaleLimit)
```

### 10.4 ウィンドウ間移動

**HandleWindowTransition（毎フレーム判定）:**

```
if currentParent == null:  // デスクトップ上
    newParent = windowManager.GetWindowFullyContaining(player.Collision)
    if newParent != null: SetParent(newParent)

else:  // 親ウィンドウ内
    newWindow = windowManager.GetTopWindowAt(player.Collision, currentParent)
    if newWindow != null && newWindow != currentParent:
        SetParent(newWindow)
```

**GetTopWindowAt（5点チェック）:**
```
5点 = [左下, 右下, 左上, 右上, 中心]

各点について Z-order前面から検索:
  → その位置にあるウィンドウを返す（最前面のもの）

全点の結果を集計:
  → 足元2点（左下・右下）を含む候補を優先
  → 複数ある場合: 底辺が低い順 → Z-index降順
  → 最有力ウィンドウを返す
```

**SetParent 処理:**
```
1. 旧親から removeChild
2. 新親に addChild
3. EventBus.Publish(PlayerParentChangedEvent)
4. 親の変更時: OnEnterWindow処理（エフェクトなど）
```

### 10.5 プレイヤー移動可能領域

```
currentParent != null の場合:
  movableRegion = currentParent.VisualBounds  // タイトルバー除く
  子ウィンドウ（ボタン等）の領域を除外

currentParent == null の場合:
  movableRegion = screen全体
```

### 10.6 PlayerFactory

```csharp
public interface IPlayerFactory
{
    Player Create(Vec2i startPosition);
    void Reset(Player player, Vec2i startPosition);
}
```

**生成フロー:**
```
1. physics = new PlayerPhysics(settings, inputService)
2. input   = new PlayerInput(inputService)
3. anim    = new PlayerAnimation()
4. player  = new Player(startPosition, physics, input, anim,
                        windowManager, noEntryZoneManager, settings)
5. physics.OnGrounded = player.HandleGrounding  // コールバック設定
6. return player
```

---

## 11. ステージシステム

### 11.1 ステージデータ（改善：JSONファイル定義）

現行: C#コードにハードコード  
**新設計: JSONファイルで外部定義（stages/*.json）**

```json
{
  "stageIndex": 1,
  "name": "Stage 1",
  "isTitleStage": false,
  "isGameClearStage": false,
  "enableDesktopIcons": false,
  "playerStart": { "x": 188, "y": 813 },
  "goal": { "x": 1250, "y": 750 },
  "goalInFront": false,
  "windows": [
    {
      "type": "NormalBlack",
      "position": { "x": 125, "y": 750 },
      "size": { "w": 625, "h": 250 }
    },
    {
      "type": "NormalWhite",
      "position": { "x": 625, "y": 625 },
      "size": { "w": 750, "h": 250 }
    },
    {
      "type": "TextDisplay",
      "position": { "x": 625, "y": 63 },
      "size": { "w": 375, "h": 125 },
      "text": "Stage 1"
    },
    {
      "type": "UnconstrainedResize",
      "position": { "x": 900, "y": 400 },
      "size": { "w": 300, "h": 200 },
      "appearance": {
        "backgroundColor": "#1A1A2E",
        "titleBarColor":   "#16213E",
        "titleText":       "Flip Me",
        "borderColor":     "#E94560",
        "borderWidth":     2
      }
    }
  ],
  "noEntryZones": [],
  "buttons": {
    "toTitle": { "x": 106, "y": 113 },
    "retry":   { "x": 369, "y": 113 }
  }
}
```

**ウィンドウ定義フィールド仕様:**

| フィールド | 型 | 必須 | 説明 |
|-----------|-----|------|------|
| `type` | string (WindowType) | 必須 | ウィンドウ種別 |
| `position` | {x,y} | 必須 | 初期位置（スクリーン座標） |
| `size` | {w,h} | 必須 | 初期サイズ（UnconstrainedResizeは論理サイズ、他は常に正） |
| `text` | string | TextDisplayのみ | 表示テキスト（appearance.titleTextとは別） |
| `appearance` | WindowAppearanceJSON | 省略可 | 外観のカスタマイズ（省略時はStrategy既定値） |

**WindowAppearanceJSON フィールド（すべて省略可）:**

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `backgroundColor` | "#RRGGBB[AA]" | 背景色 |
| `titleBarColor` | "#RRGGBB[AA]" | タイトルバー背景色 |
| `titleText` | string | タイトルバーに表示するテキスト |
| `titleTextColor` | "#RRGGBB[AA]" | タイトルテキスト色 |
| `borderColor` | "#RRGGBB[AA]" | 枠線の色 |
| `borderWidth` | int | 枠線の太さ（px） |
| `showBorder` | bool | 枠線の表示/非表示 |
| `showTitleBar` | bool | タイトルバーの表示/非表示 |

### 11.2 全ステージ一覧

| Index | 名前 | ウィンドウ数 | プレイヤー開始 | ゴール | 特徴 |
|-------|------|------------|------------|------|------|
| 0 | Title | 6 | (913, 813) | - | タイトル画面 |
| 1 | Stage 1 | 3 | (188, 813) | (1250, 750) | 基本移動 |
| 2 | Stage 2 | 4 | (188, 813) | (1625, 875) | Movable |
| 3 | Stage 3 | 4 | (188, 313) | (1625, 875) | Resizable |
| 4 | Stage 4 | 6 | (438, 355) | (125, 1000) | Z-order段階的 |
| 5 | Stage 5 | 11 | (313, 250) | (1500, 250) | Z-order複雑 |
| 6 | Stage 6 | 3 | (188, 813) | (1625, 250) | NoEntry静的領域 |
| 7 | Stage 7 | 3 | (750, 938) | (750, 250) | Resizable応用 |
| 8 | Stage 8 | 3 | (125, 875) | (1125, 125) | Resizable応用2 |
| 9 | Stage 9 | 4 | (1313, 750) | (375, 225) | 親子関係多層 |
| 10 | Stage 10 | 4 | (313, 813) | (469, 543) | Movable+親子 |
| 11 | Stage 11 | 2 | (313, 813) | (419, 543) | シンプルMovable |
| 12 | Stage 12 | 1 | (313, 813) | (419, 563) | Movable単一 |
| 13 | Stage 13 | 4 | (188, 813) | (1625, 875) | NoEntry+親子 |
| 14 | Stage 14 | 5 | (188, 813) | (1625, 875) | NoEntryウィンドウ基本 |
| 15 | Stage 15 | 5 | (188, 813) | (625, 375) | NoEntryウィンドウ複合 |
| 16 | Stage 16 | 3 | (750, 500) | (750, 875) | Minimizable |
| 17 | Stage 17 | 2 | (375, 875) | (750, 925) | ウィンドウ外移動1 |
| 18 | Stage 18 | 3 | (375, 875) | (950, 925) | ウィンドウ外移動2 |
| 19 | Stage 19 | 3 | (275, 875) | (1750, 525) | ウィンドウ外+NoEntry |
| 20 | Stage 20 | 5 | (375, 875) | (1750, 125) | ウィンドウ外+NoEntry2 |
| 21 | Stage 21 | 2 | (375, 875) | (1750, 125) | デスクトップアイコン |
| 22 | Game Clear | 6 | (913, 813) | - | クリア画面 |

### 11.3 ステージ遷移フロー

```csharp
public interface IStageManager
{
    StageIndex CurrentStage { get; }
    bool IsLoading { get; }

    Task LoadStageAsync(StageIndex index);   // ステージ切り替え
    void RestartCurrentStage();
    void NextStage();
    void ToTitle();
    bool CheckGoal(Rect playerCollision);    // 毎フレーム呼び出し
}
```

**LoadStageAsync フロー:**
```
1. Cleanup前ステージ
   - 全ゲームウィンドウを破棄
   - 静的NoEntryZoneをクリア
   - UIボタン・ゴールを破棄
   - プレイヤーをリセット

2. 100ms 待機（描画の安定化）

3. JSONからStageDataを読み込み

4. ウィンドウ群を生成・登録
   - windowFactory.Create(type, position, size)
   - windowManager.Register(window)

5. UI要素を生成
   - Goal.Create(stageData.goal)
   - ボタン類（retry, toTitle等）

6. プレイヤーをstage.playerStartに配置

7. 全要素を一括表示（明滅防止）
   - SDL2の DeferWindowPos相当で一括更新

8. EventBus.Publish(StageStartedEvent)
```

---

## 12. 描画システム

### 12.1 IRenderer

```csharp
public interface IRenderer : IDisposable
{
    Vec2i ScreenSize { get; }

    // フレーム制御
    void BeginFrame();
    void EndFrame();

    // 基本図形
    void DrawRect(Rect rect, GameColor color, float strokeWidth = 1);
    void FillRect(Rect rect, GameColor color);
    void DrawLine(Vec2i from, Vec2i to, GameColor color, float width = 1);
    void DrawCircle(Vec2i center, float radius, GameColor color);
    void FillCircle(Vec2i center, float radius, GameColor color);

    // テキスト
    void DrawText(string text, Vec2i position, GameColor color,
                  float fontSize, TextAlign align = TextAlign.TopLeft);
    Vec2i MeasureText(string text, float fontSize);

    // 画像（スプライト等、将来拡張）
    void DrawImage(IGameImage image, Rect dest);

    // クリッピング
    void SetClip(Rect rect);
    void ClearClip();

    // 変換
    void PushTransform(Vec2f scale, Vec2i translate);
    void PopTransform();
}
```

### 12.2 SkiaRenderer

```csharp
public sealed class SkiaRenderer : IRenderer
{
    // SDL2のウィンドウサーフェスに SKCanvas を生成して描画
    // SkiaSharp: SKSurface.Create() → SKCanvas.DrawXxx()
    // EndFrame(): SKSurface.Flush() → SDL_UpdateWindowSurface()
}
```

### 12.3 描画パイプライン（RenderingSystem）

**注意:** すべてのゲームウィンドウはボーダーレスSDL2ウィンドウであり、  
タイトルバー・枠線・ボタンも含めてゲームが描画する。

```
毎フレームの描画順序:

1. BeginFrame（各SDL2ウィンドウのバッファクリア）

2. ゴール描画（ゴールが属するウィンドウのバッファ上）
   - Rect(goalPos, 64×64) に "G" 文字
   - アウトライン: 8方向に1px offset（親ウィンドウの明度で色決定）
   - 本文字: Gold色

3. 静的NoEntryZone描画
   - 縞模様アニメーション（赤/黒、時計回り）
   - STRIPE_WIDTH = 20px
   - アニメーション速度: 40px/秒

4. ゲームウィンドウ描画（Z-index昇順 = 背面から）
   各ウィンドウについて:
   a. [クローム描画] タイトルバー背景 (Appearance.TitleBarColor) を FillRect
   b. [クローム描画] タイトルテキスト (Appearance.TitleText) を描画
      - FlipY=true の場合: 上下反転（Y軸スケール -1 で描画）
      - FlipX=true の場合: 左右反転（X軸スケール -1 で描画）
   c. [クローム描画] ボタン（－など）を描画
      - FlipY: タイトルバーが下端 → ボタン位置を下端に
      - FlipX: ボタンが左側に
   d. クライアント領域の背景色で FillRect (Appearance.BackgroundColor)
   e. [変換適用] FlipX/FlipY の描画変換行列を PushTransform
   f. [コンテンツ] Strategy.DrawStrategyMark() をクライアント座標で描画
   g. [変換解除] PopTransform
   h. [枠線] Appearance.BorderColor で外周枠線を DrawRect（ShowBorder時）
   i. 不可侵ウィンドウの場合: 外周アニメーション縞模様枠を描画（Z-orderクリップ付き）

5. プレイヤー描画（プレイヤーの現在親ウィンドウのバッファ上）
   - 親ウィンドウの FlipX/FlipY 変換行列を適用した上で描画
   - scaleX/scaleY を適用した矩形（アニメーション）
   - 向き（left/right）でX軸反転（FlipX 親の中ではさらに反転）
   - 色: 白（仮。スプライト対応は将来）

6. UIボタン描画（各ボタン = 独立したウィンドウ）
   - 各ボタンウィンドウのバッファに文字を描画

7. デバッグオーバーレイ（IsDebugMode時のみ）
   - プレイヤー情報（GravityMultiplier含む）
   - Z-order情報
   - パフォーマンス情報
   - 衝突境界の可視化
   - FlipX/FlipY状態の表示

8. EndFrame（全SDL2ウィンドウのバッファをフラッシュ）
```

**タイトルバー描画詳細:**

```csharp
void DrawTitleBar(GameWindow window, IRenderer renderer)
{
    var titleRect = window.Transform.TitleBarRect(GameWindow.TitleBarHeight);
    
    // タイトルバー背景
    renderer.FillRect(titleRect, window.Appearance.TitleBarColor);
    
    // テキスト（FlipX/FlipYを考慮した変換）
    if (!string.IsNullOrEmpty(window.Appearance.TitleText))
    {
        var textPos = titleRect.Center;
        var scaleX = window.FlipX ? -1f : 1f;
        var scaleY = window.FlipY ? -1f : 1f;
        renderer.PushTransform(new Vec2f(scaleX, scaleY), textPos);
        renderer.DrawText(window.Appearance.TitleText, Vec2i.Zero,
                         window.Appearance.TitleTextColor, 12f, TextAlign.Center);
        renderer.PopTransform();
    }
    
    // ミニマイズボタン（Minimizable系のみ）
    if (window.Strategy is MinimizableWindowStrategy or MinimizableNoEntryStrategy)
    {
        var btnPos = window.FlipX
            ? new Vec2i(titleRect.Left + 5, titleRect.Center.Y)   // FlipX: 左側
            : new Vec2i(titleRect.Right - 25, titleRect.Center.Y); // 通常: 右側
        // FlipY: タイトルバーが下端なので描画位置は titleRect 内で同じ相対位置
        DrawMinimizeButton(renderer, btnPos, window.FlipX, window.FlipY);
    }
}
```

### 12.4 NoEntry縞模様アニメーション

**時計回り循環ストライプ（OutlineRenderer）:**

```
STRIPE_WIDTH = 20         // 赤/黒1組のpx
BORDER_WIDTH = 5          // 枠線の太さ
SPEED        = 40         // px/秒

周回距離 perimeter = 2*(W + H)
offset = (offset + SPEED * deltaTime) % perimeter  // 時計回り進行

描画は4辺を独立に:
  上辺 (左→右): periStart=0,        length=W,   horizontal=true,  forward=true
  右辺 (上→下): periStart=W,        length=H,   horizontal=false, forward=true
  下辺 (右→左): periStart=W+H,      length=W,   horizontal=true,  forward=false
  左辺 (下→上): periStart=2W+H,     length=H,   horizontal=false, forward=false

各辺の描画（DrawClockwiseSide）:
  startDist = periStart - offset（mod perimeter）
  for ストライプ位置 in 辺の範囲:
    色 = (startDist % (STRIPE_WIDTH*2) < STRIPE_WIDTH) ? Red : DarkGray
    描画
```

### 12.5 カスタムフォント管理

```csharp
public static class FontManager
{
    public static void Initialize()
    {
        // 優先順位:
        // 1. 埋め込みリソース（NativeAOT対応のためEmbeddedResource推奨）
        // 2. assets/fonts/ 物理ファイル
        // 3. システムフォント（フォールバック）
        LoadFont("prstart",  "prstart.ttf");
        LoadFont("prstartk", "prstartk.ttf");
    }

    public static IGameFont GetFont(string name, float size);
}
```

---

## 13. UIシステム

### 13.1 ボタン仕様

すべてのボタンは独立したOSウィンドウとして表示される（WinForms Formの代わりにIPlatformWindowを使用）。

| ボタン | サイズ | テキスト | 動作 |
|--------|--------|---------|------|
| StartButton | 150×40 | "Start" | stageManager.NextStage() |
| RetryButton | 150×40 | "Retry" | stageManager.RestartCurrentStage() |
| ToTitleButton | 150×40 | "Title" | stageManager.ToTitle() |
| ExitButton | 150×40 | "Exit" | Application.Quit() |

**外観仕様:**
- 背景: RGB(200, 200, 200)、ホバー時: RGB(230, 230, 230)
- 枠線: RGB(100, 100, 100)、太さ2px
- フォント: PressStart、14pt
- 最前面（常に他ウィンドウより手前）

### 13.2 設定画面（SettingsWindow）

独立したウィンドウとして表示。

**レイアウト:**
```
┌─────────────────────────────┐
│ Settings              400px  │
├─────────────────────────────┤
│ [Player] [Window] [Gameplay] │
├─────────────────────────────┤
│ Player タブ:                 │
│  Movement Speed: [___400__]  │
│  Gravity:        [__1000__]  │
│  Jump Force:     [___700__]  │
│  Default Width:  [____60__]  │
│  Default Height: [____60__]  │
│                              │
│ Window タブ:                 │
│  Min Width:  [___100__]      │
│  Min Height: [___100__]      │
│                              │
│ Gameplay タブ:               │
│  Target FPS:      [___60__]  │
│  Snap Distance:   [___20__]  │
├─────────────────────────────┤
│ [Apply]          [Save & Close] │
└─────────────────────────────┘
```

**数値入力の範囲:**

| 項目 | 最小 | 最大 | デフォルト |
|------|------|------|-----------|
| Movement Speed | 0 | 1000 | 400 |
| Gravity | 0 | 2000 | 1000 |
| Jump Force | 0 | 1000 | 700 |
| Default Width | 10 | 200 | 60 |
| Default Height | 10 | 200 | 60 |
| Min Width | 50 | 500 | 100 |
| Min Height | 50 | 500 | 100 |
| Target FPS | 30 | 144 | 60 |
| Snap Distance | 0 | 50 | 20 |

### 13.3 ゴール（Goal）

```
ゴールの構造:
  CollisionBounds: 64×64（接触判定用）
  VisualBounds: 96×96（描画用, 1.5倍）

ゴールの描画:
  背景: 透明
  文字: "G"（PressStartフォント、Gold色）
  アウトライン: 8方向×1px、親ウィンドウ明度で色決定
    → 明度 < 0.5 （暗い背景）: R+100, G+100, B+100
    → 明度 >= 0.5（明るい背景）: R-50,  G-50,  B-50

ゴール判定:
  player.CollisionBounds.IntersectsWith(goal.CollisionBounds)

親ウィンドウ自動判定:
  毎フレーム GetWindowFullyContaining(goal.CollisionBounds) で更新
  goalInFront=true の場合: 常に最前面
```

---

## 14. デスクトップ統合

### 14.1 プラットフォーム抽象

```csharp
public interface IDesktopIntegration
{
    // デスクトップアイコン一覧を取得
    IReadOnlyList<DesktopIcon> GetDesktopIcons();

    // 更新通知を受け取る（アイコン追加・削除・移動）
    event Action DesktopIconsChanged;

    // プラットフォームがデスクトップ統合をサポートするか
    bool IsSupported { get; }
}

public readonly record struct DesktopIcon(
    string Name,
    Rect ClickableBounds  // スクリーン座標
);
```

### 14.2 Windows実装

**アイコン検出方法（Win32 API）:**

```
1. Progman ウィンドウを探す
   hwnd = FindWindow("Progman", null)

2. SHELLDLL_DefView → SysListView32 を探す
   hList = FindWindowEx(FindWindowEx(hwnd, null, "SHELLDLL_DefView", null),
                        null, "SysListView32", null)

3. 見つからない場合（Windows 10/11 WorkerW対応）:
   EnumWindows で WorkerW を列挙して再探索

4. アイコン情報取得:
   - GetWindowThreadProcessId: プロセスID
   - OpenProcess: PROCESS_VM_READ | PROCESS_VM_WRITE
   - SendMessage(LVM_GETITEMCOUNT): アイコン数
   - 各アイコン:
     - LVM_GETITEMTEXT: 名前取得
     - LVM_GETITEMRECT(LVIR_SELECTBOUNDS): クリック可能領域
     - ClientToScreen: スクリーン座標変換
   - CloseHandle: プロセスクリーンアップ

5. テキストデコード（優先順位）:
   1. ANSI（日本語環境用）
   2. Unicode UTF-16LE
   3. UTF-8
   4. ASCII印刷可能文字のみ抽出
```

**変更監視:**
```
Shell通知登録: SHChangeNotifyRegister
  - SHCNE_CREATE, SHCNE_DELETE, SHCNE_RENAMEITEM, SHCNE_UPDATEITEM

レジストリ監視（アイコン配置変更）:
  HKEY_CURRENT_USER\Software\Microsoft\Windows\Shell\Bags
  RegNotifyChangeKeyValue で非同期監視
  デバウンス: 500ms

変更通知 → GetDesktopIcons() 再取得 → DesktopIconsChanged イベント発火
```

**キャッシュ戦略:**
```
HashSet/Dictionary で前回取得結果を保存
HasChanged(newIcons): ハッシュ値比較で差分検出
変更あり時のみ更新・通知（パフォーマンス最適化）
```

### 14.3 macOS実装（Phase4で実装）

```csharp
public sealed class MacOSDesktopIntegration : IDesktopIntegration
{
    public bool IsSupported => true;

    // NSWorkspace + NSFileManager でデスクトップアイテムを取得
    // P/Invoke to Objective-C runtime
    public IReadOnlyList<DesktopIcon> GetDesktopIcons() { /* ... */ }
}
```

### 14.4 Linux実装（stub）

```csharp
public sealed class LinuxDesktopIntegration : IDesktopIntegration
{
    // Linuxはデスクトップアイコンの標準APIがないためstub
    public bool IsSupported => false;
    public IReadOnlyList<DesktopIcon> GetDesktopIcons() => [];
    public event Action DesktopIconsChanged { add { } remove { } }
}
```

### 14.5 プラットフォームファクトリー

```csharp
public static class PlatformFactory
{
    public static IDesktopIntegration CreateDesktopIntegration()
    {
        if (OperatingSystem.IsWindows()) return new WindowsDesktopIntegration();
        if (OperatingSystem.IsMacOS())   return new MacOSDesktopIntegration();
        return new LinuxDesktopIntegration();
    }

    public static IPlatformWindowSystem CreateWindowSystem()
        => new Sdl2WindowSystem();  // SDL2は全プラットフォームで共通

    public static IInputProvider CreateInputProvider()
        => new Sdl2InputProvider();  // SDL2は全プラットフォームで共通
}
```

---

## 15. 設定システム

### 15.1 全設定値

```csharp
public record GameSettings
{
    public PhysicsSettings  Physics  { get; init; } = new();
    public WindowSettings   Window   { get; init; } = new();
    public GameplaySettings Gameplay { get; init; } = new();
}

public record PhysicsSettings
{
    public float MoveSpeed       { get; init; } = 400.0f;
    public float Gravity         { get; init; } = 1000.0f;
    public float JumpForce       { get; init; } = 700.0f;
    public float MaxFallSpeed    { get; init; } = 2000.0f;  // 新規追加
    public Vec2i DefaultSize     { get; init; } = new(60, 60);
    public int   GroundTolerance { get; init; } = 5;
    public int   GroundCheckH    { get; init; } = 15;
}

public record WindowSettings
{
    public Vec2i MinSize     { get; init; } = new(100, 100);
    public Vec2i MaxSize     { get; init; } = new(800, 600);  // 追加
    public float SnapDistance { get; init; } = 20.0f;
}

public record GameplaySettings
{
    public int TargetFPS    { get; init; } = 60;
    public int PhysicsHz    { get; init; } = 120;  // 新規（固定物理レート）
}
```

### 15.2 設定管理

```csharp
public interface IGameSettings
{
    GameSettings Current { get; }

    void Update(GameSettings newSettings);  // 不変レコードで更新
    void Save();                            // config/settings.json に保存
    void Load();                            // config/settings.json から読み込み
}
```

**ファイル読み込み優先順位:**
1. 実行ファイルと同じディレクトリの `config/settings.json`
2. 埋め込みリソース（デフォルト値）
3. コード内デフォルト値

**変更通知:**
- `IEventBus.Publish(SettingsChangedEvent)` で通知
- リアルタイム設定変更対応
- ファイル監視（config/settings.json）→ 変更検出→ 自動リロード（デバッグ時）

---

## 16. デバッグシステム

### 16.1 DebugOverlay

F3キーで表示切替。独立したレンダリングレイヤーとしてフレーム最後に描画。

**表示パネル一覧:**

| パネル | 位置 | 表示内容 |
|--------|------|---------|
| PlayerInfo | (10, 10) | State, CollisionPos/Size, IsGrounded, VerticalVelocity, ParentWindow |
| WindowHierarchy | (400, 10) | Z-index, 親子関係ツリー, Strategy名 |
| PerformanceInfo | (1600, 100) | FPS, FrameTime, 各スコープの処理時間 |
| GoalInfo | (10, 250) | GoalPos, ParentWindow |
| SettingsInfo | (800, 10) | 現在の全設定値 |

**追加デバッグ描画:**
- プレイヤー衝突境界: 緑の矩形
- 接地判定エリア: 赤の矩形
- 不可侵境界: 半透明赤
- 親子関係線: 黄色点線
- Z-order番号: 各ウィンドウの右上隅

### 16.2 PerformanceMonitor

```csharp
public interface IPerformanceMonitor
{
    IDisposable BeginScope(string name);  // usingブロックで計測

    float GetFPS();
    float GetFrameTimeMs();
    IReadOnlyDictionary<string, float> GetScopeTimes();  // スコープ別平均時間
}
```

**実装（指数移動平均）:**
```
RecordScope(name, ms):
  times[name] = times[name] * 0.95f + ms * 0.05f
```

---

## 17. ロギング・エラーハンドリング

### 17.1 ILogger

```csharp
public interface ILogger
{
    void Log(LogLevel level, string message, Exception? ex = null, string? context = null);

    void Trace(string msg, string? ctx = null);
    void Debug(string msg, string? ctx = null);
    void Info(string msg, string? ctx = null);
    void Warn(string msg, string? ctx = null);
    void Error(string msg, Exception? ex = null, string? ctx = null);
    void Critical(string msg, Exception? ex = null, string? ctx = null);
}

public enum LogLevel { Trace, Debug, Info, Warn, Error, Critical }
```

**FileLoggerの出力形式:**
```
[2026-05-20 12:34:56.789] INFO     ゲーム初期化完了 [MainGame]
[2026-05-20 12:34:56.790] ERROR    衝突判定でエラー [CollisionService]
                                   Exception: NullReferenceException: ...
```

**スレッドセーフ:** `lock` で保護  
**Release構成:** NullLogger（出力なし、オーバーヘッドゼロ）

### 17.2 IErrorHandler

```csharp
public interface IErrorHandler
{
    void Handle(string message, ErrorSeverity severity,
                Exception? ex = null, string? context = null);
    T? Try<T>(Func<T> op, string opName, T? fallback = default);
    Task<T?> TryAsync<T>(Func<Task<T>> op, string opName, T? fallback = default);
}

public enum ErrorSeverity
{
    Low,      // ログのみ（描画エラー等）
    Medium,   // ログ + デバッグ通知
    High,     // ログ + ユーザー通知（回復可能）
    Critical, // ログ + ダイアログ + 終了
}
```

---

## 18. 現行設計からの改善点

### 18.1 アーキテクチャ改善

| 項目 | 現行 | 新設計 | 理由 |
|------|------|--------|------|
| フレームワーク | WinForms | SDL2 + SkiaSharp | クロスプラットフォーム、NativeAOT対応 |
| ランタイム | .NET 6 | .NET 9 | NativeAOT安定版、LTS |
| 物理更新 | 可変デルタタイム | 固定タイムステップ(120Hz) | 物理演算の決定論性・安定性向上 |
| 通知システム | 各所にオブザーバー | 型安全なEventBus | 疎結合、型安全 |
| 静的参照 | Current多数残存 | 完全DI | テスト可能、循環依存なし |
| リフレクション | Activator.CreateInstance | Factory登録 | NativeAOT対応 |
| ステージ定義 | C#ハードコード | JSONファイル | エディタ対応、外部編集可能 |

### 18.2 コード設計改善

| 項目 | 現行 | 新設計 | 理由 |
|------|------|--------|------|
| 境界の種類 | 3種（collision/render/display） | 2種（collision/visual） | displayはWinForms依存概念 |
| Z-order同期 | 複雑な条件分岐 | 常にシンプルな一括処理 | バグ修正済みの安定ロジックを採用 |
| 最大落下速度 | 無制限 | MaxFallSpeed=2000px/s | 極端な高速落下時の不具合防止 |
| プラットフォーム依存 | 全体に散在 | Platform/ に集約 | 変更箇所の明確化 |
| CollisionOptions | new + bool設定 | Builderパターン | 読みやすさ向上 |
| Strategy合成 | 継承（XXXNoEntry） | 合成（inner Strategy保持） | 柔軟性・テスト容易性 |

### 18.3 削除・簡略化

| 削除/簡略化する要素 | 理由 |
|---------------------|------|
| WM_NChitTest等のWin32メッセージ処理 | SDL2が代替 |
| BufferedGraphicsManager | SkiaSharpが独自バッファリング |
| RegionExtensions（GDI+ Region） | SkiaSharp SKPath/SKRegion で代替 |
| RegistryIconPositionWatcher | IDesktopIntegration内部実装に統合 |
| GameForm（透明フルスクリーンForm） | SDL2のOverlay相当で代替 |
| ZOrderManager（旧実装） | WindowZOrderManagerに統合済み |

### 18.4 将来拡張のための設計（ただしYAGNIを維持）

以下は「拡張可能な設計にするが実装しない」事項：

- **ステージエディタ**: JSONステージ定義を採用したことで実現可能
- **スプライト対応**: IRenderer.DrawImage() を定義済み（実装は未定）
- **BGM/SE**: IRenderer に Audio層を追加すれば対応可能な設計

### 18.5 新機能：ウィンドウカスタマイズ・反転システム

現行実装に存在せず、新設計で追加された機能群。

| 機能 | 概要 | 実装箇所 |
|------|------|---------|
| WindowAppearance | ウィンドウの外観（背景色・枠線・タイトルバー色・テキスト）を自由にカスタマイズ | `WindowAppearance` レコード、各 Strategy の `DefaultAppearance` |
| カスタムクローム | OS標準タイトルバーを廃止し、ゲームが全描画。FlipY時にタイトルバーを下端に移動できる | SDL2ボーダーレスウィンドウ + `DrawTitleBar()` |
| UnconstrainedResize | サイズ制限なし（MIN/MAXなし）のリサイズ。ドラッグでマイナスサイズにできる | `UnconstrainedResizeWindowStrategy`、`WindowTransform` |
| FlipX（水平反転） | Width < 0 でウィンドウ左右反転。コンテンツ・タイトルバーボタン・子要素すべて反転 | `WindowTransform.FlipX`、描画変換行列 |
| FlipY（垂直反転） | Height < 0 でウィンドウ上下反転。タイトルバーが下端へ移動、テキスト反転 | `WindowTransform.FlipY`、描画変換行列 |
| GravityMultiplier | FlipY祖先の数に応じて重力方向を ±1 で反転。FlipYウィンドウ内ではプレイヤーが「天井」に落ちる | `IPlayerPhysics.GravityMultiplier`、`CountFlipYAncestors()` |
| 反転時の接地判定 | 重力反転時はプレイヤー上端（Top）を基準に接地判定。SweepBoundsも上方向に拡張 | `PlayerPhysics.CheckGrounded()` |

**設計上の重要な判断:**

1. **OS titlebarを完全廃止**: FlipYでタイトルバーを下に移動するにはOS標準装飾を使えない。  
   SDL2ボーダーレスウィンドウ + 自前描画を採用することで反転を実現。

2. **WindowTransform でアンカー固定リサイズ**: 現行の ResizableWindowStrategy は左上固定でリサイズするが、  
   UnconstrainedResize も同じくアンカー（左上）固定。幅が負になると視覚的な左端が変わる。

3. **子要素の扱い**: 子ゲームウィンドウはスクリーン座標を持つ実 SDL2 ウィンドウなので変換行列外に存在する。  
   リサイズ・反転時は `UpdateChildrenInContentSpace()` で位置と大きさを再計算して配置し直す。  
   「コンテンツ空間（Content Space）」を介することで、スケールと反転を一つの変換パイプラインで統一的に扱う。  
   プレイヤーのみ描画上の変換行列内で処理される（仮想エンティティのため）。

4. **ネストしたFlipYは相殺**: FlipY祖先が偶数個 → 重力通常、奇数個 → 重力反転。  
   これにより「反転ウィンドウの中に通常ウィンドウを入れると重力が戻る」というパズル要素が生まれる。

---

*このドキュメントはゼロから再構築する際の完全な仕様書です。  
コードは一切含まず、「何をどう動かすか」を記述することに特化しています。*
