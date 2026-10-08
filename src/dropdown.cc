export module skiff.widgets.dropdown;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes;
export import skiff.widgets.theme;
export import skiff.widgets.erased;

namespace skiff::widgets {
using skiff::scene::Margin;
using skiff::scene::Spec;
} // namespace skiff::widgets

export namespace skiff::widgets {

// The closed half of a dropdown. The label, current value and open state are
// data; hover, hit testing and the chevron belong to the widget.
namespace internal {
template <class OnOpen = skiff::scene::NoAction>
class DropdownButton : public skiff::scene::Node {
public:
  explicit DropdownButton(std::string label = {}, std::string value = {},
                          OnOpen onOpen = {})
      : fOnOpen(std::move(onOpen)), fLabel(std::move(label)),
        fValue(std::move(value)) {
    fState.fHeight = 30.0f;
  }
  // Made in a theme: the one it is given, not one of the library's.
  DropdownButton(Theme theme, std::string label = {}, std::string value = {}, OnOpen onOpen = {})
      : DropdownButton(std::move(label), std::move(value), std::move(onOpen)) {
    fTheme = std::move(theme);
  }

  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }
  void setLabel(std::string label) {
    if (label == fLabel) {
      return;
    }
    fLabel = std::move(label);
    this->markDamaged();
  }
  void setValue(std::string value) {
    if (value == fValue) {
      return;
    }
    fValue = std::move(value);
    this->markDamaged();
  }
  void setOpen(bool open) {
    if (open == fOpen) {
      return;
    }
    fOpen = open;
    this->markDamaged();
  }
  [[nodiscard]] bool open() const noexcept { return fOpen; }

  Theme fTheme;
  float fLabelWidth = 52.0f;
  float fChevronWidth = 22.0f;
  float fStrokeWidth = 1.0f;
  float fOpenStrokeWidth = 2.0f;
  float fLabelAlpha = 0.5f;
  float fValueAlpha = 0.95f;

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skia::SkRect &bounds = fState.fBounds;
    const skiff::paint::Painter p(canvas, *font);
    p.fillRounded(bounds, fTheme.fCorner,
                  fState.fHovered || this->showsFocus() || fOpen
                      ? fTheme.fSurfaceHover
                      : fTheme.fSurface,
                  alpha);
    p.strokeRounded(bounds, fTheme.fCorner,
                    fOpen ? fTheme.fAccent : fTheme.fSurfaceActive,
                    fOpen ? fOpenStrokeWidth : fStrokeWidth, alpha);
    const float baseline = p.middleBaseline(bounds, fTheme.fFontSize);
    p.textClipped(fLabel, bounds.fLeft + fTheme.fPaddingX, baseline,
                  fLabelWidth, fTheme.fFontSize, fTheme.fLabel,
                  alpha * fLabelAlpha);
    const float valueLeft = bounds.fLeft + fTheme.fPaddingX + fLabelWidth;
    p.textClipped(fValue, valueLeft, baseline,
                  std::max(0.0f, bounds.fRight - valueLeft - fChevronWidth),
                  fTheme.fFontSize, fTheme.fText, alpha * fValueAlpha);

    skia::SkPaint triangle;
    triangle.setAntiAlias(true);
    triangle.setColor(fTheme.fText);
    triangle.setAlphaf(alpha * 0.7f);
    skia::SkPathBuilder path;
    const float cx = bounds.fRight - fChevronWidth * 0.5f;
    const float cy = bounds.centerY();
    if (fOpen) {
      path.moveTo(cx - 5.0f, cy + 2.5f);
      path.lineTo(cx + 5.0f, cy + 2.5f);
      path.lineTo(cx, cy - 3.5f);
    } else {
      path.moveTo(cx - 5.0f, cy - 2.5f);
      path.lineTo(cx + 5.0f, cy - 2.5f);
      path.lineTo(cx, cy + 3.5f);
    }
    path.close();
    canvas->drawPath(path.detach(), triangle);
  }

  [[nodiscard]] bool acceptsInput() const {
    return skiff::scene::acts(fOnOpen);
  }
  [[nodiscard]] bool hoverChangesAppearance() const {
    return skiff::scene::acts(fOnOpen) && !fOpen;
  }
  [[nodiscard]] bool focusChangesAppearance() const {
    return skiff::scene::acts(fOnOpen) && !fOpen;
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::button{};
    out.fLabel = fLabel;
    out.fValue = fValue;
    out.fHint = fOpen ? "expanded" : "collapsed";
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::activate{}};
    return out;
  }

  [[nodiscard]] bool onClick(float, float) {
    if (!skiff::scene::acts(fOnOpen)) {
      return false;
    }
    std::invoke(fOnOpen);
    return true;
  }

private:
  [[no_unique_address]] OnOpen fOnOpen;
  std::string fLabel;
  std::string fValue;
  bool fOpen = false;
};
} // namespace internal

