#pragma once

#include "../util/typedefs.h"

#include <functional>
#include <type_traits>

#include <nanovg.h>
#include <nanocanvas/NanoCanvas.h>

#include "../util/vec.h"

namespace drawing {

using NanoCanvas::HorizontalAlign;
using NanoCanvas::VerticalAlign;
namespace TextAlign = NanoCanvas::TextAlign;
using NanoCanvas::TextStyle;
using NanoCanvas::Font;
using Winding = NanoCanvas::Canvas::Winding;

struct Point {
  float x, y;

  Point() : Point(0, 0) {}
  Point(float x, float y) : x (x), y (y) {};
  Point(top1::vec v) : x (v.x), y (v.y) {};

  Point rotate(float rad) const {
    float sn = std::sin(rad);
    float cs = std::cos(rad);
    return {
      x * cs - y * sn,
      x * sn + y * cs
    };
  }

  Point swapXY() const {return {y, x};}
  Point flipX() const {return {-x, y};}
  Point flipY() const {return {x, -y};}

  bool operator==(Point rhs) const {return x == rhs.x && y == rhs.y;}
  bool operator!=(Point rhs) const {return x != rhs.x && y != rhs.y;}
  Point operator+(Point rhs) const {return {x + rhs.x, y + rhs.y};}
  Point operator-(Point rhs) const {return {x - rhs.x, y - rhs.y};}
  Point operator*(float s) const {return {x * s, y * s};}
  Point operator/(float s) const {return {x / s, y / s};}
  Point operator-() const {return {-x, -y};}

  operator top1::vec() const {return {x, y};}
};

struct Size {
  float w, h;

  Size() : Size(0, 0) {}
  Size(float w, float h) : w (w), h (h) {};
  Size(top1::vec v) : w (v.x), h (v.y) {};

  Size swapWH() const {return {h, w};}

  Point center() const {
    return {w / 2.f, h / 2.f};
  }
  operator top1::vec() const {return {w, h};}
};

struct Colour {

  byte r;
  byte g;
  byte b;
  byte a = 0xFF;

  Colour(float, float, float, float a = 1);
  Colour(uint32 data) {
    r = byte((data >> 16) & 0x0000FF);
    g = byte((data >> 8) & 0x0000FF);
    b = byte((data) & 0x0000FF);
    a = 0xFF;
  };
  Colour();

  Colour mix(Colour c, float ratio) const;
  Colour dim(float amount) const;
  Colour brighten(float amount) const;

  operator NanoCanvas::Color() const {
    return NanoCanvas::Color(r, g, b, a);
  }
};

inline Colour::Colour(float r, float g, float b, float a) :
  r (r * 255), g (g * 255), b (b * 255)  {}


inline Colour::Colour() : Colour(244,0,0) {}

inline Colour Colour::mix(Colour c, float ratio) const {
  Colour ret;
  ret.r = top1::withBounds<byte>(0, 255, r + (c.r - r ) * ratio);
  ret.g = top1::withBounds<byte>(0, 255, g + (c.g - g ) * ratio);
  ret.b = top1::withBounds<byte>(0, 255, b + (c.b - b ) * ratio);
  ret.a = top1::withBounds<byte>(0, 255, a + (c.a - a ) * ratio);
  return ret;
}

inline Colour Colour::dim(float amount) const {
  float dim = 1 - amount;
  Colour ret;
  ret.r = top1::withBounds<byte>(0, 255, r * dim);
  ret.g = top1::withBounds<byte>(0, 255, g * dim);
  ret.b = top1::withBounds<byte>(0, 255, b * dim);
  ret.a = a;
  return ret;
}

inline Colour Colour::brighten(float amount) const {
  Colour ret;
  ret.r = top1::withBounds<byte>(0, 255, r + (255 - r) * amount);
  ret.g = top1::withBounds<byte>(0, 255, g + (255 - g) * amount);
  ret.b = top1::withBounds<byte>(0, 255, b + (255 - b) * amount);
  ret.a = a;
  return ret;
}

struct MainColour : public Colour {

  const Colour dimmed;

  MainColour(Colour basic) :
    Colour (basic),
    dimmed (basic.dim(0)) {}

  MainColour(Colour basic, Colour dimmed) :
    Colour (basic),
    dimmed (dimmed) {}

  MainColour(uint32 basic) :
    Colour (basic),
    dimmed (dim(0.1)) {}

  MainColour(uint32 basic, uint32 dimmed) :
    Colour (basic),
    dimmed (dimmed) {}

  using Colour::operator NanoCanvas::Color;
};

class Canvas; // FWDCL

/**
 * Anything that can be drawn on screen.
 * Holds a pointer to its parent
 */
class Drawable {
public:

  Drawable() {};

  /**
   * Draw this widget to the context.
   * Called from the parent's draw method.
   * @param ctx the canvas to draw on.
   */
  virtual void draw(Canvas& ctx) = 0;

};

class SizedDrawable : public Drawable {
protected:
public:
  Size size;

  SizedDrawable() {};
  SizedDrawable(Size s) : size (s) {};

};

/**
 * Eventually we will get rid of NanoVG,
 * but we want to keep the canvas interface
 */
class Canvas : public NanoCanvas::Canvas {
public:
  using Super = NanoCanvas::Canvas;

  // Enums
  using Super::Winding;
  using Super::LineCap;
  using Super::LineJoin;

