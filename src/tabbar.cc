export module skiff.widgets.tabbar;

import std;
import skia;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.theme;
export import skiff.widgets.erased;

namespace skiff::widgets {
using skiff::scene::Margin;
using skiff::scene::Spec;
} // namespace skiff::widgets

export namespace skiff::widgets {

// A row of text tabs, one of them selected: lazer's OsuTabControl, and the
// two hand-written copies of it in the beatmap listing. It knows how to wrap
// when it runs out of width, which of its own tabs the pointer is on, and
// how to say that one of them was clicked. What a tab means is the caller's
// business, and so is anything drawn beside the selected one, which is what
// drawDecoration is for.
namespace internal {
template <class OnSelect = skiff::scene::NoAction,
          class IsActive = skiff::scene::NoAction,
          class Decorate = skiff::scene::NoAction>
class TabBar : public skiff::scene::Node {
public:
  struct Tab {
    std::string fLabel;
    int fValue = 0;
  };

  // What selecting a tab does; which tabs read as selected -- left out it is
  // the one whose value is selected(), and a bar where several can be on at
  // once answers for itself; and what is drawn after the selected tab, given
  // its box: a sort chevron, a count, an underline.
  // Made in a theme: the one it is given, not one of the library's.
  TabBar(Theme theme, OnSelect onSelect, IsActive isActive = {}, Decorate decorate = {})
      : TabBar(std::move(onSelect), std::move(isActive), std::move(decorate)) {
    fTheme = std::move(theme);
  }
  explicit TabBar(OnSelect onSelect = {}, IsActive isActive = {},
                  Decorate decorate = {})
      : fOnSelect(std::move(onSelect)), fIsActive(std::move(isActive)),
        fDecorate(std::move(decorate)) {
    fState.fRelativeSizeAxes = skiff::scene::axes::kX;
    fState.fWidth = 1.0f;
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }
  void setHeader(std::string header, float width) {
    if (header != fHeader || width != fHeaderWidth) {
      fHeader = std::move(header);
      fHeaderWidth = width;
      this->invalidateLayout();
    }
  }
  void setMetrics(float fontSize, float lineHeight, float spacing) {
    if (fontSize != fFontSize || lineHeight != fLineHeight ||
        spacing != fSpacing) {
      fFontSize = fontSize;
      fLineHeight = lineHeight;
      fSpacing = spacing;
      this->invalidateLayout();
    }
  }
  void setWrap(bool wrap) {
    if (wrap != fWrap) {
      fWrap = wrap;
      this->invalidateLayout();
    }
  }

  void setTabs(std::vector<Tab> tabs) {
    if (tabs.size() == fTabs.size() &&
        std::equal(tabs.begin(), tabs.end(), fTabs.begin(),
                   [](const Tab &a, const Tab &b) {
                     return a.fValue == b.fValue && a.fLabel == b.fLabel;
                   })) {
      return;
    }
    fTabs = std::move(tabs);
    this->invalidateLayout();
  }

  void setSelected(int value) {
    if (value == fSelected) {
      return;
    }
    fSelected = value;
    // The selected tab is drawn bold and may carry a decoration, both of
    // which move the tabs after it along.
    this->invalidateLayout();
  }
  [[nodiscard]] int selected() const noexcept { return fSelected; }
  // The tab picked: told at once, or -- where the action answers -- said
  // pressed, the answer asked as the press is delivered (onPress).
  void pick(int index)
    requires skiff::scene::Answering<OnSelect>
  {
    fPicked = index;
    skiff::scene::pressLater(fState);
  }
  void pick(int index) { std::invoke(fOnSelect, fTabs[static_cast<std::size_t>(index)].fValue); }
  auto onPress()
    requires skiff::scene::Answering<OnSelect>
  {
    return std::invoke(fOnSelect, fTabs[static_cast<std::size_t>(fPicked)].fValue);
  }
  [[nodiscard]] int picked() const noexcept { return fPicked; }
  int fPicked = -1;
  [[nodiscard]] std::span<const Tab> tabs() const noexcept { return fTabs; }

