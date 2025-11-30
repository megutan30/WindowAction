# 衝突判定システム統一化リファクタリング - 詳細実装計画

**作成日**: 2025-01-29
**最終更新**: 2025-01-29
**ステータス**: フェーズ1完了（コミット: 2acb34f）

---

## 📋 実装進捗サマリー

### 全体進捗
- [x] **フェーズ1**: 低リスク統一化（完了: 2025-01-29）
- [x] **フェーズ2-3**: 実装しない判断（2025-01-30）

### コード削減実績
- **開始時**: 180行の重複コード
- **フェーズ1後**: 約80行（約100行削減、約56%削減）
- **最終成果**: フェーズ1のみで十分な改善達成

### 実装しない判断（YAGNI原則適用）
- **フェーズ2**: PlayerPhysics.CheckGrounded()の統一化
  - 現在の実装が正常に動作しており、リファクタリングの必要性が低い
  - 拡張メソッドでは複雑性が増加（INoEntryZoneManager, IWindowManager等の依存関係）
  - 専用のGroundDetectionServiceは過剰な抽象化（YAGNI違反）
  - 1-2週間のテスト期間が必要だが、自動テスト環境がない
  - ROI低: 実装コストに対してメリットが少ない

- **フェーズ3**: NoEntryZoneManagerの非推奨化
  - CollisionServiceとNoEntryZoneManagerの両方が正常に動作
  - 3ヶ月の非推奨期間を設ける必要性が低い
  - 現状のFallbackパターンで十分に機能している

---

## ✅ 完了済み（2025-01-29）

### 定数統一化とスイープ判定実装
- [x] CollisionFilter.csに境界定数追加（NOENTRY_BOUNDARY_WIDTH=5, PARENT_BOUNDARY_BUFFER=5）
- [x] CollisionFilter.GetBoundaryWidth()で定数使用
- [x] CollisionFilter.ApplyParentBoundaryBuffer()で定数使用
- [x] CollisionFilter.ApplyParentBoundaryBufferForResize()で定数使用
- [x] NoEntryBoundaryCollider.csで定数使用（borderThickness）
- [x] WindowStrategies.CheckChildBoundaryContact()で定数使用
- [x] NoEntryZoneManager.csにCreateSweepBounds()メソッド追加
- [x] NoEntryZoneManager.GetValidPosition()にスイープ判定適用（静的NoEntryZone）
- [x] NoEntryZoneManager.GetValidPosition()にスイープ判定適用（不可侵ウィンドウ境界）
- [x] NoEntryZoneManager.GetValidPosition()にスイープ判定適用（通常ウィンドウ）
- [x] ビルド成功確認（0エラー、17警告は既存）

---

## 🚀 フェーズ1: 低リスク統一化（✅ 完了: 2025-01-29）

**目標**: 重複コード150行削減（83%削減） → **実績**: 約100行削減（約56%削減）
**推定時間**: 2-3時間 → **実績**: 約2時間
**リスク**: 低
**コミット**: 2acb34f

### 1.1 動作テストとコミット

#### 1.1.1 スイープ判定の動作テスト
- [x] ゲームを起動
- [x] 細いプレイヤー（デフォルトサイズ）で不可侵ウィンドウに接近
- [x] 通常速度での移動が正常に動作することを確認
- [x] 高速移動時に不可侵境界を貫通しないことを確認
  - [x] X軸方向（左右）の高速移動
  - [x] Y軸方向（上下）の高速移動
  - [x] 斜め方向の高速移動
- [x] エッジケースの確認
  - [x] 境界ギリギリでの移動
  - [x] 複数の不可侵ウィンドウ間での移動
  - [x] 親子ウィンドウでの移動

#### 1.1.2 コミット（既存分として完了）
- [x] git statusで変更ファイルを確認
- [x] git diff で変更内容を確認
- [x] git add で以下をステージング:
  - [x] CollisionFilter.cs
  - [x] NoEntryBoundaryCollider.cs
  - [x] WindowStrategies.cs
  - [x] NoEntryZoneManager.cs
- [x] コミットメッセージ作成
- [x] git commit実行
- [x] git log で確認

---

### 1.2 SweepBoundsHelperクラスの作成（✅ 完了）

**目標**: 3箇所の重複コード（~50行）を1箇所に統一 → **達成**

#### 1.2.1 新規ファイル作成
- [x] `D:\DevelopProject\WindowAction\MultiWindowActionGame\Services\SweepBoundsHelper.cs` を作成

#### 1.2.2 クラス実装
- [x] 名前空間定義: `namespace MultiWindowActionGame.Services`
- [x] クラス定義: `public static class SweepBoundsHelper`
- [x] XMLドキュメントコメント追加
- [x] `CreateSweepBounds()` メソッド実装（両軸）
- [x] `CreateSweepBoundsXAxis()` メソッド実装（X軸のみ）
- [x] `CreateSweepBoundsYAxis()` メソッド実装（Y軸のみ）
- [x] `CreateVerticalSweepBounds()` メソッド実装（PlayerPhysics用）
- [x] using ディレクティブ追加（System, System.Drawing）

