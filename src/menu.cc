export module skiff.widgets.menu;

import std;
import skia;
import skiff.paint;
import skiff.scene;
export import skiff.widgets.theme;
export import skiff.widgets.erased;
import skiff.widgets.dropdown;

export namespace skiff::widgets {

// One menu of a MenuBar: its title, and what it offers.
struct Menu {
  std::string fTitle;
  std::vector<std::string> fEntries;
};

// A menu's title on the bar: plain text, brighter under the pointer, and
// with an accent line under it while its menu is open.
class MenuTitle : public skiff::scene::Node {
public:
  MenuTitle(std::string label, const Theme &theme, float height)
      : fLabel(std::move(label)), fTheme(theme) {
    fState.fHeight = height;
  }

  [[nodiscard]] const std::string &label() const noexcept { return fLabel; }
  void setOpen(bool open) {
    if (open != fOpen) {
      fOpen = open;
      this->markDamaged();
    }
  }
  void setBold(bool bold) {
    if (bold != fBold) {
      fBold = bold;
      this->invalidateLayout();
    }
  }

  void measure(const skia::SkRect &) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(nullptr, *font);
    fState.fWidth = p.measure(fLabel, kFontSize, fBold) + 2.0f * kPadX;
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const skia::SkRect &bounds = fState.fBounds;
    const bool lit = fOpen || fState.fHovered;
    p.textIn(bounds, fLabel, kFontSize, lit || fBold ? fTheme.fText : fTheme.fTextDim,
             alpha, fBold, kPadX);
    if (fOpen) {
      p.fillRounded(skia::SkRect::MakeLTRB(bounds.fLeft + kPadX - 4.0f,
                                           bounds.fBottom - 3.0f,
                                           bounds.fRight - kPadX + 4.0f,
                                           bounds.fBottom - 1.0f),
                    1.0f, fTheme.fAccent, alpha);
    }
  }

  [[nodiscard]] bool acceptsInput() const { return true; }
  [[nodiscard]] bool hoverChangesAppearance() const { return true; }
  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::button{};
    out.fLabel = fLabel;
    out.fSelected = fOpen;
    out.fActions = {skiff::scene::semantic_action::activate{}};
    return out;
  }

  // The press is the bar's to act on, in the bubble phase; it is held here
  // so its release does not fall through to what is under the bar.
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
  static constexpr float kFontSize = 14.0f;
  static constexpr float kPadX = 12.0f;

  std::string fLabel;
  Theme fTheme;
  bool fOpen = false;
  bool fBold = false;
};

// A menu bar as GTK draws one: the titles as plain text along a strip at the
// top, a soft shadow under the strip, and under an open title its menu. A
// click on a title opens or closes its menu; while one is open, the pointer
// moving onto another title opens that one instead, and a click anywhere
// else closes it. Choosing an entry calls `onChoose(menu, entry)` and
// closes the menu.
//
// It fills its parent and is drawn over what is under the strip, so it goes
// after it among its parent's children; what is under the strip starts at
// barHeight(). With no menu open, nothing but the titles takes the pointer.
// The first menu's title is bold: the application's own menu.
namespace internal {
template <class OnChoose = skiff::scene::NoAction>
class MenuBar : public skiff::scene::Node {
public:
  explicit MenuBar(OnChoose onChoose = {}) : fOnChoose(std::move(onChoose)) {
    fState.apply({.fill = true});
    fLook.fRowHeight = 30.0f;
    fLook.fFontSize = 14.0f;
    fLook.fTextInset = 14.0f;
    fLook.fDimAlpha = 1.0f;
  }

  static constexpr float kBarHeight = 40.0f;
  [[nodiscard]] static constexpr float barHeight() noexcept { return kBarHeight; }

  void setTheme(Theme value) {
    fLook.fTheme = std::move(value);
    this->setMenus(std::move(fMenus));
  }
  void setBarColour(skia::SkColor colour) {
    fBarColour = colour;
    this->markDamaged();
  }

