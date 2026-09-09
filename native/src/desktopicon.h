#ifndef DESKTOPICON_H
#define DESKTOPICON_H

#include <windows.h>

/* DesktopIconManager.cs / RegistryIconPositionWatcher.csからの移植。
   SHChangeNotifyRegister（作成/削除/名前変更/更新）とレジストリ監視スレッド
   （移動、HKCU\...\Shell\BagsのRegNotifyChangeKeyValue）の両方によるイベント
   駆動で追従する。アイコンの名前取得（LVM_GETITEMTEXT）は行わない
   （ゲームプレイ上は矩形の当たり判定だけが必要なため）。 */
#define MAX_DESKTOP_ICONS 64

/* "WA_DesktopIconMarker"ウィンドウクラスを登録する。DesktopIcon_Refreshを
   呼ぶ前に起動時に一度だけ呼ぶこと。 */
void DesktopIcon_RegisterWindowClass(HINSTANCE hInstance);

/* デスクトップの実際のアイコン矩形（アイコン画像部分、LVIR_ICON基準）を
   スクリーン座標で取得し直し、可視化用の枠線マーカーを再生成する。
   Progman -> SHELLDLL_DefView -> SysListView32の通常経路と、それが
   見つからない場合のWorkerW経由の代替探索の両方を試す
   （DesktopIconManager.GetDesktopListViewHandle/TryAlternativeDesktopSearch
   と一致）。取得に失敗した場合は既存のアイコンを全て破棄し0件になる。 */
void DesktopIcon_Refresh(HINSTANCE hInstance);

/* 可視化マーカーを全て破棄し、アイコン一覧をクリアする。 */
void DesktopIcon_Clear(void);

int DesktopIcon_Count(void);

/* index番目のアイコンのスクリーン座標での矩形。範囲外なら全て0のRECTを返す。 */
RECT DesktopIcon_GetBounds(int index);

/* 現在のステージ/テストモードでデスクトップアイコンを有効とすべきか
   （Stage_DesktopIconsEnabled、またはENABLE_STAGE_EDITORビルドでのテスト
   モード中）。player.cの接地判定とDesktopIcon_UpdatePollingの両方が
   同じ条件を参照するため、判定はここに一本化してある。 */
int DesktopIcon_IsActiveForCurrentStage(void);

/* Shell変更通知の受信ウィンドウ登録とレジストリ監視スレッドの起動。
   DesktopIcon_RegisterWindowClassと同様、起動時に一度だけ呼ぶこと
   （WinMain参照）。 */
void DesktopIcon_InitEventWatchers(HINSTANCE hInstance);

/* メインループから毎フレーム呼ぶ。Shell変更通知またはレジストリ監視
   スレッドが変更を検知していれば（内部の共有フラグ経由）、この呼び出しの
   中でDesktopIcon_Refreshを実行する -- クロスプロセスAPI呼び出しを伴う
   Refresh自体は通知元のスレッドではなく必ずメインスレッドから行う。
   無効なステージでは何もしない（アイコンのクリア自体はLoadStage側の
   DesktopIcon_Clear呼び出しに任せる）。 */
void DesktopIcon_UpdateEvents(HINSTANCE hInstance);

#endif