#### 1.2.3 PlayerPhysics.csでの使用
- [x] PlayerPhysics.cs を開く（行104-109付近）
- [x] using MultiWindowActionGame.Services 追加
- [x] 独自のSweep Bounds計算コード削除（行104-109）
- [x] `SweepBoundsHelper.CreateVerticalSweepBounds()` 呼び出しに置き換え

#### 1.2.4 CollisionValidator.csでの使用
- [x] CollisionValidator.cs を開く（行38-60付近）
- [x] `CreateSweepBounds()` メソッド削除（行38-60）
- [x] 呼び出し箇所を `SweepBoundsHelper` メソッドに置き換え（8箇所）
  - [x] ValidatePosition内のX軸チェック（3箇所）
  - [x] ValidatePosition内のY軸チェック（3箇所）
  - [x] 不可侵境界のX軸チェック（1箇所）
  - [x] 不可侵境界のY軸チェック（1箇所）

#### 1.2.5 NoEntryZoneManager.csでの使用
- [x] NoEntryZoneManager.cs を開く（行139-161付近）
- [x] `CreateSweepBounds()` メソッド削除（行130-161）
- [x] 呼び出し箇所を `SweepBoundsHelper` メソッドに置き換え（6箇所）
  - [x] GetValidPosition内の静的NoEntryZone判定（X軸）
  - [x] GetValidPosition内の静的NoEntryZone判定（Y軸）
  - [x] GetValidPosition内の不可侵ウィンドウ境界判定（X軸）
  - [x] GetValidPosition内の不可侵ウィンドウ境界判定（Y軸）
  - [x] GetValidPosition内の通常ウィンドウ判定（X軸）
  - [x] GetValidPosition内の通常ウィンドウ判定（Y軸）

#### 1.2.6 ビルドとテスト
- [x] `dotnet build MultiWindowActionGame.sln` 実行
- [x] ビルドエラーがないことを確認
- [x] 警告が増えていないことを確認（17警告、既存のまま）

#### 1.2.7 コミット
- [x] git add で以下をステージング:
  - [x] SweepBoundsHelper.cs（新規）
  - [x] PlayerPhysics.cs
  - [x] CollisionValidator.cs
  - [x] NoEntryZoneManager.cs
  - [x] WindowStrategies.cs
  - [x] CollisionFilter.cs
  - [x] NoEntryBoundaryCollider.cs
  - [x] COLLISION_REFACTORING_PLAN.md
- [x] コミット実行（コミット: 2acb34f）

---

### 1.3 WindowStrategiesのif-elseパターン統一化（✅ 完了）

**目標**: 5箇所の重複コード（~60行）を3つのヘルパーメソッドに統一 → **達成**

#### 1.3.1 BaseWindowStrategyにヘルパーメソッド追加
- [x] WindowStrategies.cs を開く
- [x] BaseWindowStrategy クラスにヘルパーメソッド追加
  - [x] `CheckCollision()` メソッド追加
  - [x] `ValidateSize()` メソッド追加
  - [x] `ValidatePosition()` メソッド追加

#### 1.3.2-1.3.5 各Strategyでの使用
- [x] ResizableWindowStrategy.UpdateResize()で `CheckCollision()` 使用
- [x] ResizableWindowStrategy.CalculateNewSize()で `ValidateSize()` 使用
- [x] MovableWindowStrategy.CalculateMovement()で `ValidatePosition()` 使用
- [x] MovableWindowStrategy.CheckCollision()で `CheckCollision()` 使用
- [x] 4箇所のif-elseパターン削除（約40行削減）

#### 1.3.6 ビルドとテスト
- [x] `dotnet build MultiWindowActionGame.sln` 実行
- [x] ビルドエラーなし、警告17個（既存）

#### 1.3.7 コミット
- [x] コミット実行（コミット: 2acb34f、Phase 1全体として）

---

### 1.4 PlayerPhysicsフィルタリング統一化

**判定**: 実装不要（既にシンプルな実装）

#### 1.4.1 現状確認
- [x] PlayerPhysics.csのフィルタリングロジックを確認
- [x] `if (window.IsNoEntryWindow) continue;` パターンのみ
- [x] CollisionFilter.ShouldSkipWindow導入は過剰な抽象化と判断

#### 1.4.2 結論
- [x] Phase 1.4はスキップ
- [x] 既存のシンプルな実装を維持
- [x] KISSの原則に従い、不要な複雑化を避ける

---

### 1.5 フェーズ1完了確認（✅ 完了）

#### 1.5.1 総合テスト
- [x] ビルド成功確認（0エラー、17警告は既存）
- [x] Phase 1実装完了

#### 1.5.2 ドキュメント更新
- [x] COLLISION_REFACTORING_PLAN.md更新
- [x] Phase 1完了マーク
- [x] 実績記録（約100行削減、約56%削減）
- [x] コミット実行（コミット: 2acb34f）

---

## 🚫 フェーズ2-3: 実装しない（YAGNI原則適用）

**決定日**: 2025-01-30
**ステータス**: 実装しない

### 実装しない理由