  void setMenus(std::vector<Menu> menus) {
    fMenus = std::move(menus);
    fTitles.clear();
    for (const Menu &menu : fMenus) {
      fTitles.emplace_back(menu.fTitle, fLook.fTheme, kBarHeight);
    }
    if (!fTitles.empty()) {
      fTitles.front().setBold(true);
    }
    if (fOpen >= static_cast<int>(fMenus.size())) {
      fOpen = -1;
    }
    this->fillRows();
  }
  // One menu's entries again, as when one of them is checked now.
  void setEntries(std::size_t menu, std::vector<std::string> entries) {
    if (menu >= fMenus.size()) {
      return;
    }
    fMenus[menu].fEntries = std::move(entries);
    if (fOpen == static_cast<int>(menu)) {
      this->fillRows();
    }
  }

  [[nodiscard]] int openMenu() const noexcept { return fOpen; }
  void open(int menu) {
    fOpen = menu >= 0 && menu < static_cast<int>(fMenus.size()) ? menu : -1;
    this->fillRows();
  }
  void close() { this->open(-1); }

  void forEachChild(auto &&f) {
    f(fTitles);
    f(fRows);
  }

  void layoutChildren() {
    const skia::SkRect box = fState.contentBox();
    float x = kInsetX;
    for (MenuTitle &title : fTitles) {
      title.fState.arrange(x, 0.0f);
      skiff::scene::layout(title, box);
      x += title.bounds().width();
    }
    fPopup = skia::SkRect::MakeEmpty();
    if (fOpen < 0 || fRows.empty()) {
      return;
    }
    skia::SkFont *font = skiff::paint::defaultFont();
    float widest = 0.0f;
    if (font != nullptr) {
      const skiff::paint::Painter p(nullptr, *font);
      for (const DropdownRow &row : fRows) {
        widest = std::max(widest, p.measure(row.label(), fLook.fFontSize));
      }
    }
    const MenuTitle &title = fTitles[static_cast<std::size_t>(fOpen)];
    const float width = std::max(kMinWidth, widest + 2.0f * fLook.fTextInset + 8.0f);
    const float height = static_cast<float>(fRows.size()) * (fLook.fRowHeight + kRowGap) -
                         kRowGap + 2.0f * kPopupPad;
    const float left = std::min(title.bounds().fLeft, box.fRight - width - 4.0f);
    fPopup = skia::SkRect::MakeXYWH(left, box.fTop + kBarHeight + 2.0f, width, height);
    const skia::SkRect inner =
        skia::SkRect::MakeLTRB(fPopup.fLeft + kPopupPad, fPopup.fTop + kPopupPad,
                               fPopup.fRight - kPopupPad, fPopup.fBottom - kPopupPad);
    float y = 0.0f;
    for (DropdownRow &row : fRows) {
      row.fState.arrange(0.0f, y);
      skiff::scene::layout(row, inner);
      y += row.fState.fBounds.height() + kRowGap;
    }
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    const skia::SkRect box = fState.fBounds;
    const skia::SkRect bar =
        skia::SkRect::MakeXYWH(box.fLeft, box.fTop, box.width(), kBarHeight);
    skiff::paint::verticalGradient(
        canvas,
        skia::SkRect::MakeXYWH(box.fLeft, bar.fBottom, box.width(), kShadow),
        skia::colorSetARGB(90, 0, 0, 0), skia::colorSetARGB(0, 0, 0, 0), alpha);
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    p.fillRounded(bar, 0.0f, fBarColour, alpha);
    if (fPopup.isEmpty()) {
      return;
    }
    p.fillRounded(skia::SkRect::MakeLTRB(fPopup.fLeft, fPopup.fTop + 3.0f, fPopup.fRight,
                                         fPopup.fBottom + 3.0f),
                  kRadius,
                  skia::colorSetARGB(70, 0, 0, 0), alpha);
    p.fillRounded(fPopup, kRadius, fLook.fTheme.fSurface, alpha);
    p.strokeRounded(fPopup, kRadius, fLook.fTheme.fSurfaceActive, 1.0f, alpha);
  }

  // Only an open menu covers the window: a click off it closes it.
  [[nodiscard]] bool acceptsInput() const { return fOpen >= 0; }
  [[nodiscard]] bool focusable() const { return false; }

