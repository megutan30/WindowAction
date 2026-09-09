#ifndef PLAYER_H
#define PLAYER_H

#include <windows.h>
#include "animation.h"

/* C++移行フェーズ4: 元のC#版(PlayerPhysics.cs/PlayerWindowInteraction.cs/
   PlayerInputHandler.cs/PlayerStateMachine.cs)と同じ4つの関心事に分割する。
   ネイティブ移植の初期段階ではこれらを全部Player構造体に平坦化していたが、
   今回のC++移行でC#版と同じ構成に戻す。外部ファイル(collision.cpp/
   editor.cpp/gamewindow.cpp/hierarchy.cpp)からの直接フィールドアクセスは
   事前調査でparentIdx/isMinimized/inheritedFlipX/Yの3種類しか無いことを
   確認済みで、GameWindowDataも同名のparentIdxフィールドを持つため、
   移行時にはp->parentIdxのような呼び出し元テキストを機械的に置換するのでは
   なく、変数がPlayer*であることを個別に確認しながら移した
   （GameWindowDataのparentIdx/hwndとの取り違えを防ぐため）。 */

struct PlayerPhysics
{
    float x, y;   /* 左上座標、スクリーン座標系（滑らかな物理演算のためfloat） */
    int width, height; /* 現在のサイズ -- 可変: リサイズされる親に合わせてスケーリングされる */
    float vy;
    int grounded;
    int facingRight;
};

struct PlayerWindowInteraction
{
    /* Player_ApplyParentRelativeTransform専用: 直近にこの関数を適用した時点の
       親の可視矩形。サイズ・位置ともに、ジェスチャー開始時点ではなくこちらを
       フレームごとの基準にする -- そうしないと、(1)リサイズ中にプレイヤー
       自身が歩いて動いた分が次のフレームで「ジェスチャー開始時点からの
       再計算」により上書きされて戻されてしまう、(2)既にある程度リサイズが
       進行した状態のウィンドウへ途中から入った場合、入った瞬間に「ジェス
       チャー開始時点からずっとそこにいたかのような」倍率が一気に掛かって
       サイズ・位置が瞬間的にジャンプしてしまう、という2つの不具合が起きる。
       フレームごとの差分だけを積み重ねることで、歩行による移動と、いつ
       入ってきても連続的なリサイズ追従を両立させる。 */
    RECT lastAppliedParentRect;
    /* lastAppliedParentRect がどのウィンドウ（インデックス）についての
       ものかを記録する。この値が現在の親と一致する限り、新しいリサイズ
       ジェスチャーが始まってもlastAppliedParentRectを破棄せず引き続き基準
       として使う -- 一致しない場合（プレイヤーが行き来した、または初めて
       この親に入った）のみ、現在の矩形をその場で新しい基準として確立し
       直す（Player_ApplyParentRelativeTransform参照）。 */
    int lastAppliedParentIdx;
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
};

/* 最小化の縮小・拡大アニメーション状態(GameWindowData.minimizeAnimState等と
   同じ仕組み)。0=無し、1=縮小中、2=復元中。Player_StartMinimizeAnim/
   Player_UpdateMinimizeAnim参照。C#版に「アニメーション」専用のクラスは
   無いが、PlayerStateMachineとは別の関心事(見た目のトランジションであり
   ゲームプレイ状態ではない)のため独立させている。 */
struct PlayerMinimizeAnim
{
    int minimizeAnimState;
    float minimizeAnimT;
    RECT minimizeAnimFrom;
    RECT minimizeAnimTo;
    /* GameWindowData.iconicBitmapと同じ仕組み: 縮小アニメーションを開始する
       直前にフルサイズの見た目を1回だけキャプチャしておいたもの。DWMの
       タスクバーサムネイル/ライブプレビュー用（PlayerWindowProcの
       WM_DWMSENDICONICTHUMBNAIL/WM_DWMSENDICONICLIVEPREVIEWBITMAP参照）。 */
    HBITMAP iconicBitmap;
};

