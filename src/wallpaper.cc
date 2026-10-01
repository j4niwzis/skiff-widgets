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
    this->keepOld();
    fGradient = gradient;
    fBlurred = nullptr;
    fExtra.clear();
    this->markDamaged();
  }
  void setPattern(std::shared_ptr<const Pattern> pattern, skia::SkColor colour) {
    if (pattern == fPattern && colour == fColour) {
      return;
    }
    this->keepOld();
    fPattern = std::move(pattern);
    fColour = colour;
    fDrawn = nullptr;
    fBlurred = nullptr;
    fExtra.clear();
    this->markDamaged();
  }
  // How opaque all of it is drawn: a window see-through as a whole shows
  // the desktop through its chat's background too. Taken as the paint's
  // alpha of what it draws already -- no layer.
  void setOpacity(float opacity) {
    if (opacity == fOpacity) {
      return;
    }
    fOpacity = opacity;
    this->markDamaged();
  }
  // How much the frost blurs, from 0 (not at all) to 1 (about five screen
  // pixels): the copy made smaller the more it blurs -- smoothly, not in
  // whole pixels of a radius -- and blurred there by one. Made again once,
  // where it changes.
  void setBlur(float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (amount == fAmount) {
      return;
    }
    fAmount = amount;
    fBlurred = nullptr;
    fExtra.clear();
    this->markDamaged();
  }
  // The other blurs asked of the backdrop (0 to 1), besides its own: what
  // frosts by an amount of its own -- bubbles, panels, a chat's lines --
  // finds it made, once for a size, as the backdrop's own is.
  void setBlurs(std::vector<float> amounts) {
    for (float &each : amounts) {
      each = std::clamp(each, 0.0f, 1.0f);
    }
    std::ranges::sort(amounts);
    const auto [first, last] = std::ranges::unique(amounts);
    amounts.erase(first, last);
    if (amounts == fAmounts) {
      return;
    }
    fAmounts = std::move(amounts);
    fExtra.clear();
    this->markDamaged();
  }
  // A picture in place of the gradient and the pattern; none, none.
  void setPicture(skia::Sp<skia::SkImage> picture) {
    if (picture == fPicture) {
      return;
    }
    this->keepOld();
    fPicture = std::move(picture);
    fDrawn = nullptr;
    fBlurred = nullptr;
    fExtra.clear();
    this->markDamaged();
  }

  // A change of look -- another chat's background -- faded across from the
  // look before: what that drew kept (its gradient, its picture or pattern
  // as drawn), drawn going as the new one comes. The first change of a frame
  // keeps it; the setters after it, the same change, keep nothing more.
  void keepOld() {
    if (fKeptThisFrame) {
      return;
    }
    fKeptThisFrame = true;
    fOldGradient = fGradient && !fPicture ? fGradient : std::nullopt;
    fOldDrawn = fDrawn;
    fFade.jump(0.0f);
    fFade.setTarget(1.0f);
    skiff::scene::work::mark(fState.fId);
  }
  [[nodiscard]] bool wantsTick() const { return fFade.moving(); }
  [[nodiscard]] bool settling() const { return fFade.moving(); }
  void update(double nowMs) {
    if (fFade.step(nowMs)) {
      this->markDamaged();
    }
    if (!fFade.moving()) {
      fOldGradient.reset();
      fOldDrawn = nullptr;
    }
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    fKeptThisFrame = false;
    const skia::SkRect &box = fState.fBounds;
    if (box.isEmpty()) {
      return;
    }
    alpha *= fOpacity;
    // The look going, under the one coming.
    const float coming = fFade.value();
    if (coming < 1.0f) {
      if (fOldGradient) {
        skiff::paint::verticalGradient(canvas, box, fOldGradient->top, fOldGradient->bottom, alpha * (1.0f - coming));
      }
      if (fOldDrawn) {
        skia::SkPaint old;
        old.setAlphaf(alpha * (1.0f - coming));
        canvas->drawImageRect(fOldDrawn, box, skia::SkSamplingOptions(skia::SkFilterMode::kLinear), &old);
      }
      alpha *= coming;
    }
    if (fGradient && !fPicture) {
      skiff::paint::verticalGradient(canvas, box, fGradient->top, fGradient->bottom, alpha);
    }
    const bool patterned = fPattern && fPattern->width > 0.0f && fPattern->height > 0.0f && !fPattern->steps.empty();
    if (!fPicture && !patterned) {
      this->offerBackdrop(canvas, box);
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
      fBlurred = nullptr;
    fExtra.clear();
    }
    if (!fDrawn) {
      return;
    }
    skia::SkPaint paint;
    paint.setAlphaf(alpha);
    // Made at the device's pixels: put down as it is, not filtered again at
    // every repaint -- a bilinear pass over the whole wallpaper was most of
    // a frame on a software canvas.
    canvas->drawImageRect(fDrawn, box, skia::SkSamplingOptions(skia::SkFilterMode::kNearest), &paint);
    this->offerBackdrop(canvas, box);
  }

  // Frosted nodes' backdrop: this wallpaper blurred -- made once for a size,
  // at a quarter of it, and drawn back up smooth -- and where it is on the
  // device. What frosts draws a piece of it, one image; nothing is blurred
  // at a frame.
  // This wallpaper shrunk as much as `amount` (0 to 1) says, and blurred
  // there: up to three times as blurred as it once went -- shrunk to a
  // thirteenth at the most.
  [[nodiscard]] skia::Sp<skia::SkImage> blurredAt(float amount, const skia::SkRect &box) const {
    const float shrink = 1.0f + amount * 12.0f;
    const int width = std::max(1, static_cast<int>(box.width() / shrink));
    const int height = std::max(1, static_cast<int>(box.height() / shrink));
    skia::SkBitmap small;
    if (!small.tryAllocN32Pixels(width, height)) {
      return nullptr;
    }
    small.eraseColor(0);
    skia::SkCanvas into(small);
    const skia::SkRect all = skia::SkRect::MakeWH(static_cast<float>(width), static_cast<float>(height));
    if (fGradient && !fPicture) {
      skiff::paint::verticalGradient(&into, all, fGradient->top, fGradient->bottom, 1.0f);
    }
    if (fDrawn) {
      into.drawImageRect(fDrawn, all, skia::SkSamplingOptions(skia::SkFilterMode::kLinear, skia::SkMipmapMode::kLinear));
    }
    // Blurred for real, a little -- a box three times over, near enough a
    // Gaussian: the picture still seen through the frost, not a wash of
    // its colours. Once for a size, on a fraction of its pixels.
    boxBlur(small, amount > 0.0f ? 1 : 0, 3);
    return small.asImage();
  }
  // A blurred copy drawn back up to the device's pixels, once.
  [[nodiscard]] static skia::Sp<skia::SkImage> upTo(const skia::Sp<skia::SkImage> &blurred, int fullWidth, int fullHeight) {
    skia::SkBitmap full;
    if (!blurred || !full.tryAllocN32Pixels(fullWidth, fullHeight)) {
      return nullptr;
    }
    full.eraseColor(0);
    skia::SkCanvas into(full);
    into.drawImageRect(blurred, skia::SkRect::MakeWH(static_cast<float>(fullWidth), static_cast<float>(fullHeight)),
                       skia::SkSamplingOptions(skia::SkFilterMode::kLinear));
    return full.asImage();
  }

  void offerBackdrop(skia::SkCanvas *canvas, const skia::SkRect &box) {
    if (!fBlurred) {
      fBackdropFull = nullptr;
      fBlurred = this->blurredAt(fAmount, box);
      if (!fBlurred) {
        return;
      }
    }
    // And drawn back up to the device's pixels, once: what frosts puts down
    // a piece of it as it is -- the blurred quarter was scaled up, bilinear,
    // under every frosted panel at every repaint.
    const skia::SkRect device = canvas->getTotalMatrix().mapRect(box);
    const int fullWidth = std::max(1, static_cast<int>(std::ceil(device.width())));
    const int fullHeight = std::max(1, static_cast<int>(std::ceil(device.height())));
    if (fBlurred && (!fBackdropFull || fBackdropFull->width() != fullWidth || fBackdropFull->height() != fullHeight)) {
      fBackdropFull = upTo(fBlurred, fullWidth, fullHeight);
      fExtra.clear();
    }
    // The other blurs asked: each made once, at the device's pixels.
    if (fExtra.size() != fAmounts.size()) {
      fExtra.clear();
      for (const float amount : fAmounts) {
        fExtra.emplace_back(amount, amount == fAmount && fBackdropFull ? fBackdropFull
                                                                       : upTo(this->blurredAt(amount, box), fullWidth, fullHeight));
      }
    }
    skiff::scene::detail::backdrop() = {fBackdropFull ? fBackdropFull : fBlurred, device, fExtra};
  }