  using Node::onPointer;
  void onPointer(skiff::scene::phase::bubble, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    if (const int title = this->titleOf(reply.fTarget); title >= 0) {
      this->open(title == fOpen ? -1 : title);
      reply.handle();
      return;
    }
    if (const int row = this->rowOf(reply.fTarget); row >= 0) {
      const int menu = fOpen;
      this->close();
      std::invoke(fOnChoose, menu, row);
      reply.handle();
    }
  }
  void onPointer(skiff::scene::phase::bubble, const skiff::scene::pointer::move &,
                 skiff::scene::PointerReply &) {
    if (const int title = this->titleOf(this->hoveredTitle()); fOpen >= 0 && title >= 0 &&
                                                                title != fOpen) {
      this->open(title);
    }
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &,
                 skiff::scene::PointerReply &reply) {
    this->close();
    reply.handle();
  }

  using Node::onSemantic;
  void onSemantic(skiff::scene::phase::bubble,
                  const skiff::scene::semantic_action::activate &,
                  skiff::scene::Reply &reply) {
    if (const int title = this->titleOf(reply.fTarget); title >= 0) {
      this->open(title == fOpen ? -1 : title);
      reply.handle();
    } else if (const int row = this->rowOf(reply.fTarget); row >= 0) {
      const int menu = fOpen;
      this->close();
      std::invoke(fOnChoose, menu, row);
      reply.handle();
    }
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::group{};
    return out;
  }

private:
  static constexpr float kInsetX = 6.0f;
  static constexpr float kShadow = 8.0f;
  static constexpr float kMinWidth = 180.0f;
  static constexpr float kPopupPad = 5.0f;
  static constexpr float kRowGap = 1.0f;
  static constexpr float kRadius = 8.0f;

  void fillRows() {
    fRows.clear();
    for (std::size_t i = 0; i < fTitles.size(); ++i) {
      fTitles[i].setOpen(static_cast<int>(i) == fOpen);
    }
    if (fOpen >= 0) {
      for (const std::string &entry : fMenus[static_cast<std::size_t>(fOpen)].fEntries) {
        fRows.emplace_back(entry, fLook);
      }
    }
    this->invalidateLayout();
    this->markDamaged();
  }
  [[nodiscard]] skiff::scene::NodeId hoveredTitle() const {
    for (const MenuTitle &title : fTitles) {
      if (title.hovered()) {
        return title.id();
      }
    }
    return 0;
  }
  [[nodiscard]] int titleOf(skiff::scene::NodeId id) const {
    for (std::size_t i = 0; i < fTitles.size(); ++i) {
      if (id != 0 && fTitles[i].id() == id) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }
  [[nodiscard]] int rowOf(skiff::scene::NodeId id) const {
    for (std::size_t i = 0; i < fRows.size(); ++i) {
      if (id != 0 && fRows[i].id() == id) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  [[no_unique_address]] OnChoose fOnChoose;
  DropdownLook fLook;
  skia::SkColor fBarColour = skia::colorSetARGB(255, 36, 40, 44);
  std::vector<Menu> fMenus;
  std::vector<MenuTitle> fTitles;
  std::vector<DropdownRow> fRows;
  int fOpen = -1;
  skia::SkRect fPopup = skia::SkRect::MakeEmpty();
};
} // namespace internal

// The menu bar over an erased call, taking the action given and erasing it.
template <class OnChoose>
class ErasedMenuBar : public internal::MenuBar<AnyCallFor<void(int, int), OnChoose>> {
public:
  explicit ErasedMenuBar(OnChoose onChoose = {}) : internal::MenuBar<AnyCallFor<void(int, int), OnChoose>>(AnyCallFor<void(int, int), OnChoose>(std::move(onChoose))) {}
};
// The menu bar: made for its action in a release build, over an erased call
// otherwise.
template <class OnChoose = skiff::scene::NoAction>
using MenuBar = std::conditional_t<kErasedActions, ErasedMenuBar<OnChoose>, internal::MenuBar<OnChoose>>;


} // namespace skiff::widgets