/* PlayerInputHandler.cs相当。キーボード状態を毎回読み直すだけでフィールドを
   一切持たない(C#版も同様にステートレス)。 */
struct PlayerInputHandler
{
    bool ShouldJump() const;
    bool IsMovingLeft() const;
    bool IsMovingRight() const;
};

/* PlayerStateMachine.cs相当。ネイティブ移植はStateを専用フィールドとして
   保持せず、必要な箇所でvy/groundedから都度導出する設計を最初から採って
   いたため、ここでも新しいフィールドは追加せず、導出ロジックだけを
   ひとまとめにする。 */
struct PlayerStateMachine
{
    enum class State
    {
        Normal,
        Jumping,
        Falling,
        Grounded
    };
    static State GetState(float vy, int grounded);
};

typedef struct {
    PlayerPhysics physics;
    PlayerWindowInteraction windowInteraction;
    PlayerMinimizeAnim minimizeAnim;
    PlayerInputHandler inputHandler;
    PlayerStateMachine stateMachine;
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

/* プレイヤーの現在の親が `windowIndex` の場合、`newRect`（現在フレームでの
   親の可視矩形）と直近にこの関数を適用した時点の矩形(lastAppliedParentRect)
   との差分に合わせて、サイズと相対位置の両方を追従させる。通常の子
   ウィンドウ(Hierarchy_ApplyRelativeTransform)と同じ「親に対する相対位置・
   相対サイズを保つ」挙動を、通常のResizableと制限なしリサイズの両方の子
   であるプレイヤーにも適用する。最終的にウィンドウの新しい境界内に収まる
   よう位置を再クランプする。 */
void Player_ApplyParentRelativeTransform(Player *p, int windowIndex, RECT newRect);

/* 制限なしリサイズウィンドウが反転した瞬間に一度だけ呼ぶ。Player_
   ApplyParentRelativeTransformの正スケール比だけの追従では反転（親矩形の
   左上そのものが動く/入れ替わる）を正しく表現できず、プレイヤーが見た目上
   移動したタイトルバー側にめり込む不具合があった。`parentBounds`（反転を
   反映済みの現在の親矩形）を軸に、mirrorX/mirrorYで指定された軸について
   プレイヤーの位置を正しく鏡映する。 */
void Player_MirrorWithinParent(Player *p, int windowIndex, RECT parentBounds, int mirrorX, int mirrorY);

/* プレイヤーをフリーズさせ（Player_Update が何もしなくなる）、現在の親から
   切り離し、後の Player_OnRestore のために記憶しておく。プレイヤーが乗っている
   ウィンドウが最小化されたときに呼ばれる PlayerForm.OnMinimize を反映。 */
void Player_OnMinimize(Player *p);

/* 乗っているウィンドウの最小化アニメーションが開始した瞬間に一度だけ呼ぶ
   （SetWindowMinimized参照）。GameWindowの縮小アニメーションと同じく、
   画面下端(タスクバー方向)へ向かって縮んでいく見た目のアニメーションを
   開始する。プレイヤーの通常の物理演算/移動更新はここで即座に停止する
   （isMinimized=1、Player_Update先頭のガード）が、parentIdxの記録や実際の
   ウィンドウの非表示化は行わない -- それらはアニメーション完了時に呼ばれる
   Player_OnMinimizeが引き続き担当する。 */
void Player_StartMinimizeAnim(Player *p);

/* Player_StartMinimizeAnimで開始した縮小アニメーションを1フレーム分進める。
   isMinimizedの値に関わらず（Player_Updateとは別経路で）毎フレーム呼ぶ
   こと。アニメーション中でなければ即座に戻る。 */
void Player_UpdateMinimizeAnim(Player *p, float dt);

/* プレイヤーのフリーズを解除する。記憶していた親がまだ有効で、最小化されておらず、
   重なりも維持していればそこに再アタッチする。そうでなければプレイヤーの現在位置に
   あるウィンドウを新たに採用する。PlayerForm.OnRestore を反映。 */
void Player_OnRestore(Player *p);

#endif