  // Where a tab ended up, in screen coordinates.
  [[nodiscard]] skia::SkRect tabBounds(std::size_t i) const {
    if (i >= fRects.size()) {
      return skia::SkRect::MakeEmpty();
    }
    const skia::SkRect &local = fRects[i];
    return skia::SkRect::MakeXYWH(fState.fBounds.fLeft + local.fLeft,
                                  fState.fBounds.fTop + local.fTop, local.width(),
                                  local.height());
  }

public:
  Theme fTheme;
  std::string fHeader;       // caption in the column to the left, may be empty
  float fHeaderWidth = 0.0f; // where the tabs start, header or no header
  float fFontSize = 13.0f;
  float fLineHeight = 16.0f;
  float fBaseline = -1.0f; // within a line box; below zero means fFontSize
  float fSpacing = 10.0f;
  float fSelectedExtra = 0.0f; // room after the selected tab for a decoration
  bool fWrap = true;

  // The height depends on how the tabs wrap, which depends on the width this
  // has been given, so it is worked out here where that is known and the
  // positions are kept for drawing and for hit testing.
  void measure(const skia::SkRect &parent) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(nullptr, *font);
    const float width =
        fState.fRelativeSizeAxes.template has<skiff::scene::axis::x>() ? parent.width() * fState.fWidth : fState.fWidth;
    float x = fHeaderWidth;
    float y = 0.0f;
    fRects.clear();
    fRects.reserve(fTabs.size());
    for (const Tab &tab : fTabs) {
      // Measured bold either way, so selecting one does not shuffle the row.
      const float w = p.measure(tab.fLabel, fFontSize, true);
      if (fWrap && x > fHeaderWidth && x + w > width) {
        x = fHeaderWidth;
        y += fLineHeight;
      }
      fRects.push_back(skia::SkRect::MakeXYWH(x, y, w, fLineHeight));
      x += w + fSpacing + (this->activeTab(tab) ? fSelectedExtra : 0.0f);
    }
    fState.fHeight = y + fLineHeight;
  }

  // The bar lights the tab under the pointer, so moving between two tabs of
  // the same bar changes what it draws while the bar itself stays hovered.
  // Nothing else in the tree would notice that.
  // Ticked while the pointer is over it, or a tab is still lit from it.
  [[nodiscard]] bool wantsTick() const { return fState.hovered() || fHotTab != -1; }
  void update(double) {
    const int hot = this->tabAt(fState.hoverX(), fState.hoverY());
    if (hot != fHotTab) {
      fHotTab = hot;
      this->markDamaged();
    }
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const float baseline = fBaseline >= 0.0f ? fBaseline : fFontSize;
    if (!fHeader.empty()) {
      p.text(fHeader, fState.fBounds.fLeft, fState.fBounds.fTop + baseline, fFontSize,
             fTheme.fLabel, alpha);
    }
    for (std::size_t i = 0; i < fTabs.size(); ++i) {
      const bool active = this->activeTab(fTabs[i]);
      const skia::SkRect box = this->tabBounds(i);
      skia::SkColor colour = active ? fTheme.fText : fTheme.fTextDim;
      if (static_cast<int>(i) == fHotTab) {
        colour = skiff::paint::lighten(colour, 0.2f);
      }
      p.text(fTabs[i].fLabel, box.fLeft, box.fTop + baseline, fFontSize, colour,
             alpha, active);
      if (active) {
        std::invoke(fDecorate, canvas, box, alpha);
      }
    }
    if (this->showsFocus()) {
      p.strokeRounded(fState.fBounds, 3.0f, fTheme.fAccent, 1.0f, alpha);
    }
  }

  [[nodiscard]] bool acceptsInput() const { return true; }
  [[nodiscard]] bool focusChangesAppearance() const { return true; }

  using Node::onKey;
  void onKey(skiff::scene::phase::target, const skiff::scene::key::down &press,
             skiff::scene::Reply &reply) {
    namespace keys = skiff::scene::keys;
    if (fTabs.empty()) {
      return;
    }
    const auto selected = std::ranges::find_if(
        fTabs, [this](const Tab &tab) { return tab.fValue == fSelected; });
    std::size_t index = selected == fTabs.end()
                            ? 0
                            : static_cast<std::size_t>(selected - fTabs.begin());
    if (press.key == keys::kLeft || press.key == keys::kUp) {
      index = (index + fTabs.size() - 1) % fTabs.size();
    } else if (press.key == keys::kRight || press.key == keys::kDown) {
      index = (index + 1) % fTabs.size();
    } else if (press.key == keys::kHome) {
      index = 0;
    } else if (press.key == keys::kEnd) {
      index = fTabs.size() - 1;
    } else {
      skiff::scene::defaultKey(*this, skiff::scene::phase::target{}, press, reply);
      return;
    }
    this->pick(static_cast<int>(index));
    reply.handle();
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::tab{};
    out.fLabel = fHeader;
    const auto selected = std::ranges::find_if(
        fTabs, [this](const Tab &tab) { return tab.fValue == fSelected; });
    if (selected != fTabs.end()) {
      out.fValue = selected->fLabel;
    }
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::increment{},
                    skiff::scene::semantic_action::decrement{}};
    return out;
  }

