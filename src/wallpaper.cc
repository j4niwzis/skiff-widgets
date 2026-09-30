export module skiff.widgets.wallpaper;

import std;
import skia;
import splice;
import skiff.paint;
import skiff.scene;

export namespace skiff::widgets {

// A wallpaper's pattern, as the program read it: shapes made of steps in
// absolute points, in a box of the pattern's own size -- an SVG's paths,
// say. All the shapes one after another, each begun by a move.
namespace pattern_step {
struct move {
  float x = 0.0f, y = 0.0f;
};
struct line {
  float x = 0.0f, y = 0.0f;
};
struct cubic {
  float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f, x = 0.0f, y = 0.0f;
};
struct close {};
} // namespace pattern_step
using PatternStep = splice::variant<pattern_step::move, pattern_step::line, pattern_step::cubic, pattern_step::close>;
struct Pattern {
  float width = 0.0f;
  float height = 0.0f;
  std::vector<PatternStep> steps;
};

// A chat's wallpaper, as Telegram's: a gradient and over it a pattern in a
// colour of its own -- or a picture of the user's -- scaled to cover it and
// centred; or nothing, what is behind it showing (a plain colour). The
// pattern or the picture is drawn once for a size, into pixels kept: what is
// behind a list is repainted at every step of a scroll.
class Wallpaper : public skiff::scene::Node {
public:
  void setGradient(std::optional<skiff::scene::Gradient> gradient) {
    if (gradient == fGradient) {
      return;
    }
    fGradient = gradient;
    this->markDamaged();
  }
  void setPattern(std::shared_ptr<const Pattern> pattern, skia::SkColor colour) {
    if (pattern == fPattern && colour == fColour) {
      return;
    }
    fPattern = std::move(pattern);
    fColour = colour;
    fDrawn = nullptr;
    this->markDamaged();
  }
  // A picture in place of the gradient and the pattern; none, none.
  void setPicture(skia::Sp<skia::SkImage> picture) {
    if (picture == fPicture) {
      return;
    }
    fPicture = std::move(picture);
    fDrawn = nullptr;
    this->markDamaged();
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    const skia::SkRect &box = fState.fBounds;
    if (box.isEmpty()) {
      return;
    }
    if (fGradient && !fPicture) {
      skiff::paint::verticalGradient(canvas, box, fGradient->top, fGradient->bottom, alpha);
    }
    const bool patterned = fPattern && fPattern->width > 0.0f && fPattern->height > 0.0f && !fPattern->steps.empty();
    if (!fPicture && !patterned) {
      return;
    }
    // At the device's pixels, where the canvas is scaled for them.
    const float scale = std::max(1.0f, canvas->getTotalMatrix().getScaleX());
    const int width = static_cast<int>(std::ceil(box.width() * scale));
    const int height = static_cast<int>(std::ceil(box.height() * scale));
    if (!fDrawn || fDrawnWidth != width || fDrawnHeight != height) {
      fDrawn = this->drawn(width, height);
      fDrawnWidth = width;
      fDrawnHeight = height;
    }
    if (!fDrawn) {
      return;
    }
    skia::SkPaint paint;
    paint.setAlphaf(alpha);
    canvas->drawImageRect(fDrawn, box, skia::SkSamplingOptions(skia::SkFilterMode::kLinear), &paint);
  }

private:
  // The picture, or the pattern, in pixels, width by height: covering them,
  // centred.
  [[nodiscard]] skia::Sp<skia::SkImage> drawn(int width, int height) const {
    skia::SkBitmap bitmap;
    if (!bitmap.tryAllocN32Pixels(width, height)) {
      return nullptr;
    }
    bitmap.eraseColor(0);
    skia::SkCanvas into(bitmap);
    if (fPicture) {
      const float w = static_cast<float>(fPicture->width()), h = static_cast<float>(fPicture->height());
      if (w <= 0.0f || h <= 0.0f) {
        return nullptr;
      }
      const float cover = std::max(static_cast<float>(width) / w, static_cast<float>(height) / h);
      const skia::SkRect at = skia::SkRect::MakeXYWH((static_cast<float>(width) - w * cover) * 0.5f,
                                                    (static_cast<float>(height) - h * cover) * 0.5f, w * cover, h * cover);
      into.drawImageRect(fPicture, at, skia::SkSamplingOptions(skia::SkFilterMode::kLinear, skia::SkMipmapMode::kLinear));
      return bitmap.asImage();
    }
    const float cover = std::max(static_cast<float>(width) / fPattern->width, static_cast<float>(height) / fPattern->height);
    into.translate((static_cast<float>(width) - fPattern->width * cover) * 0.5f,
                   (static_cast<float>(height) - fPattern->height * cover) * 0.5f);
    into.scale(cover, cover);
    skia::SkPathBuilder shapes;
    for (const PatternStep &step : fPattern->steps) {
      splice::visit(splice::overloaded{[&](const pattern_step::move &one) { shapes.moveTo(one.x, one.y); },
                                       [&](const pattern_step::line &one) { shapes.lineTo(one.x, one.y); },
                                       [&](const pattern_step::cubic &one) {
                                         shapes.cubicTo(one.x1, one.y1, one.x2, one.y2, one.x, one.y);
                                       },
                                       [&](const pattern_step::close &) { shapes.close(); }},
                    step);
    }
    skia::SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(fColour);
    into.drawPath(shapes.detach(), paint);
    return bitmap.asImage();
  }

  std::optional<skiff::scene::Gradient> fGradient;
  skia::Sp<skia::SkImage> fPicture;
  std::shared_ptr<const Pattern> fPattern;
  skia::SkColor fColour = 0;
  skia::Sp<skia::SkImage> fDrawn;
  int fDrawnWidth = 0;
  int fDrawnHeight = 0;
};

} // namespace skiff::widgets
