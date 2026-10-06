export module skiff.widgets.wallpaper;

import std;
import skia;
import splice;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.erased;

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
using PatternStep = spl::variant<pattern_step::move, pattern_step::line, pattern_step::cubic, pattern_step::close>;
struct Pattern {
  float width = 0.0f;
  float height = 0.0f;
  std::vector<PatternStep> steps;
};

// What a wallpaper's blurred copies are made from: the gradient drawn under
// its pattern (none under a picture), the picture or pattern in pixels, its
// box, and the device's pixels each copy is drawn back up to.
struct BlurMaking {
  std::optional<skiff::scene::Gradient> gradient;
  skia::Sp<skia::SkImage> drawn;
  skia::SkRect box = skia::SkRect::MakeEmpty();
  int fullWidth = 1;
  int fullHeight = 1;
};

namespace detail {
// A box blur of premultiplied pixels, across then down, `passes` times.
inline void boxBlur(skia::SkBitmap &bitmap, int radius, int passes) {
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
// A wallpaper shrunk as much as `amount` (0 to 1) says, and blurred there:
// up to three times as blurred as it once went -- shrunk to a thirteenth at
// the most.
[[nodiscard]] inline skia::Sp<skia::SkImage> blurredAt(const BlurMaking &from, float amount) {
  const float shrink = 1.0f + amount * 12.0f;
  const int width = std::max(1, static_cast<int>(from.box.width() / shrink));
  const int height = std::max(1, static_cast<int>(from.box.height() / shrink));
  skia::SkBitmap small;
  if (!small.tryAllocN32Pixels(width, height)) {
    return nullptr;
  }
  small.eraseColor(0);
  skia::SkCanvas into(small);
  const skia::SkRect all = skia::SkRect::MakeWH(static_cast<float>(width), static_cast<float>(height));
  if (from.gradient) {
    skiff::paint::verticalGradient(&into, all, from.gradient->top, from.gradient->bottom, 1.0f);
  }
  if (from.drawn) {
    into.drawImageRect(from.drawn, all, skia::SkSamplingOptions(skia::SkFilterMode::kLinear, skia::SkMipmapMode::kLinear));
  }
  // Blurred for real, a little -- a box three times over, near enough a
  // Gaussian: the picture still seen through the frost, not a wash of its
  // colours. Once for a size, on a fraction of its pixels.
  boxBlur(small, amount > 0.0f ? 1 : 0, 3);
  return small.asImage();
}
// A blurred copy drawn back up to the device's pixels, once.
[[nodiscard]] inline skia::Sp<skia::SkImage> upTo(const skia::Sp<skia::SkImage> &blurred, int fullWidth, int fullHeight) {
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
} // namespace detail

// A wallpaper's blurred copies, each made once at the device's pixels: those
// it was asked for made as it is drawn, and any other a frosted thing asks
// for made where it is first asked -- not the wallpaper's own blur put in its
// place, a panel frosted by another amount than the one it asked. Shared by
// the backdrops its wallpaper offers, until its look or its size changes.
class Blurs {
public:
  explicit Blurs(BlurMaking from) : fFrom(std::move(from)) {}
  [[nodiscard]] const BlurMaking &from() const { return fFrom; }
  [[nodiscard]] skia::Sp<skia::SkImage> at(float amount) const {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto found = std::ranges::find_if(fMade, [&](const auto &one) { return std::abs(one.first - amount) < 1e-4f; });
    if (found != fMade.end()) {
      return found->second;
    }
    skia::Sp<skia::SkImage> image = detail::upTo(detail::blurredAt(fFrom, amount), fFrom.fullWidth, fFrom.fullHeight);
    fMade.emplace_back(amount, image);
    return image;
  }

private:
  BlurMaking fFrom;
  // What was made, kept: a cache, filled by those that only read it.
  mutable std::vector<std::pair<float, skia::Sp<skia::SkImage>>> fMade;
};

// What lies behind a window's contents, blurred: a wallpaper's blurred
// copies, its own amount (for what asks none), where on the device it is,
// whether it hides what is under it, and the frame it was drawn in. A
// Wallpaper makes it; a BackdropPane draws what of it is under it.
struct Backdrop {
  std::shared_ptr<const Blurs> blurs;
  float own = 0.3f;
  skia::SkRect device = skia::SkRect::MakeEmpty();
  bool opaque = false;
  std::uint64_t frame = 0;
};
// The backdrop blurred as much as `blur` says (0 to 1); below 0, by its own.
[[nodiscard]] inline skia::Sp<skia::SkImage> backdropImage(const Backdrop &one, float blur) {
  return one.blurs ? one.blurs->at(blur >= 0.0f ? blur : one.own) : nullptr;
}
// The piece of a backdrop under a shape, as blurred as `blur` says: the
// shape filled with the image, one antialiased draw -- not an antialiased
// clip, a mask made for each frosted thing at each repaint, and the image
// drawn into it. At the device's pixels, put down as it is; smoothed only
// where it is smaller than the device; nothing outside the backdrop.
inline void drawBackdrop(skia::SkCanvas *canvas, const Backdrop &one, float blur, const skia::SkRRect &shape, float alpha) {
  skia::SkMatrix inverse;
  if (one.device.isEmpty() || !canvas->getTotalMatrix().invert(&inverse)) {
    return;
  }
  const skia::Sp<skia::SkImage> image = backdropImage(one, blur);
  if (!image) {
    return;
  }
  const skia::SkMatrix local =
      skia::SkMatrix::RectToRect(skia::SkRect::MakeIWH(image->width(), image->height()), inverse.mapRect(one.device));
  const bool sharp = std::abs(static_cast<float>(image->width()) - one.device.width()) <= 1.0f;
  skia::SkPaint paint;
  paint.setAntiAlias(true);
  paint.setAlphaf(alpha);
  paint.setShader(image->makeShader(skia::SkTileMode::kDecal, skia::SkTileMode::kDecal,
                                    skia::SkSamplingOptions(sharp ? skia::SkFilterMode::kNearest : skia::SkFilterMode::kLinear),
                                    &local));
  canvas->drawRRect(shape, paint);
}
// What is under a shape, frosted, of the backdrops the wallpapers offered (in
// the order they lie, the lowest first): those drawn in this frame and under
// it, from the topmost that hides all under it up -- the wallpapers really
// under it, not whichever was drawn last (a panel over the window's
// background then showed the chat's, or the chat's other blur, row by row as
// each was repainted). One drawn this frame is one shown: a hidden one is
// not drawn.
template <std::ranges::forward_range Backdrops>
  requires std::convertible_to<std::ranges::range_reference_t<Backdrops>, const Backdrop &>
void drawBackdrops(skia::SkCanvas *canvas, Backdrops &&all, float blur, const skia::SkRRect &shape, float alpha) {
  const skia::SkRect on = canvas->getTotalMatrix().mapRect(shape.rect());
  const std::uint64_t now = skiff::scene::work::frameNumber();
  auto under = std::views::filter(all, [&](const Backdrop &one) {
                 return one.frame == now && skia::SkRect::Intersects(one.device, on);
               });
  const auto hiding = std::ranges::find_last_if(
      under, [&](const Backdrop &one) { return one.opaque && one.device.contains(on.makeInset(0.5f, 0.5f)); });
  std::ranges::for_each(hiding.empty() ? under.begin() : hiding.begin(), under.end(),
                        [&](const Backdrop &one) { drawBackdrop(canvas, one, blur, shape, alpha); });
}
// Where backdrops come from: a value called as it is drawn, as an Image's
// source is -- the program's, giving those its wallpapers offered, the
// lowest first.
template <class Source>
concept BackdropSource = std::copy_constructible<Source> && requires(const Source &source) {
  requires std::ranges::forward_range<decltype(source())>;
  requires std::convertible_to<std::ranges::range_reference_t<decltype(source())>, const Backdrop &>;
};
// What of the backdrop is under it on the screen, in its box and corner
// radius: frosted glass, behind what a node holds -- its first part, filling
// it. As blurred as setBlur says (0 to 1); below 0, the backdrop's own.
// The backdrops a source gives, drawn: those it is called for.
template <BackdropSource Source>
void drawFrom(const Source &source, skia::SkCanvas *canvas, float blur, const skia::SkRRect &shape, float alpha) {
  drawBackdrops(canvas, source(), blur, shape, alpha);
}
// A source, whatever it is: its backdrops drawn as they would be, lazily, by
// the code made where it was erased. Erasure, outside a release build only.
class AnyBackdropSource {
public:
  // Itself first: a copy is checked as one before it is asked whether it is
  // a source -- the other way round, that asks itself again, without end.
  template <class Source>
    requires(!std::same_as<std::remove_cvref_t<Source>, AnyBackdropSource> && BackdropSource<Source>)
  explicit AnyBackdropSource(Source source)
      : fDraw([source = std::move(source)](skia::SkCanvas *canvas, float blur, const skia::SkRRect &shape, float alpha) {
          drawFrom(source, canvas, blur, shape, alpha);
        }) {}
  void draw(skia::SkCanvas *canvas, float blur, const skia::SkRRect &shape, float alpha) const {
    fDraw(canvas, blur, shape, alpha);
  }

private:
  std::function<void(skia::SkCanvas *, float, const skia::SkRRect &, float)> fDraw;
};
inline void drawFrom(const AnyBackdropSource &source, skia::SkCanvas *canvas, float blur, const skia::SkRRect &shape,
                     float alpha) {
  source.draw(canvas, blur, shape, alpha);
}
namespace internal {
template <class Source> class BackdropPane : public skiff::scene::Node {
public:
  explicit BackdropPane(Source source, float blur = -1.0f) : fSource(std::move(source)), fBlur(blur) {}
  void setBlur(float blur) {
    if (blur == fBlur) {
      return;
    }
    fBlur = blur;
    this->markDamaged();
  }
  // A colour over the frost, in the same shape: the plate of what it is
  // behind -- which, frosted, has none of its own under it.
  void setTint(std::optional<skia::SkColor> tint) {
    if (tint == fTint) {
      return;
    }
    fTint = tint;
    this->markDamaged();
  }
  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    // Its shape as its State says: its corner radius, or each corner's own
    // -- as what it is behind says them.
    const skia::SkRRect shape = skiff::scene::detail::roundedBox(fState, fState.fBounds);
    drawFrom(fSource, canvas, fBlur, shape, alpha);
    if (fTint) {
      skia::SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor(*fTint);
      paint.setAlphaf(paint.getAlphaf() * alpha);
      canvas->drawRRect(shape, paint);
    }
  }

private:
  Source fSource;
  float fBlur;
  std::optional<skia::SkColor> fTint;
};
} // namespace internal

// The pane over AnyBackdropSource, taking a source of its own type and
// erasing it: all of its code but this is internal::BackdropPane<AnyBackdropSource>'s.
template <BackdropSource Source>
class ErasedBackdropPane : public internal::BackdropPane<AnyBackdropSource> {
  using Base = internal::BackdropPane<AnyBackdropSource>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedBackdropPane(Source source, float blur = -1.0f)
      : internal::BackdropPane<AnyBackdropSource>(AnyBackdropSource(std::move(source)), blur) {}
};
// The pane: made for its source in a release build, over AnyBackdropSource
// otherwise.
template <BackdropSource Source>
using BackdropPane = std::conditional_t<kErasedActions, ErasedBackdropPane<Source>, internal::BackdropPane<Source>>;


// Where a wallpaper's backdrop goes: nowhere, unless the program says.
struct NoBackdropOut {
  static void offer(skiff::scene::NodeId, const Backdrop &) {}
};
// Where a wallpaper offers, whatever it is: through its one function.
// Erasure, outside a release build only.
struct AnyBackdropOut {
  void (*fOffer)(skiff::scene::NodeId, const Backdrop &) = &NoBackdropOut::offer;
  template <class Out> [[nodiscard]] static AnyBackdropOut of() { return {&Out::offer}; }
  void offer(skiff::scene::NodeId id, const Backdrop &one) const { fOffer(id, one); }
};

// Telegram's freeform gradient, as lib_ui and Telegram's apps make it: up
// to four colours, each at its point, turned in a small swirl about the
// centre; each pixel the colours weighted by the fourth power of how near
// it is to each (0.9 less the distance, nothing past it). Made 64 by 64,
// and stretched over what it fills.
[[nodiscard]] inline skia::Sp<skia::SkImage> freeformGradient(const std::vector<skia::SkColor> &colours) {
  constexpr int kSide = 64;
  constexpr std::array<std::pair<float, float>, 4> kPoints{{{0.80f, 0.10f}, {0.35f, 0.25f}, {0.20f, 0.90f}, {0.65f, 0.75f}}};
  skia::SkBitmap bitmap;
  if (colours.empty() || !bitmap.tryAllocN32Pixels(kSide, kSide)) {
    return nullptr;
  }
  skia::SkCanvas into(bitmap);
  const auto channel = [](skia::SkColor colour, int shift) { return static_cast<float>((colour >> shift) & 0xFFu); };
  const auto at = [&](int x, int y) {
    const float centreX = static_cast<float>(x) / kSide - 0.5f;
    const float centreY = static_cast<float>(y) / kSide - 0.5f;
    const float swirl = 0.35f * std::sqrt(centreX * centreX + centreY * centreY);
    const float theta = swirl * swirl * 0.8f * 8.0f;
    const float px = std::clamp(0.5f + centreX * std::cos(theta) - centreY * std::sin(theta), 0.0f, 1.0f);
    const float py = std::clamp(0.5f + centreX * std::sin(theta) + centreY * std::cos(theta), 0.0f, 1.0f);
    const auto weights = std::ranges::to<std::vector>(std::views::transform(std::views::iota(std::size_t{0}, std::min(colours.size(), kPoints.size())), [&](std::size_t i) {
                           const float dx = px - kPoints[i].first, dy = py - kPoints[i].second;
                           const float near = std::max(0.0f, 0.9f - std::sqrt(dx * dx + dy * dy));
                           return std::pair{i, near * near * near * near};
                         }));
    const float sum = std::ranges::fold_left(std::views::values(weights), 0.0f, std::plus{});
    const auto mixed = [&](int shift) {
      return sum <= 0.0f ? channel(colours.front(), shift)
                         : std::ranges::fold_left(weights, 0.0f, [&](float so_far, const auto &one) {
                             return so_far + channel(colours[one.first], shift) * one.second;
                           }) / sum;
    };
    return skia::colorSetARGB(255, static_cast<unsigned>(std::lround(mixed(16))), static_cast<unsigned>(std::lround(mixed(8))),
                              static_cast<unsigned>(std::lround(mixed(0))));
  };
  std::ranges::for_each(std::views::iota(0, kSide), [&](int y) {
    std::ranges::for_each(std::views::iota(0, kSide), [&](int x) {
      skia::SkPaint paint;
      paint.setColor(at(x, y));
      into.drawRect(skia::SkRect::MakeXYWH(static_cast<float>(x), static_cast<float>(y), 1.0f, 1.0f), paint);
    });
  });
  return bitmap.asImage();
}

// A chat's wallpaper, as Telegram's: a gradient and over it a pattern in a
// colour of its own -- or a picture of the user's -- scaled to cover it and
// centred; or nothing, what is behind it showing (a plain colour). The
// pattern or the picture is drawn once for a size, into pixels kept: what is
// behind a list is repainted at every step of a scroll.
// What it blurs of itself, for what frosts, is offered to Out::offer(its id,
// backdrop) as it is drawn: a callback by its type.
namespace internal {
template <class Out = NoBackdropOut> class Wallpaper : public skiff::scene::Node {
public:
  // Where it offers what it blurs: what an erased wallpaper is told of its own.
  void setOut(Out out) { fOut = std::move(out); }
  void setGradient(std::optional<skiff::scene::Gradient> gradient) {
    if (gradient == fGradient) {
      return;
    }
    this->keepOld();
    fGradient = gradient;
    fBlurs = nullptr;
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
    fBlurs = nullptr;
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
    fBlurs = nullptr;
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
    fBlurs = nullptr;
    this->markDamaged();
  }
  // Telegram's freeform gradient of these colours (freeformGradient), in
  // place of the gradient, the pattern and a picture; none, none.
  void setFreeform(std::vector<skia::SkColor> colours) {
    if (colours == fFreeform) {
      return;
    }
    this->keepOld();
    fFreeform = std::move(colours);
    fFreeformImage = freeformGradient(fFreeform);
    fDrawn = nullptr;
    fBlurs = nullptr;
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
    fBlurs = nullptr;
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
    if (!fPicture && !patterned && !fFreeformImage) {
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
      fBlurs = nullptr;
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

  // Frosted nodes' backdrop: this wallpaper blurred -- each amount made once
  // for a size, shrunk, blurred there and drawn back up to the device's
  // pixels -- and where it is on the device. What frosts draws a piece of
  // it, one image; nothing is blurred at a frame. Made again where its size
  // changes, not where it only moves: a panel sliding moved it every frame.
  void offerBackdrop(skia::SkCanvas *canvas, const skia::SkRect &box) {
    const skia::SkRect device = canvas->getTotalMatrix().mapRect(box);
    const int fullWidth = std::max(1, static_cast<int>(std::ceil(device.width())));
    const int fullHeight = std::max(1, static_cast<int>(std::ceil(device.height())));
    if (!fBlurs || fBlurs->from().box.width() != box.width() || fBlurs->from().box.height() != box.height() ||
        fBlurs->from().fullWidth != fullWidth || fBlurs->from().fullHeight != fullHeight) {
      auto made = std::make_shared<const Blurs>(
          BlurMaking{fGradient && !fPicture ? fGradient : std::nullopt, fDrawn, box, fullWidth, fullHeight});
      // Its own and each asked made now, as it is drawn -- not where the
      // first frosted thing asks for it.
      static_cast<void>(made->at(fAmount));
      std::ranges::for_each(fAmounts, [&](float amount) { static_cast<void>(made->at(amount)); });
      fBlurs = std::move(made);
    }
    fOut.offer(fState.fId, Backdrop{fBlurs, fAmount, device, this->opaque(), skiff::scene::work::frameNumber()});
  }

private:
  [[no_unique_address]] Out fOut{};
  // Whether nothing under it shows through: drawn whole, its look settled,
  // its picture or its gradient opaque.
  [[nodiscard]] bool opaque() const {
    const auto solid = [](skia::SkColor colour) { return (colour >> 24) == 0xFFu; };
    return fOpacity >= 1.0f && !fFade.moving() &&
           (fPicture           ? fPicture->isOpaque()
            : fFreeformImage   ? true
                               : fGradient && solid(fGradient->top) && solid(fGradient->bottom));
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
    // The freeform gradient: stretched over all of it, as tdesktop does.
    if (fFreeformImage && !fPicture) {
      into.drawImageRect(fFreeformImage, skia::SkRect::MakeIWH(width, height),
                         skia::SkSamplingOptions(skia::SkFilterMode::kLinear));
      return bitmap.asImage();
    }
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
      spl::visit(spl::overloaded{[&](const pattern_step::move &one) { shapes.moveTo(one.x, one.y); },
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
  std::vector<skia::SkColor> fFreeform;
  skia::Sp<skia::SkImage> fFreeformImage;
  skia::Sp<skia::SkImage> fPicture;
  std::shared_ptr<const Pattern> fPattern;
  skia::SkColor fColour = 0;
  skia::Sp<skia::SkImage> fDrawn;
  // Its blurred copies, made at the device's pixels: what frosted things
  // draw from.
  std::shared_ptr<const Blurs> fBlurs;
  // The other blurs asked, made with its own.
  std::vector<float> fAmounts;
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
} // namespace internal

// The wallpaper over AnyBackdropOut, told its own: all of its code but this is
// internal::Wallpaper<AnyBackdropOut>'s.
template <class Out>
class ErasedWallpaper : public internal::Wallpaper<AnyBackdropOut> {
  using Base = internal::Wallpaper<AnyBackdropOut>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  ErasedWallpaper() { this->setOut(AnyBackdropOut::of<Out>()); }
};
// The wallpaper: made for its Out in a release build, over AnyBackdropOut
// otherwise.
template <class Out = NoBackdropOut>
using Wallpaper = std::conditional_t<kErasedActions, ErasedWallpaper<Out>, internal::Wallpaper<Out>>;


} // namespace skiff::widgets