// The dropdown button over AnyAction, taking the action given and erasing it.
template <class OnOpen>
class ErasedDropdownButton : public internal::DropdownButton<AnyActionFor<OnOpen>> {
  using Base = internal::DropdownButton<AnyActionFor<OnOpen>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedDropdownButton(std::string label = {}, std::string value = {}, OnOpen onOpen = {})
      : internal::DropdownButton<AnyActionFor<OnOpen>>(std::move(label), std::move(value), AnyActionFor<OnOpen>(std::move(onOpen))) {}
  ErasedDropdownButton(Theme theme, std::string label = {}, std::string value = {}, OnOpen onOpen = {})
      : internal::DropdownButton<AnyActionFor<OnOpen>>(std::move(theme), std::move(label), std::move(value), AnyActionFor<OnOpen>(std::move(onOpen))) {}
};
// The dropdown button: made for its action in a release build, over
// AnyAction otherwise.
template <class OnOpen = skiff::scene::NoAction>
using DropdownButton = std::conditional_t<kErasedActions, ErasedDropdownButton<OnOpen>, internal::DropdownButton<OnOpen>>;


// How a dropdown list's rows look: the list's, copied to each row so a row
// draws itself without reaching back to the list.
struct DropdownLook {
  Theme fTheme;
  float fRowHeight = 24.0f;
  float fFontSize = 13.0f;
  float fPlateRadius = 6.0f;
  float fRowRadius = 6.0f;
  float fTextInset = 12.0f;
  float fDimAlpha = 0.8f; // an option that is not the current one
};

// One option of an open dropdown. It lights when the pointer is on it, and
// its press is the list's to act on: the list sees it in the bubble phase,
// by the row's id.
class DropdownRow : public skiff::scene::Node {
public:
  DropdownRow(std::string label, const DropdownLook &look)
      : fLabel(std::move(label)), fLook(look) {
    fState.fRelativeSizeAxes = skiff::scene::axes::kX;
    fState.fWidth = 1.0f;
    fState.fHeight = look.fRowHeight;
  }

  void setChosen(bool chosen) {
    if (chosen != fChosen) {
      fChosen = chosen;
      this->markDamaged();
    }
  }
  void setLook(const DropdownLook &look) {
    fLook = look;
    fState.fHeight = look.fRowHeight;
    this->invalidateLayout();
  }
  [[nodiscard]] const std::string &label() const noexcept { return fLabel; }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const Theme &theme = fLook.fTheme;
    const skia::SkRect &bounds = fState.fBounds;
    const skiff::paint::Painter p(canvas, *font);
    if (fChosen || fState.fHovered || this->showsFocus()) {
      p.fillRounded(bounds, fLook.fRowRadius,
                    fChosen ? theme.fAccent : theme.fSurfaceHover, alpha);
    }
    p.textIn(bounds, fLabel, fLook.fFontSize,
             fChosen ? theme.fOnAccent : theme.fText,
             alpha * (fChosen ? 1.0f : fLook.fDimAlpha), false,
             fLook.fTextInset);
  }

  [[nodiscard]] bool acceptsInput() const { return true; }
  [[nodiscard]] bool hoverChangesAppearance() const { return !fChosen; }
  [[nodiscard]] bool focusChangesAppearance() const { return true; }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::list_item{};
    out.fLabel = fLabel;
    out.fSelected = fChosen;
    out.fActions = {skiff::scene::semantic_action::focus{},
                    skiff::scene::semantic_action::activate{}};
    return out;
  }

  // The press is held: the list closes on it, and the release must not fall
  // through to what was under the list.
  using Node::onPointer;
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    reply.capturePointer();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::up &,
                 skiff::scene::PointerReply &reply) {
    reply.releasePointer();
    reply.handle();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::cancel &,
                 skiff::scene::PointerReply &reply) {
    reply.releasePointer();
    reply.handle();
  }

private:
  std::string fLabel;
  DropdownLook fLook;
  bool fChosen = false;
};

// The open half of a dropdown: a plate with a row per option, the current one
// held at full strength and the one under the pointer lit. The height comes
// from the rows; each row notices the pointer and is hit on its own.
namespace internal {
template <class OnChoose = skiff::scene::NoAction>
class DropdownList : public skiff::scene::Node {
public:
  explicit DropdownList(OnChoose onChoose = {})
      : fOnChoose(std::move(onChoose)) {
    fState.fAutoSizeAxes = skiff::scene::axes::kY;
    // Two above the first row, four below the last, which is where the plate
    // ends.
    fState.fPadding = {kTopInset, 0.0f, kBottomInset, 0.0f};
  }

  void forEachChild(auto &&f) { f(fRows); }

  void setTheme(Theme value) {
    fLook.fTheme = std::move(value);
    this->restyleRows();
  }
  void setRowHeight(float height) {
    if (height != fLook.fRowHeight) {
      fLook.fRowHeight = height;
      this->restyleRows();
    }
  }
  void setFontSize(float size) {
    if (size != fLook.fFontSize) {
      fLook.fFontSize = size;
      this->restyleRows();
    }
  }
  void setRadii(float plate, float row) {
    if (plate != fLook.fPlateRadius || row != fLook.fRowRadius) {
      fLook.fPlateRadius = plate;
      fLook.fRowRadius = row;
      this->restyleRows();
    }
  }
  void setTextInset(float inset) {
    if (inset != fLook.fTextInset) {
      fLook.fTextInset = inset;
      this->restyleRows();
    }
  }