  Canvas(NVGcontext* ctx, Size size, float scaleRatio = 1.0f) :
    Canvas(ctx, size.w, size.h, scaleRatio) {}

  Canvas(NVGcontext* ctx, float width, float height, float scaleRatio = 1.0f) :
    Super(ctx, width, height, scaleRatio) {}

  using Super::moveTo;
  Canvas& moveTo(Point p) {
    Super::moveTo(p.x, p.y);
    return *this;
  }

  using Super::lineTo;
  Canvas& lineTo(Point p) {
    Super::lineTo(p.x, p.y);
    return *this;
  }

  using Super::arcTo;
  Canvas& arcTo(Point p1, Point p2, float r) {
    Super::arcTo(p1.x, p1.y, p2.x, p2.y, r);
    return *this;
  }

  using Super::quadraticCurveTo;
  Canvas& quadraticCurveTo(Point control, Point end) {
    Super::quadraticCurveTo(control.x, control.y, end.x, end.y);
    return *this;
  }

  using Super::bezierCurveTo;
  Canvas& bezierCurveTo(Point cp1, Point cp2, Point end) {
    Super::bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
    return *this;
  }

  using Super::arc;
  Canvas& arc(Point cp,float r,
   float sAngle,float eAngle,bool counterclockwise = false) {
    Super::arc(cp.x, cp.y, r, sAngle, eAngle, counterclockwise);
    return *this;
  }

  using Super::rect;
  Canvas& rect(Point p, Size s) {
    Super::rect(p.x, p.y, s.w, s.h);
    return *this;
  }

  using Super::roundedRect;
  Canvas& roundedRect(Point p, Size s,float r) {
    Super::roundedRect(p.x, p.y, s.w, s.h, r);
    return *this;
  }

  using Super::circle;
  Canvas& circle(Point p, float r) {
    Super::circle(p.x, p.y, r);
    return *this;
  }

  using Super::ellipse;
  Canvas& ellipse(Point p, float rx, float ry) {
    Super::ellipse(p.x, p.y, rx, ry);
    return *this;
  }


  using Super::clearColor;
  Canvas& clearColor(const Colour& color) {
    Super::clearColor(color);
    return *this;
  }


  using Super::fillText;
  Canvas& fillText(const std::string& text, Point p, float rowWidth = NAN) {
    Super::fillText(text, p.x, p.y, rowWidth);
    return *this;
  }

  using Super::textAlign;

  using Super::fillStyle;
  Canvas& fillStyle(const Colour& color) {
    Super::fillStyle(color);
    return *this;
  }

  using Super::strokeStyle;
  Canvas& strokeStyle(const Colour& color) {
    Super::strokeStyle(color);
    return *this;
  }

  using Super::fill;
  Canvas& fill(const Colour& color) {
    Super::fillStyle(color);
    Super::fill();
    return *this;
  }

  using Super::stroke;
  Canvas& stroke(const Colour& color) {
    Super::strokeStyle(color);
    Super::stroke();
    return *this;
  }


  using Super::translate;
  Canvas& translate(Point p) {
    Super::translate(p.x, p.y);
    return *this;
  }

  Canvas& rotateAround(float r, Point p) {
    translate(p);
    rotate(r);
    translate(-p);
  }

  Canvas& draw(Drawable &d) {
    d.draw(*this);
    return *this;
  }

  Canvas& drawAt(Point p, Drawable &d) {
    save();
    translate(p);
    d.draw(*this);
    restore();
    return *this;
  }

  Canvas& callAt(Point p, const std::function<void(void)>& f) {
    save();
    translate(p);
    f();
    restore();
    return *this;
  }

  template<typename It>
  Canvas& bzCurve(It pointB, It pointE, float f = 0.5, float t = 1) {

    moveTo(*pointB);

    float m = 0;
    Point d1;

    Point prev = *pointB;
    Point cur = prev;
    Point next = prev;
    for (auto it = pointB; it != pointE; ++it) {
      prev = cur;
      cur = next;
      next = *it;
      if (cur == prev) continue;
      m = (next.y - prev.y) / (next.x - prev.x);
      Point d2 = {
        (next.x - cur.x) * -f,
        0
      };
      d2.y = d2.x * m * t;
      bezierCurveTo(prev - d1, cur + d2, cur);
      d1 = d2;
    }
    return *this;
  }

  template<typename It>
  Canvas& roundedCurve(It pointB, It pointE, float maxR = -1) {

    maxR = maxR < 0 ? std::numeric_limits<float>::max() : maxR;
    moveTo(*pointB);

    Point cur = *pointB;
    Point nxt = cur;
    auto it = pointB;
    for (auto it = pointB; it < pointE; ++it) {
      cur = nxt;
      nxt = *it;
      if (nxt == cur) continue;
      float rx = std::abs(nxt.x - cur.x) / 2.0;
      float ry = std::abs(nxt.y - cur.y) / 2.0;
      float r = std::min(std::min(rx, ry), maxR);
      Point md = (cur + nxt) / 2.0;
      Point cp1 = {md.x, cur.y};
      Point cp2 = md;
      Point cp3 = {md.x, nxt.y};

      arcTo(cp1, cp2, r);
      arcTo(cp3, nxt, r);
    }
    return *this;
  }

  // Debug

  Canvas& debugDot(Point p, Colour c = 0xFFFF00) {
    beginPath();
    circle(p, 1);
    fill(c);
    return *this;
  }
};

}
