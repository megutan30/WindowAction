#ifndef PLAYER_H
#define PLAYER_H

#include <windows.h>
#include "animation.h"

typedef struct {
    float x, y;   /* 左上座標、スクリーン座標系（滑らかな物理演算のためfloat） */
    int width, height; /* 現在のサイズ -- 可変: リサイズされる親に合わせてスケーリングされる */
    SIZE origSize; /* スケール基準サイズ -- origSizeGen を参照 */
    int origSizeGen; /* origSize が最後に確定した時点の g_resizeGeneration の値 */
    float vy;
    int grounded;
    int facingRight;
    int parentIdx; /* -1 = どのウィンドウの内部にもいない（C#のPlayerForm.Parentに対応） */
    int isMinimized; /* フリーズ状態: 親だったウィンドウが最小化された */
    int lastValidParentIdx; /* Player_OnRestore のために記憶しておく、lastValidParent に対応 */
    /* GameWindowData.inheritedFlipX/Yと同じ仕組み(Hierarchy_ToggleInheritedFlip
       参照): 制限なしリサイズウィンドウの子として乗っている間にその祖先が
       反転する「たびに」XORで積算される永続フラグ。親から離れても値は
       保持され、再度どこかの祖先が反転するまで変化しない。inheritedFlipYは
       見た目のミラー描画だけでなく、重力とジャンプの向きの反転にも使う
       （上下逆さの床/天井の上に立つ）。inheritedFlipXは見た目のみ。 */
    int inheritedFlipX, inheritedFlipY;
    HWND hwnd;
    PlayerAnimation anim;
} Player;

#define PLAYER_SIZE 60
#define PLAYER_MIN_SIZE 5 /* GetMinimumSize() として使われる Form.MinimumSize(5,5) に対応 */

void RegisterPlayerWindowClass(HINSTANCE hInstance);
HWND CreatePlayerWindow(HINSTANCE hInstance, Player *p, int startX, int startY);
void Player_Reset(Player *p, int startX, int startY);
void Player_Update(Player *p, float dt);
void Player_GetBounds(const Player *p, RECT *out);
void Player_FollowParentMove(Player *p, int parentIdx, int dx, int dy);
Player *Player_GetActive(void);

/* ステージ読み込み直後/プレイヤーリセット直後にプレイヤーの初期親を割り当てる。
   StageManager.InitializePlayer/FinalizeStageSetup を反映: 次フレームの移動に
   よる遷移を待つのではなく、プレイヤーをクライアント領域（タイトルバーを除く）に
   完全に含む、最も前面（最高Z-order）のウィンドウを選ぶ。 */
void Player_AssignInitialParent(Player *p);

/* プレイヤーの現在の親が `windowIndex` の場合、origSize
   （現在のリサイズ世代に対して Hierarchy_ApplyScale が遅延確定させたもの）から
   再スケーリングし、ウィンドウの新しい境界内に位置を再クランプする。
   PlayerForm.ApplyEffect(ResizeEffect) + AdjustPositionAfterResize を反映。 */
void Player_ApplyParentScale(Player *p, int windowIndex, float scaleX, float scaleY);

/* プレイヤーをフリーズさせ（Player_Update が何もしなくなる）、現在の親から
   切り離し、後の Player_OnRestore のために記憶しておく。プレイヤーが乗っている
   ウィンドウが最小化されたときに呼ばれる PlayerForm.OnMinimize を反映。 */
void Player_OnMinimize(Player *p);

/* プレイヤーのフリーズを解除する。記憶していた親がまだ有効で、最小化されておらず、
   重なりも維持していればそこに再アタッチする。そうでなければプレイヤーの現在位置に
   あるウィンドウを新たに採用する。PlayerForm.OnRestore を反映。 */
void Player_OnRestore(Player *p);

#endif
