#ifndef EDITOR_H
#define EDITOR_H

/* ステージエディター（テストステージ）。開発ビルド(build.bat dev、
   /DENABLE_STAGE_EDITOR)でのみ中身が存在する -- 本番ビルドでは
   このヘッダをincludeしても何も宣言されない（呼び出し側も
   #ifdef ENABLE_STAGE_EDITORで囲むこと）。 */
#ifdef ENABLE_STAGE_EDITOR

#include <windows.h>
#include "gamewindow.h"

/* タイトル画面の「Test」ボタンが押されたときに1を立てるリクエストフラグ。
   g_requestNext等（strategy.c）と同じパターン。 */
extern int g_requestTest;

/* 配置パレット+ツールバーからなる空のテストステージを作る（背景用の
   全画面ウィンドウは持たない -- 本物のデスクトップがそのまま見える/床に
   なる）。Stage_Loadとは完全に別経路（TOTAL_STAGESの範囲外の概念）で、
   通常のNext/Retry進行には一切巻き込まれない。 */
void Editor_LoadTestStage(HINSTANCE hInstance);

/* 現在テストステージ中かどうか。CheckGoal等、通常のステージ進行ロジックを
   スキップすべき箇所で参照する。 */
int Editor_IsTestStage(void);

/* パレットアイコン(index)のドラッグを開始する（WM_LBUTTONDOWN）。 */
void Editor_StartPaletteDrag(int index);

/* ドラッグ中の全パレットアイコンをカーソル位置へ追従させる。毎フレーム
   1回呼び出すこと。 */
void Editor_UpdatePaletteDrags(void);

/* パレットアイコン(index)のドラッグを終了する（WM_LBUTTONUP）。パレット列の
   外にドロップされていれば、そのアイコンが表す種別の実サイズウィンドウを
   ドロップ位置へ生成する。アイコン自身は常にホームポジションへ戻す。 */
void Editor_EndPaletteDrag(int index);

/* 現在配置されている内容（パレット/ツールバー自身は除く）を、stage.cへ
   貼り付け可能なCreateGameWindow呼び出し列としてstage_export.txt
   （カレントディレクトリ）へ書き出す。 */
void Editor_ExportStage(void);

/* テストステージ中のみ有効: Deleteキーが押された瞬間（押しっぱなしでの
   連続削除を避けるためエッジ検出）、その時点のマウスカーソル直下にある
   配置済みウィンドウ（パレットアイコン/ツールバーボタン自身を除く、
   種別を問わない）を1つ削除する。毎フレーム1回呼び出すこと。 */
void Editor_HandleDeleteInput(void);

#endif /* ENABLE_STAGE_EDITOR */

#endif