  void setOptions(std::vector<std::string> options) {
    if (options.size() == fRows.size() &&
        std::ranges::equal(options, fRows, {}, {}, &DropdownRow::label)) {
      return;
    }
    fRows.clear();
    for (std::string &label : options) {
      fRows.emplace_back(std::move(label), fLook);
    }
    this->markChosen();
    this->invalidateLayout();
  }

  void setCurrent(int current) {
    if (current == fCurrent) {
      return;
    }
    fCurrent = current;
    this->markChosen();
  }
  [[nodiscard]] int current() const noexcept { return fCurrent; }

  void setExpanded(bool expanded) { this->setVisible(expanded); }
  [[nodiscard]] bool expanded() const noexcept { return fState.fVisible; }

  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    float y = 0.0f;
    for (DropdownRow &row : fRows) {
      row.fState.arrange(0.0f, y);
      skiff::scene::layout(row, box);
      y += row.fState.fBounds.height() + kRowGap;
    }
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr || fRows.empty()) {
      return;
    }
    // Its own backing plate, since what is underneath stays where it is.
    const skiff::paint::Painter p(canvas, *font);
    p.fillRounded(fState.fBounds, fLook.fPlateRadius, fLook.fTheme.fSurface,
                  alpha);
    p.strokeRounded(fState.fBounds, fLook.fPlateRadius, fLook.fTheme.fAccent,
                    1.5f, alpha);
  }

  // A click between two rows is still a click on the list, and is
  // swallowed: an open dropdown covers what is under it.
  [[nodiscard]] bool acceptsInput() const { return fState.fVisible; }
  [[nodiscard]] bool focusable() const { return false; }

  using Node::onPointer;
  // A row's press. Choosing on press is the widget's contract: screens read
  // their action as soon as the press is dispatched.
  void onPointer(skiff::scene::phase::bubble, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    if (const int index = this->rowOf(reply.fTarget); index >= 0) {
      std::invoke(fOnChoose, index);
      reply.handle();
    }
  }
  // The list itself -- the padding and the gaps -- is held, so that a
  // release after the owner closed the list still belongs here.
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    reply.capturePointer();
    reply.handle();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::up &,
                 skiff::scene::PointerReply &reply) {
    reply.releasePointer();
    reply.handle();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::cancel &,
                 skiff::scene::PointerReply &reply) {
    reply.releasePointer();
    reply.handle();
  }

  using Node::onSemantic;
  void onSemantic(skiff::scene::phase::bubble,
                  const skiff::scene::semantic_action::activate &,
                  skiff::scene::Reply &reply) {
    if (const int index = this->rowOf(reply.fTarget); index >= 0) {
      std::invoke(fOnChoose, index);
      reply.handle();
    }
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::list{};
    return out;
  }

  std::vector<DropdownRow> fRows;

private:
  static constexpr float kRowGap = 2.0f;
  static constexpr float kTopInset = 2.0f;
  static constexpr float kBottomInset = 4.0f;

  [[nodiscard]] int rowOf(skiff::scene::NodeId id) const {
    for (std::size_t i = 0; i < fRows.size(); ++i) {
      if (fRows[i].id() == id) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }
  void markChosen() {
    for (std::size_t i = 0; i < fRows.size(); ++i) {
      fRows[i].setChosen(static_cast<int>(i) == fCurrent);
    }
  }
  void restyleRows() {
    for (DropdownRow &row : fRows) {
      row.setLook(fLook);
    }
    this->markDamaged();
  }

  [[no_unique_address]] OnChoose fOnChoose;

public:
  // What choosing a row calls, as held: where a model's list keeps the pick.
  OnChoose &onChoose() noexcept { return fOnChoose; }

private:
  DropdownLook fLook;
  int fCurrent = -1;
};
} // namespace internal

// The dropdown list over an AnyCall of the row's index, taking the action given and erasing it.
template <class OnChoose>
class ErasedDropdownList : public internal::DropdownList<AnyCallFor<void(int), OnChoose>> {
  using Base = internal::DropdownList<AnyCallFor<void(int), OnChoose>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedDropdownList(OnChoose onChoose = {}) : internal::DropdownList<AnyCallFor<void(int), OnChoose>>(AnyCallFor<void(int), OnChoose>(std::move(onChoose))) {}
};
// The dropdown list: made for its action in a release build, over
// an erased call otherwise.
template <class OnChoose = skiff::scene::NoAction>
using DropdownList = std::conditional_t<kErasedActions, ErasedDropdownList<OnChoose>, internal::DropdownList<OnChoose>>;


} // namespace skiff::widgets
