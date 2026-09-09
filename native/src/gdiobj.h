#ifndef GDIOBJ_H
#define GDIOBJ_H

#include <windows.h>

/* C++移行フェーズ1: GDIリソース(HPEN/HBRUSH等)のCreate/Delete手動ペアを
   RAIIに置き換えるための軽量ラッパー。挙動は従来の
   「Create→(Select→描画→元のオブジェクトへSelect戻す)→Delete」と
   完全に同一で、デストラクタで自動的に後始末するだけ。 */

class GdiPen
{
public:
    GdiPen(int style, int width, COLORREF color) : h_(CreatePen(style, width, color)) {}
    ~GdiPen()
    {
        if (h_)
            DeleteObject(h_);
    }
    GdiPen(const GdiPen &) = delete;
    GdiPen &operator=(const GdiPen &) = delete;
    operator HPEN() const { return h_; }

private:
    HPEN h_;
};

class GdiBrush
{
public:
    explicit GdiBrush(COLORREF color) : h_(CreateSolidBrush(color)) {}
    ~GdiBrush()
    {
        if (h_)
            DeleteObject(h_);
    }
    GdiBrush(const GdiBrush &) = delete;
    GdiBrush &operator=(const GdiBrush &) = delete;
    operator HBRUSH() const { return h_; }

private:
    HBRUSH h_;
};

/* SelectObjectで選んだGDIオブジェクトを、スコープを抜ける際に自動的に
   元のオブジェクトへ戻す。オブジェクト自体の所有権は持たない
   (DeleteObjectは呼ばない) -- GdiPen/GdiBrush側のデストラクタが担当する。 */
class ScopedSelectObject
{
public:
    ScopedSelectObject(HDC hdc, HGDIOBJ obj) : hdc_(hdc), old_(SelectObject(hdc, obj)) {}
    ~ScopedSelectObject() { SelectObject(hdc_, old_); }
    ScopedSelectObject(const ScopedSelectObject &) = delete;
    ScopedSelectObject &operator=(const ScopedSelectObject &) = delete;

private:
    HDC hdc_;
    HGDIOBJ old_;
};

/* GdiPen/GdiBrushのように専用のコンストラクタ引数を持たない、その他の
   GDIオブジェクト(HFONT/HBITMAP等)向けの汎用ラッパー。既に生成済みの
   ハンドルを受け取って所有権だけ持つ(呼び出し側はCreateFontA/
   CreateCompatibleBitmap等をそのまま使い、結果をここに渡すだけでよい)。 */
template <typename T>
class GdiHandle
{
public:
    explicit GdiHandle(T h) : h_(h) {}
    ~GdiHandle()
    {
        if (h_)
            DeleteObject(h_);
    }
    GdiHandle(const GdiHandle &) = delete;
    GdiHandle &operator=(const GdiHandle &) = delete;
    operator T() const { return h_; }

private:
    T h_;
};

/* CreateCompatibleDC/DeleteDCのRAIIラッパー。GDIオブジェクトではなくDCその
   ものなのでDeleteObjectではなくDeleteDCを呼ぶ点がGdiHandle<T>と異なる。 */
class ScopedCompatibleDC
{
public:
    explicit ScopedCompatibleDC(HDC hdcRef) : h_(CreateCompatibleDC(hdcRef)) {}
    ~ScopedCompatibleDC()
    {
        if (h_)
            DeleteDC(h_);
    }
    ScopedCompatibleDC(const ScopedCompatibleDC &) = delete;
    ScopedCompatibleDC &operator=(const ScopedCompatibleDC &) = delete;
    operator HDC() const { return h_; }

private:
    HDC h_;
};

/* GetDC(hwnd)/ReleaseDC(hwnd, dc)のRAIIラッパー。ウィンドウDC(または
   GetDC(NULL)の画面DC)はCreateCompatibleDCで作ったDCと違い解放にhwndも
   必要なため別クラスにしている。 */
class ScopedWindowDC
{
public:
    explicit ScopedWindowDC(HWND hwnd) : hwnd_(hwnd), h_(GetDC(hwnd)) {}
    ~ScopedWindowDC()
    {
        if (h_)
            ReleaseDC(hwnd_, h_);
    }
    ScopedWindowDC(const ScopedWindowDC &) = delete;
    ScopedWindowDC &operator=(const ScopedWindowDC &) = delete;
    operator HDC() const { return h_; }

private:
    HWND hwnd_;
    HDC h_;
};

#endif