#### フェーズ2: PlayerPhysics.CheckGrounded()の統一化

**当初の目標**: CollisionServiceExtensionsでPlayerPhysics.CheckGrounded()を統一化

**実装しない理由**:
1. **現在の実装が正常に動作**: PlayerPhysics.CheckGrounded()は正常に動作しており、バグもない
2. **複雑性の増加**: 拡張メソッドに以下の依存関係が必要で、シグネチャが複雑化
   - INoEntryZoneManager（静的NoEntryZone判定）
   - IWindowManager（ボタン取得、ウィンドウ判定）
   - GameWindow? parentWindow（親コンテキスト）
   - PlayerSettings（GroundCheckHeight設定）
3. **過剰な抽象化**: 専用のGroundDetectionServiceを作成するのは過剰（YAGNI違反）
4. **テスト環境の欠如**: 1-2週間のテスト期間が必要だが、自動テスト環境がない
5. **低ROI**: 実装コスト（3-4時間 + テスト期間）に対してメリットが少ない

#### フェーズ3: NoEntryZoneManagerの非推奨化

**当初の目標**: [Obsolete]属性を追加してNoEntryZoneManagerを非推奨化し、3ヶ月後に削除

**実装しない理由**:
1. **両システムが正常動作**: CollisionServiceとNoEntryZoneManagerの両方が正常に動作
2. **Fallbackパターンが機能**: 現在のFallbackパターンで十分に機能している
3. **3ヶ月の非推奨期間が不要**: 単一開発者プロジェクトで長期非推奨期間は不要
4. **保守コストの増加**: [Obsolete]属性の管理と削除作業のコストが高い

### CollisionServiceExtensions.csの扱い

**現状**: NotImplementedExceptionのプレースホルダーとして存在

**保持理由**:
- 将来、本当に必要になった時のための設計ドキュメントとして機能
- ファイルの存在がコードベースに悪影響を与えない（使用されていない）
- 削除せずに保持することで、設計判断の記録として残る

**今後の方針**:
- 現状のまま保持（NotImplementedExceptionのまま）
- 将来的に実装の必要性が生じた場合のみ実装を検討
- 実装する場合は、拡張メソッドではなく専用のGroundDetectionServiceクラスを検討

---

## 📊 最終成果サマリー（Phase 1のみ完了）

### コード品質の改善

| 項目 | Before | After (Phase 1) | 削減率 |
|------|--------|-----------------|--------|
| 重複コード | 180行 | 約80行 | 約56% |
| SweepBounds実装 | 3箇所 | 1箇所 (SweepBoundsHelper) | 67% |
| WindowStrategies重複 | 4箇所 | 0箇所 (BaseWindowStrategy) | 100% |
| CollisionFilter定数 | 散在 | 1箇所に統一 | - |

### 保守性の向上

- ✅ Sweep Bounds修正: 3箇所 → 1箇所（SweepBoundsHelper）
- ✅ 境界定数管理: 散在 → CollisionFilter.csに統一
- ✅ WindowStrategiesヘルパー: 重複 → BaseWindowStrategyに統一

### パフォーマンス

- ✅ 計算処理の統一化によるキャッシュ効率向上（軽微）
- ⚠️ 大幅な改善はなし（Phase 1は保守性向上が主目的）

---

## 📝 学んだ教訓と今後の指針

### 成功要因（Phase 1完了から）
- ✅ **段階的アプローチ**: 低リスクから開始することで、安全に実装できた
- ✅ **YAGNI原則の適用**: Phase 2-3は実装しないと判断し、過剰な抽象化を回避
- ✅ **明確なスコープ**: Phase 1のみに集中し、約2時間で完了

### 設計判断の教訓
- ✅ **現状維持の価値**: 動作している実装を無理に統一化する必要はない
- ✅ **ROIの重視**: 実装コストに対するメリットを慎重に評価
- ✅ **テスト環境の重要性**: 自動テスト環境がない状態での大規模リファクタリングは高リスク
- ✅ **Fallbackパターンの効果**: DI対応と後方互換性を両立できる優れた設計

### 今後のメンテナンス指針
- **SweepBounds追加**: SweepBoundsHelperに新しいメソッドを追加
- **境界定数変更**: CollisionFilter.csの定数を変更
- **WindowStrategies拡張**: BaseWindowStrategyのヘルパーメソッドを活用
- **CollisionServiceExtensions**: 現状は保持し、将来必要になった場合のみ実装を検討

---

## 🔗 関連ドキュメント

- [D:\DevelopProject\WindowAction\CLAUDE.md](D:\DevelopProject\WindowAction\CLAUDE.md) - プロジェクト仕様書
- [D:\DevelopProject\WindowAction\MultiWindowActionGame\Services\CollisionServiceExtensions.cs](D:\DevelopProject\WindowAction\MultiWindowActionGame\Services\CollisionServiceExtensions.cs) - 実装しないと判断したファイル（プレースホルダーとして保持）

---

**更新履歴**:
- 2025-01-29: 初版作成、Phase 1完了
- 2025-01-30: Phase 2-3を実装しない判断、計画書大幅簡略化（884行→309行）