private:
  // The backdrop: made at a quarter of the size, and blurred there by this
  // many of its pixels either way.
  static constexpr float kShrink = 4.0f;
  static constexpr int kRadius = 3;
  // A box blur of premultiplied pixels, across then down, `passes` times.
  static void boxBlur(skia::SkBitmap &bitmap, int radius, int passes) {
    const int w = bitmap.width(), h = bitmap.height();
    if (w <= 0 || h <= 0 || radius <= 0) {
      return;
    }
    auto *pixels = static_cast<std::uint8_t *>(bitmap.getPixels());
    const std::size_t stride = bitmap.rowBytes();
    std::vector<std::uint8_t> line(static_cast<std::size_t>(std::max(w, h)) * 4u);
    const int window = 2 * radius + 1;
    const auto pass = [&](int count, int length, auto at) {
      for (int i = 0; i < count; ++i) {
        for (int j = 0; j < length; ++j) {
          std::memcpy(&line[static_cast<std::size_t>(j) * 4u], at(i, j), 4);
        }
        std::array<int, 4> sum{};
        for (int k = -radius; k <= radius; ++k) {
          const auto j = static_cast<std::size_t>(std::clamp(k, 0, length - 1));
          for (std::size_t c = 0; c < 4; ++c) {
            sum[c] += line[j * 4u + c];
          }
        }
        for (int j = 0; j < length; ++j) {
          std::uint8_t *out = at(i, j);
          for (std::size_t c = 0; c < 4; ++c) {
            out[c] = static_cast<std::uint8_t>(sum[c] / window);
          }
          const auto add = static_cast<std::size_t>(std::min(j + radius + 1, length - 1));
          const auto drop = static_cast<std::size_t>(std::max(j - radius, 0));
          for (std::size_t c = 0; c < 4; ++c) {
            sum[c] += line[add * 4u + c] - line[drop * 4u + c];
          }
        }
      }
    };
    for (int p = 0; p < passes; ++p) {
      pass(h, w, [&](int y, int x) { return pixels + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4u; });
      pass(w, h, [&](int x, int y) { return pixels + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4u; });
    }
  }
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
  skia::Sp<skia::SkImage> fBlurred;
  // The backdrop at the device's pixels: what frosted panels draw from.
  skia::Sp<skia::SkImage> fBackdropFull;
  // The other blurs asked, and each made at the device's pixels.
  std::vector<float> fAmounts;
  std::vector<std::pair<float, skia::Sp<skia::SkImage>>> fExtra;
  float fOpacity = 1.0f;
  // The look before a change, fading out; and the fade.
  std::optional<skiff::scene::Gradient> fOldGradient;
  skia::Sp<skia::SkImage> fOldDrawn;
  skiff::paint::Tween fFade{1.0f, 260.0f};
  bool fKeptThisFrame = false;
  float fAmount = 0.3f;
  int fDrawnWidth = 0;
  int fDrawnHeight = 0;
};

} // namespace skiff::widgets