  using Node::onSemantic;
  void onSemantic(skiff::scene::phase::target at,
                  const skiff::scene::semantic_action::increment &,
                  skiff::scene::Reply &reply) {
    this->onKey(at, skiff::scene::key::down{skiff::scene::keys::kRight}, reply);
  }
  void onSemantic(skiff::scene::phase::target at,
                  const skiff::scene::semantic_action::decrement &,
                  skiff::scene::Reply &reply) {
    this->onKey(at, skiff::scene::key::down{skiff::scene::keys::kLeft}, reply);
  }

  [[nodiscard]] bool onClick(float x, float y) {
    const int hit = this->tabAt(x, y);
    if (hit < 0) {
      return false;
    }
    this->pick(hit);
    return true;
  }

  [[nodiscard]] bool activeTab(const Tab &tab) const {
    return this->answer(fIsActive, tab.fValue);
  }

  // Which tabs read as selected: the one whose value is selected(), unless
  // the bar was given its own answer.
  [[nodiscard]] bool answer(const skiff::scene::NoAction &, int value) const {
    return value == fSelected;
  }
  template <std::size_t Bytes>
  [[nodiscard]] bool answer(const AnyCall<bool(int), Bytes> &given, int value) const {
    return given.holds() ? given(value) : value == fSelected;
  }
  template <class Answer>
  [[nodiscard]] bool answer(const Answer &given, int value) const {
    return std::invoke(given, value);
  }

  [[nodiscard]] int tabAt(float x, float y) const {
    for (std::size_t i = 0; i < fRects.size(); ++i) {
      if (this->tabBounds(i).contains(x, y)) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

private:
  [[no_unique_address]] OnSelect fOnSelect;

public:

private:

public:
  // What selecting a tab calls, as held: where a model's tabs keep the pick.
  OnSelect &onSelect() noexcept { return fOnSelect; }

private:
  [[no_unique_address]] IsActive fIsActive;
  [[no_unique_address]] Decorate fDecorate;
  std::vector<Tab> fTabs;
  std::vector<skia::SkRect> fRects; // relative to this bar
  int fSelected = -1;
  int fHotTab = -1;
};
} // namespace internal

// The tab bar over erased calls -- a tab chosen, by its value; whether a tab
// reads as selected; and what is drawn after the selected one, in its box --
// taking what it is given and erasing it.
template <class OnSelect, class IsActive, class Decorate>
class ErasedTabBar
    : public internal::TabBar<AnyCallFor<void(int), OnSelect>, AnyCallFor<bool(int), IsActive>,
                              AnyCallFor<void(skia::SkCanvas *, const skia::SkRect &, float), Decorate>> {
  using Base = internal::TabBar<AnyCallFor<void(int), OnSelect>, AnyCallFor<bool(int), IsActive>, AnyCallFor<void(skia::SkCanvas *, const skia::SkRect &, float), Decorate>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  ErasedTabBar(Theme theme, OnSelect onSelect, IsActive isActive = {}, Decorate decorate = {})
      : Base(std::move(theme), AnyCallFor<void(int), OnSelect>(std::move(onSelect)), AnyCallFor<bool(int), IsActive>(std::move(isActive)),
             AnyCallFor<void(skia::SkCanvas *, const skia::SkRect &, float), Decorate>(std::move(decorate))) {}
  explicit ErasedTabBar(OnSelect onSelect = {}, IsActive isActive = {}, Decorate decorate = {})
      : Base(AnyCallFor<void(int), OnSelect>(std::move(onSelect)), AnyCallFor<bool(int), IsActive>(std::move(isActive)), AnyCallFor<void(skia::SkCanvas *, const skia::SkRect &, float), Decorate>(std::move(decorate))) {}
};
// The tab bar: made for what it is given in a release build, over those
// otherwise.
template <class OnSelect = skiff::scene::NoAction, class IsActive = skiff::scene::NoAction,
          class Decorate = skiff::scene::NoAction>
using TabBar = std::conditional_t<kErasedActions && !AnswersPresses<OnSelect, IsActive, Decorate>, ErasedTabBar<OnSelect, IsActive, Decorate>,
                                  internal::TabBar<OnSelect, IsActive, Decorate>>;



} // namespace skiff::widgets
