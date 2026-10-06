export module skiff.widgets.textarea;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes.text;
export import skiff.widgets.theme;
export import skiff.widgets.erased;
// With skiff's shaping, the caret steps over whole characters (UAX #29): an
// emoji sequence or a letter with its marks is one step and one Backspace.
#ifdef SKIFF_TEXT_SHAPING
import alef.grapheme;
#endif

export namespace skiff::widgets {

// Editable text, of one line or several, as every field of a screen: it
// wraps at its width by words (a word too wide broken where it must be),
// grows with its text up to a number of lines and then scrolls to keep the
// caret in view. It draws no fill of its own -- what holds it draws the
// field -- only its text, its selection and a thin blinking caret.
//
// Editing as a desktop's: a press puts the caret, a drag selects, Shift with
// a move extends the selection; Ctrl+A selects all, Ctrl+C, Ctrl+X and
// Ctrl+V copy, cut and paste through skiff::scene's clipboard; Ctrl with
// the arrows, Backspace or Delete goes by words. Enter calls
// `onSubmit(text)` where it acts; otherwise it starts a new line in a field
// of several lines, and is passed on in a field of one. Shift+Enter always
// starts a new line where there are several.
//
// Atoms: ranges of the text the program marks -- a mention picked from a
// list -- drawn as a message draws them, a pill (its picture through
// `Pictures`, as a text's), in the theme's accent. The caret steps over one
// whole; Backspace at its end (Delete at its start) unmarks it, its text
// then what the program said it reads as, plain, to be edited as any.
// What is sent (`onSubmit`, plainText()) has each atom as it reads plain.
struct TextAtom {
  std::size_t first = 0;
  std::size_t last = 0;
  std::string target;  // what it stands for, as a pill's link
  std::string plain;   // the text it reads as, unmarked or sent
  // A picture in the line -- a custom emoji -- in place of its text (an em
  // space the program put in for the room it takes), not a pill.
  bool picture = false;
  // What it is once unmarked, to be edited, where that is not what it reads
  // as: a mention sent by its name, taken apart into its user's id.
  std::string unmarked;
};
// Blocks: what the program makes of whole paragraphs -- a quote, say --
// given as the field's `Blocks` parameter, a type with static members; the
// field itself knows none. For each paragraph (a line up to a newline, and
// what it wraps onto) it asks look(text, start): how many bytes at its start
// are marks, not drawn and taking no room, the caret kept out of them; how
// far its lines stand in; and how much room they keep at the right. It asks
// drawBehind(...) to draw under the lines in view, each given with where its
// paragraph starts and where it is drawn. And a key pressed with nothing
// selected goes to key(text, caret, press) first: an edit it returns -- a
// range replaced, the caret put -- is made instead of what the key would do.
struct BlockLook {
  std::size_t hidden = 0;  // marks at the paragraph's start: not drawn
  float indent = 0.0f;     // how far its lines stand in
  float right = 0.0f;      // room kept at their right
  bool monospace = false;  // drawn in the monospace face: code
  // Not laid out at all: no line, no room, no caret in it -- a code block's
  // closing fence, which a message does not show either.
  bool collapsed = false;
};
// A line in view, for drawBehind: its paragraph's start, where it is drawn.
struct ShownLine {
  std::size_t paragraph = 0;
  float top = 0.0f;
  float bottom = 0.0f;
};
// What a block's key does to the text: [from, to) replaced by `with`, the
// caret put at `caret`, in the text as it is after.
struct TextEdit {
  std::size_t from = 0;
  std::size_t to = 0;
  std::string with;
  std::size_t caret = 0;
};
// Inline formatting: what the program makes of runs of the text -- bold, a
// link -- as its Blocks' `format`, a type of the program's that the field
// keeps but never looks into. The field keeps spans of it beside the text,
// moved along with every edit and kept in each step of undo; of the Blocks
// it asks only how a run with a format looks (runLook) and whether what is
// typed at a span's end goes into it (grows) -- a bold word goes on bold, a
// link stops where it ends. What makes and takes spans away -- a shortcut, a
// mark typed -- is the program's too: its hooks are given the field as it
// is (a FieldView) and give back what to change (a FieldChange), which the
// field makes as one step of undo. A field whose Blocks name no format has
// none, and draws as it did.
struct NoFormat {
  friend bool operator==(const NoFormat &, const NoFormat &) = default;
};
template <class Format>
struct TextSpan {
  std::size_t first = 0;
  std::size_t last = 0;
  Format format{};
  friend bool operator==(const TextSpan &, const TextSpan &) = default;
};
// How a run is drawn: its face, monospace or not, lines under and through
// it, the accent for its colour, and a faint plate behind it. Where spans
// overlap, the run has all of theirs.
struct RunLook {
  skiff::paint::TextFace face{};
  bool monospace = false;
  bool underline = false;
  bool strike = false;
  bool accent = false;
  bool plate = false;
  friend bool operator==(const RunLook &, const RunLook &) = default;
};
[[nodiscard]] constexpr RunLook joined(const RunLook &a, const RunLook &b) noexcept {
  return {.face = {.bold = a.face.bold || b.face.bold, .italic = a.face.italic || b.face.italic},
          .monospace = a.monospace || b.monospace,
          .underline = a.underline || b.underline,
          .strike = a.strike || b.strike,
          .accent = a.accent || b.accent,
          .plate = a.plate || b.plate};
}
// The field as a hook sees it: its text, the selection (the caret and where
// it was started from) and its spans.
template <class Format>
struct FieldView {
  std::string_view text;
  std::size_t caret = 0;
  std::size_t anchor = 0;
  const std::vector<TextSpan<Format>> &spans;
  [[nodiscard]] std::size_t low() const noexcept { return std::min(caret, anchor); }
  [[nodiscard]] std::size_t high() const noexcept { return std::max(caret, anchor); }
  [[nodiscard]] bool selection() const noexcept { return caret != anchor; }
};
// [from, to) replaced by `with`.
struct TextReplace {
  std::size_t from = 0;
  std::size_t to = 0;
  std::string with;
};
// What a hook has the field do: its text replaced in places, each on the
// text as the ones before left it (spans moved along as by typing); then,
// where it says, its spans set anew, and spans added; and the selection put.
template <class Format>
struct FieldChange {
  std::vector<TextReplace> edits;
  std::optional<std::vector<TextSpan<Format>>> spans;
  // Spans put on after the edits (and after `spans`, where it is set): a run
  // its marks made, where the marks were taken out.
  std::vector<TextSpan<Format>> added;
  std::size_t caret = 0;
  std::size_t anchor = 0;
};
// The format a field's Blocks name, NoFormat where they name none.
template <class Blocks>
struct format_of {
  using type = NoFormat;
};
template <class Blocks>
  requires requires { typename Blocks::format; }
struct format_of<Blocks> {
  using type = typename Blocks::format;
};
template <class Blocks>
using FormatOf = typename format_of<Blocks>::type;

// No blocks: every paragraph plain.
struct NoBlocks {
  [[nodiscard]] static BlockLook look(std::string_view, std::size_t) { return {}; }
  static void drawBehind(skia::SkCanvas *, const skiff::paint::Painter &, std::string_view, std::span<const ShownLine>,
                         const skia::SkRect &, const Theme &, float, float) {}
  [[nodiscard]] static std::optional<TextEdit> key(std::string_view, std::size_t, const skiff::scene::key::down &) {
    return std::nullopt;
  }
  // What is typed, as it is put in: as it is.
  [[nodiscard]] static std::optional<TextEdit> typed(std::string_view, std::size_t, std::string_view) { return std::nullopt; }
};
// What is typed, as blocks put it in: theirs where they say, as it is
// where they have nothing to say.
template <class Blocks>
  requires requires(const Blocks &blocks, std::string_view text, std::size_t caret) { blocks.typed(text, caret, text); }
[[nodiscard]] std::optional<TextEdit> typedBy(const Blocks &blocks, std::string_view text, std::size_t caret,
                                              std::string_view typed) {
  return blocks.typed(text, caret, typed);
}
template <class Blocks>
[[nodiscard]] std::optional<TextEdit> typedBy(const Blocks &, std::string_view, std::size_t, std::string_view) {
  return std::nullopt;
}
// A run's look, as the Blocks say; plain where they say nothing.
template <class Blocks, class Format>
  requires requires(const Blocks &blocks, const Format &format) {
    { blocks.runLook(format) } -> std::convertible_to<RunLook>;
  }
[[nodiscard]] RunLook runLookBy(const Blocks &blocks, const Format &format) {
  return blocks.runLook(format);
}
template <class Blocks, class Format>
[[nodiscard]] RunLook runLookBy(const Blocks &, const Format &) {
  return {};
}
// Whether text typed at a span's end goes into it: yes, unless the Blocks
// say otherwise.
template <class Blocks, class Format>
  requires requires(const Blocks &blocks, const Format &format) {
    { blocks.grows(format) } -> std::convertible_to<bool>;
  }
[[nodiscard]] bool growsBy(const Blocks &blocks, const Format &format) {
  return blocks.grows(format);
}
template <class Blocks, class Format>
[[nodiscard]] bool growsBy(const Blocks &, const Format &) {
  return true;
}
// A key, as the Blocks take it with the field as it is -- a selection too:
// a shortcut's change; nothing where it is not theirs.
template <class Blocks, class Format>
  requires requires(const Blocks &blocks, const FieldView<Format> &view, const skiff::scene::key::down &press) {
    { blocks.edit(view, press) } -> std::convertible_to<std::optional<FieldChange<Format>>>;
  }
[[nodiscard]] std::optional<FieldChange<Format>> editBy(const Blocks &blocks, const FieldView<Format> &view,
                                                        const skiff::scene::key::down &press) {
  return blocks.edit(view, press);
}
template <class Blocks, class Format>
[[nodiscard]] std::optional<FieldChange<Format>> editBy(const Blocks &, const FieldView<Format> &,
                                                        const skiff::scene::key::down &) {
  return std::nullopt;
}
// What was just typed, as the Blocks make it over -- a mark closing a run
// made its format: the change; nothing where it stays as typed.
template <class Blocks, class Format>
  requires requires(const Blocks &blocks, const FieldView<Format> &view) {
    { blocks.typedIn(view) } -> std::convertible_to<std::optional<FieldChange<Format>>>;
  }
[[nodiscard]] std::optional<FieldChange<Format>> typedInBy(const Blocks &blocks, const FieldView<Format> &view) {
  return blocks.typedIn(view);
}
template <class Blocks, class Format>
[[nodiscard]] std::optional<FieldChange<Format>> typedInBy(const Blocks &, const FieldView<Format> &) {
  return std::nullopt;
}
// A field's blocks, whatever they are: asked as they would be, through their
// functions -- for a format, the field's spans of it. Erasure, outside a
// release build only.
template <class Format>
struct AnyBlocksOf {
  using format = Format;
  BlockLook (*fLook)(std::string_view, std::size_t) = &NoBlocks::look;
  void (*fDrawBehind)(skia::SkCanvas *, const skiff::paint::Painter &, std::string_view, std::span<const ShownLine>,
                      const skia::SkRect &, const Theme &, float, float) = &NoBlocks::drawBehind;
  std::optional<TextEdit> (*fKey)(std::string_view, std::size_t, const skiff::scene::key::down &) = &NoBlocks::key;
  std::optional<TextEdit> (*fTyped)(std::string_view, std::size_t, std::string_view) = &NoBlocks::typed;
  RunLook (*fRunLook)(const Format &) = [](const Format &) { return RunLook{}; };
  bool (*fGrows)(const Format &) = [](const Format &) { return true; };
  std::optional<FieldChange<Format>> (*fEdit)(const FieldView<Format> &, const skiff::scene::key::down &) =
      [](const FieldView<Format> &, const skiff::scene::key::down &) { return std::optional<FieldChange<Format>>(); };
  std::optional<FieldChange<Format>> (*fTypedIn)(const FieldView<Format> &) =
      [](const FieldView<Format> &) { return std::optional<FieldChange<Format>>(); };
  template <class Blocks> [[nodiscard]] static AnyBlocksOf of() {
    return {&Blocks::look, &Blocks::drawBehind, &Blocks::key,
            [](std::string_view text, std::size_t caret, std::string_view typed) { return typedBy(Blocks{}, text, caret, typed); },
            [](const Format &format) { return runLookBy(Blocks{}, format); },
            [](const Format &format) { return growsBy(Blocks{}, format); },
            [](const FieldView<Format> &view, const skiff::scene::key::down &press) { return editBy(Blocks{}, view, press); },
            [](const FieldView<Format> &view) { return typedInBy(Blocks{}, view); }};
  }
  [[nodiscard]] BlockLook look(std::string_view text, std::size_t start) const { return fLook(text, start); }
  void drawBehind(skia::SkCanvas *canvas, const skiff::paint::Painter &p, std::string_view text,
                  std::span<const ShownLine> lines, const skia::SkRect &box, const Theme &theme, float size,
                  float alpha) const {
    fDrawBehind(canvas, p, text, lines, box, theme, size, alpha);
  }
  [[nodiscard]] std::optional<TextEdit> key(std::string_view text, std::size_t caret,
                                            const skiff::scene::key::down &press) const {
    return fKey(text, caret, press);
  }
  [[nodiscard]] std::optional<TextEdit> typed(std::string_view text, std::size_t caret, std::string_view what) const {
    return fTyped(text, caret, what);
  }
  [[nodiscard]] RunLook runLook(const Format &format) const { return fRunLook(format); }
  [[nodiscard]] bool grows(const Format &format) const { return fGrows(format); }
  [[nodiscard]] std::optional<FieldChange<Format>> edit(const FieldView<Format> &view,
                                                        const skiff::scene::key::down &press) const {
    return fEdit(view, press);
  }
  [[nodiscard]] std::optional<FieldChange<Format>> typedIn(const FieldView<Format> &view) const {
    return fTypedIn(view);
  }
};
// Blocks with no format, erased: as they were before formats.
using AnyBlocks = AnyBlocksOf<NoFormat>;
namespace internal {
template <class OnSubmit = skiff::scene::NoAction, class Pictures = skiff::nodes::NoPictures, class Blocks = NoBlocks>
class TextArea : public skiff::scene::Node {
public:
  using Atom = TextAtom;
  using Format = FormatOf<Blocks>;
  using Span = TextSpan<Format>;
  explicit TextArea(std::string placeholder = {}, OnSubmit onSubmit = {})
      : fPlaceholder(std::move(placeholder)), fOnSubmit(std::move(onSubmit)) {
    fState.fHeight = this->heightFor(1);
    fState.setCursor(skiff::scene::cursor::text{});
  }
  // Made in a theme: the one it is given, not one of the library's.
  TextArea(Theme theme, std::string placeholder, OnSubmit onSubmit = {})
      : TextArea(std::move(placeholder), std::move(onSubmit)) {
    fTheme = std::move(theme);
    fState.fHeight = this->heightFor(1);
  }

  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  // Its pictures and blocks: what an erased field is told of its own.
  void setPictures(Pictures pictures) { fPictures = std::move(pictures); }
  void setBlocks(Blocks blocks) { fBlocks = std::move(blocks); }
  // Text put in at the caret, over what is selected, as if typed: what a
  // picker gives the field (an emoji, say).
  void insertText(std::string text) { this->insert(std::move(text)); }
  void setText(std::string text) {
    fUndo.clear();
    fRedo.clear();
    this->breakRun();
    fText = std::move(text);
    fAtoms.clear();
    fSpans.clear();
    fCaret = fAnchor = fText.size();
    this->edited();
  }
  // Its spans: the program's formats over runs of the text, by offsets in
  // text(). Set: as they are given, those past the text cut to it.
  [[nodiscard]] const std::vector<Span> &spans() const noexcept { return fSpans; }
  void setSpans(std::vector<Span> spans) {
    fSpans = std::move(spans);
    this->keepSpansIn();
    this->edited();
  }
  // The spans by offsets in plainText(): what is sent with it.
  [[nodiscard]] std::vector<Span> plainSpans() const {
    return std::ranges::to<std::vector<Span>>(std::views::transform(fSpans, [this](const Span &one) {
      return Span{this->plainOffset(one.first), this->plainOffset(one.last), one.format};
    }));
  }
  // What is between two offsets selected, as a picker replaces what was
  // typed for it (an @ and a name begun).
  void select(std::size_t from, std::size_t to) {
    fAnchor = std::min(from, fText.size());
    fCaret = std::min(to, fText.size());
    this->showCaret();
  }
  // An atom put in at the caret, over what is selected: `shown` drawn as a
  // pill for `target`, read as `plain`.
  void insertAtom(std::string shown, std::string target, std::string plain, bool picture = false,
                  std::string unmarked = {}) {
    if (fSingle) {
      std::erase(shown, '\n');
    }
    const std::size_t size = shown.size();
    this->insert(std::move(shown));
    fAtoms.push_back({fCaret - size, fCaret, std::move(target), std::move(plain), picture, std::move(unmarked)});
    std::ranges::sort(fAtoms, {}, &Atom::first);
    this->markDamaged();
  }
  [[nodiscard]] const std::vector<Atom> &atoms() const noexcept { return fAtoms; }
  // The text with each atom as it reads plain: what is sent.
  [[nodiscard]] std::string plainText() const {
    std::string out;
    std::size_t at = 0;
    for (const Atom &one : fAtoms) {
      out.append(fText, at, one.first - at);
      out += one.plain;
      at = one.last;
    }
    out.append(fText, at);
    return out;
  }
  // One line only: no wrapping, a newline never typed.
  void setSingleLine(bool single) {
    fMaxLines = single ? 1 : std::max(fMaxLines, 8);
    fSingle = single;
    this->invalidateLayout();
  }
  // A dot for each character, as for a password; nothing of it copied.
  void setMasked(bool masked) {
    fMasked = masked;
    this->invalidateLayout();
  }
  void setMaxLines(int lines) {
    fMaxLines = std::max(1, lines);
    this->invalidateLayout();
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    this->markDamaged();
  }
  void setFontSize(float size) {
    fFontSize = size;
    this->invalidateLayout();
  }
  [[nodiscard]] bool hasSelection() const noexcept { return fCaret != fAnchor; }

  void measure(const skia::SkRect &parent) {
    const float width = fState.fRelativeSizeAxes.has<skiff::scene::axis::x>()
                            ? parent.width() * fState.fWidth
                            : fState.fWidth;
    this->wrap(width - fState.fPadding.fLeft - fState.fPadding.fRight);
    fState.fHeight = this->heightFor(
        std::clamp(static_cast<int>(fLines.size()), 1, fMaxLines));
  }

  [[nodiscard]] bool acceptsInput() const { return true; }
  // A text field is what a press gives the focus to.
  [[nodiscard]] bool takesFocusOnPress() const { return true; }
  // The caret blinks, 530 on and 530 off: a frame at each turn, asked for
  // when it is due -- not a frame at a time while it is focused.
  // Ticked while it has the caret.
  [[nodiscard]] bool wantsTick() const { return this->focused(); }
  [[nodiscard]] double wakeAt() const {
    if (!this->focused()) {
      return std::numeric_limits<double>::infinity();
    }
    const double since = fNowMs - fCaretSinceMs;
    return fNowMs + (530.0 - std::fmod(since, 530.0));
  }
  void update(double nowMs) {
    fNowMs = nowMs;
    const bool shown =
        this->focused() && std::fmod(nowMs - fCaretSinceMs, 1060.0) < 530.0;
    if (shown != fCaretShown) {
      fCaretShown = shown;
      // A blink repaints the caret, where it was drawn -- not the whole
      // field, which a blink left as it was.
      if (fCaretRect.isEmpty()) {
        this->markDamaged();
      } else {
        fState.fMovedDamage = skiff::scene::joined(fState.fMovedDamage, fCaretRect.makeOutset(1.0f, 1.0f));
        skiff::scene::work::mark(fState.fId);
      }
    }
  }

  // A press puts the caret, and a drag from it selects.
  using Node::onPointer;
  // Past its lines, the wheel scrolls it: three lines a notch, as a
  // scroll view's 60 pixels. At an end, the wheel goes on to what holds it.
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::scroll &wheel,
                 skiff::scene::PointerReply &reply) {
    const int before = this->firstShown();
    fFirst = before - static_cast<int>(std::lround(wheel.dy * 3.0f));
    if (this->firstShown() != before) {
      this->markDamaged();
      reply.handle();
    }
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::down &at,
                 skiff::scene::PointerReply &reply) {
    // A right press -- a long press, on a phone: the text menu asked of the
    // host, the caret where it was pressed unless that is in what is
    // selected, which the menu's Cut and Copy then take.
    if (at.button == 3) {
      const std::size_t pressed = this->outOfAtoms(this->offsetAt(at.x, at.y));
      if (!this->hasSelection() || pressed < this->low() || pressed > this->high()) {
        this->breakRun();
        fCaret = fAnchor = pressed;
      }
      this->showCaret();
      skiff::scene::textMenusAsked().push_back(skiff::scene::text_menu::of_field{.selection = this->hasSelection(), .masked = fMasked});
      reply.handle();
      return;
    }
    this->breakRun();  // the caret put elsewhere: what is typed next is a step of its own
    fCaret = this->outOfAtoms(this->offsetAt(at.x, at.y));
    fAnchor = fCaret;
    fDragging = true;
    reply.capturePointer();
    this->showCaret();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::move &at,
                 skiff::scene::PointerReply &reply) {
    if (!fDragging) {
      return;
    }
    fCaret = this->outOfAtoms(this->offsetAt(at.x, at.y));
    this->showCaret();
    reply.handle();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::up &,
                 skiff::scene::PointerReply &reply) {
    fDragging = false;
    reply.releasePointer();
  }
  void onPointer(skiff::scene::phase::target, const skiff::scene::pointer::cancel &,
                 skiff::scene::PointerReply &reply) {
    fDragging = false;
    reply.releasePointer();
  }
  [[nodiscard]] bool onClick(float, float) { return true; }

  using Node::onText;
  void onText(skiff::scene::phase::target, const skiff::scene::text::commit &typed,
              skiff::scene::Reply &reply) {
    // What was being composed is what the commit puts in: taken out first.
    this->dropComposition();
    // The program's blocks first, as for a key: what is typed may become
    // something else where it goes -- "--" a dash.
    if (!typed.text.empty() && !fMasked && !fSingle && !this->hasSelection()) {
      if (std::optional<TextEdit> edit = typedBy(fBlocks, fText, fCaret, typed.text)) {
        this->applyEdit(*edit);
        reply.handle();
        return;
      }
    }
    if (!typed.text.empty()) {
      this->insert(std::string(typed.text));
      // What was typed, as the program makes it over: a closing mark.
      if (!fMasked && !fSingle) {
        if (std::optional<FieldChange<Format>> change = typedInBy(fBlocks, this->view())) {
          this->applyChange(std::move(*change));
        }
      }
    }
    reply.handle();
  }
  // A hook's change made: a step of its own to undo.
  void applyChange(FieldChange<Format> change) {
    this->remember(false);
    this->breakRun();
    for (const TextReplace &one : change.edits) {
      const std::size_t from = std::min(one.from, fText.size());
      const std::size_t to = std::clamp(one.to, from, fText.size());
      this->eraseText(from, to);
      fText.insert(from, one.with);
      this->shiftAtoms(from, static_cast<std::ptrdiff_t>(one.with.size()));
    }
    if (change.spans) {
      fSpans = std::move(*change.spans);
    }
    std::ranges::move(change.added, std::back_inserter(fSpans));
    this->keepSpansIn();
    fCaret = this->outOfAtoms(std::min(change.caret, fText.size()), true);
    fAnchor = std::min(change.anchor, fText.size());
    this->edited();
  }
  // An input method's composition -- a phone keyboard's word as it is typed,
  // before it is committed: in the text at the caret, in place of the one
  // before, so that it is seen as it is typed; taken out again as the
  // commit comes, which puts in what was chosen. A field that took only
  // commits showed nothing of a word typed on Gboard until a space.
  void onText(skiff::scene::phase::target, const skiff::scene::text::compose &composing,
              skiff::scene::Reply &reply) {
    this->dropComposition();
    if (!composing.text.empty()) {
      const std::size_t from = this->hasSelection() ? this->low() : fCaret;
      this->insert(std::string(composing.text));
      fComposed = std::pair{from, fCaret};
    }
    reply.handle();
  }
  void dropComposition() {
    if (!fComposed) {
      return;
    }
    const auto [from, to] = *std::exchange(fComposed, std::nullopt);
    this->eraseText(from, std::min(to, fText.size()));
    fCaret = fAnchor = from;
    this->edited();
  }
  // A block's edit put in: a step of its own to undo.
  void applyEdit(const TextEdit &edit) {
    this->remember(false);
    this->breakRun();
    this->eraseText(edit.from, edit.to);
    fText.insert(edit.from, edit.with);
    this->shiftAtoms(edit.from, static_cast<std::ptrdiff_t>(edit.with.size()));
    fCaret = fAnchor = this->outOfAtoms(std::min(edit.caret, fText.size()), true);
    this->edited();
  }

  using Node::onKey;
  void onKey(skiff::scene::phase::target, const skiff::scene::key::down &press,
             skiff::scene::Reply &reply) {
    namespace keys = skiff::scene::keys;
    namespace modifier = skiff::scene::modifier;
    const bool shift = press.modifiers.has<modifier::shift>();
    const bool control = press.modifiers.has<modifier::control>();
    const auto move = [&](std::size_t to) {
      fCaret = this->outOfAtoms(to, to > fCaret);
      if (!shift) {
        fAnchor = fCaret;
      }
    };
    // Back a step, and forward again: Ctrl+Z; Ctrl+Shift+Z or Ctrl+Y.
    if (control && press.key == keys::kZ) {
      if (shift) {
        this->redo();
      } else {
        this->undo();
      }
      reply.handle();
      return;
    }
    if (control && press.key == keys::kY) {
      this->redo();
      reply.handle();
      return;
    }
    // The caret moved: what is typed after it is a step of its own.
    if (press.key == keys::kLeft || press.key == keys::kRight || press.key == keys::kUp || press.key == keys::kDown ||
        press.key == keys::kHome || press.key == keys::kEnd) {
      this->breakRun();
    }
    // The program's formats first: a shortcut, with what is selected.
    if (!fMasked && !fSingle) {
      if (std::optional<FieldChange<Format>> change = editBy(fBlocks, this->view(), press)) {
        this->applyChange(std::move(*change));
        reply.handle();
        return;
      }
    }
    // The program's blocks next: a key that means something in one of
    // them -- Enter in a quote -- is theirs.
    if (!fMasked && !fSingle && !this->hasSelection()) {
      if (std::optional<TextEdit> edit = fBlocks.key(fText, fCaret, press)) {
        this->applyEdit(*edit);
        reply.handle();
        return;
      }
    }
    if (press.key == keys::kEnter) {
      if (!fSingle && (shift || !skiff::scene::acts(fOnSubmit))) {
        this->insert("\n");
      } else if (skiff::scene::acts(fOnSubmit)) {
        // Once for a press: Enter held a moment repeats, and each repeat
        // came before the program had emptied the field -- what was written
        // sent twice.
        if (press.repeat) {
          reply.handle();
          return;
        }
        std::invoke(fOnSubmit, std::string_view(this->plainText()));
      } else {
        return; // a form's to act on
      }
    } else if (control && press.key == keys::kA) {
      fAnchor = 0;
      fCaret = fText.size();
    } else if (control && (press.key == keys::kC || press.key == keys::kX)) {
      // Nothing selected here: not this field's to take -- the window may
      // copy what is selected elsewhere.
      if (!this->hasSelection() || fMasked) {
        return;
      }
      skiff::scene::setClipboardText(this->selected());
      if (press.key == keys::kX) {
        this->erase(this->low(), this->high());
      }
    } else if (control && press.key == keys::kV) {
      std::string pasted = skiff::scene::clipboardText();
      if (fSingle) {
        std::ranges::replace(pasted, '\n', ' ');
      }
      this->insert(pasted);
    } else if (press.key == keys::kBackspace) {
      if (this->hasSelection()) {
        this->erase(this->low(), this->high());
      } else if (const auto atom = std::ranges::find(fAtoms, fCaret, &Atom::last); atom != fAtoms.end()) {
        this->unmark(atom);
      } else if (fCaret > 0) {
        this->erase(control ? wordBefore(fText, fCaret) : previous(fText, fCaret), fCaret);
      }
    } else if (press.key == keys::kDelete) {
      if (this->hasSelection()) {
        this->erase(this->low(), this->high());
      } else if (const auto atom = std::ranges::find(fAtoms, fCaret, &Atom::first); atom != fAtoms.end()) {
        const std::size_t first = atom->first;
        this->unmark(atom);
        fCaret = fAnchor = first;
      } else if (fCaret < fText.size()) {
        this->erase(fCaret, control ? wordAfter(fText, fCaret) : next(fText, fCaret));
      }
    } else if (press.key == keys::kLeft) {
      if (this->hasSelection() && !shift) {
        move(this->low());
      } else {
        move(control ? wordBefore(fText, fCaret) : previous(fText, fCaret));
      }
    } else if (press.key == keys::kRight) {
      if (this->hasSelection() && !shift) {
        move(this->high());
      } else {
        move(control ? wordAfter(fText, fCaret) : next(fText, fCaret));
      }
    } else if (press.key == keys::kHome) {
      move(control || fLines.empty() ? 0 : fLines[this->lineOf(fCaret)].fStart);
    } else if (press.key == keys::kEnd) {
      move(control || fLines.empty() ? fText.size() : fLines[this->lineOf(fCaret)].fEnd);
    } else if (press.key == keys::kUp || press.key == keys::kDown) {
      // With Ctrl, or past the first or the last line: not the field's --
      // the window's, as a chat's input gives Up to editing the last message
      // and Ctrl+Up to answering one.
      if (fSingle || control) {
        return;
      }
      const std::size_t to = this->lineMoved(press.key == keys::kUp ? -1 : 1);
      if (to == fCaret) {
        return;
      }
      move(to);
    } else {
      return;
    }
    this->showCaret();
    reply.handle();
  }

  [[nodiscard]] skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics out;
    out.fRole = skiff::scene::semantic_role::text_box{};
    out.fLabel = fPlaceholder;
    if (!fMasked) {
      out.fValue = fText;
    }
    out.fActions = {skiff::scene::semantic_action::focus{}};
    return out;
  }

  void drawSelf(skia::SkCanvas *canvas, float alpha) {
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return;
    }
    const skiff::paint::Painter p(canvas, *font);
    const skia::SkRect box = this->textBox();
    const int save = canvas->save();
    canvas->clipRect(box, true);
    const float lineHeight = fFontSize * kLineSpacing;
    const float shift = this->scrollX(p);
    if (fText.empty()) {
      p.text(fPlaceholder, box.fLeft, box.fTop + kPadY + fFontSize, fFontSize,
             fTheme.fTextFaint, alpha);
    }
    const int first = this->firstShown();
    const std::size_t low = this->low(), high = this->high();
    // Under the lines in view, what the program's blocks draw there.
    if (!fMasked && !fSingle) {
      std::vector<ShownLine> shownLines;
      for (int i = first; i < static_cast<int>(fLines.size()); ++i) {
        const float top = box.fTop + kPadY + static_cast<float>(i - first) * lineHeight;
        shownLines.push_back({this->lineStartOf(fLines[static_cast<std::size_t>(i)].fStart), top, top + lineHeight});
      }
      fBlocks.drawBehind(canvas, p, fText, shownLines, box, fTheme, fFontSize, alpha);
    }
    for (int i = first; i < static_cast<int>(fLines.size()); ++i) {
      const Line &line = fLines[static_cast<std::size_t>(i)];
      const float top = box.fTop + kPadY + static_cast<float>(i - first) * lineHeight;
      if (low != high && low <= line.fEnd && high >= line.fStart) {
        const std::size_t from = std::max(low, line.fStart);
        const std::size_t to = std::min(high, line.fEnd);
        const float x0 = this->xAt(p, line, from) - shift;
        float x1 = this->xAt(p, line, to) - shift;
        if (high > line.fEnd) {
          x1 += fFontSize * 0.3f; // the newline, selected
        }
        p.fillRounded(skia::SkRect::MakeLTRB(box.fLeft + x0, top, box.fLeft + x1, top + lineHeight),
                      2.0f, fTheme.fAccent, alpha * 0.35f);
      }
      this->drawLine(canvas, p, line, box.fLeft - shift, top + fFontSize, alpha);
    }
    // Where the caret is, known while it is off too: a blink repaints it
    // there.
    if (this->focused() && !fLines.empty()) {
      const std::size_t at = this->lineOf(fCaret);
      const float x = this->xAt(p, fLines[at], fCaret) - shift;
      const float top = box.fTop + kPadY + static_cast<float>(static_cast<int>(at) - first) * lineHeight;
      fCaretRect = skia::SkRect::MakeXYWH(box.fLeft + x, top + 1.0f, 1.2f, fFontSize + 3.0f);
      if (fCaretShown) {
        p.fillRect(fCaretRect, fTheme.fText, alpha);
      }
    }
    canvas->restoreToCount(save);
  }

private:
  static constexpr float kPadY = 6.0f;
  static constexpr float kLineSpacing = 1.3f;

  // A line as drawn: the bytes of the text it shows. A line ends at a
  // newline (which it does not show) or where the next word no longer fits.
  struct Line {
    std::size_t fStart = 0;
    std::size_t fEnd = 0;
  };

  // How the paragraph an offset is in looks, as the program's blocks say.
  [[nodiscard]] BlockLook lookAt(std::size_t at) const {
    return fMasked || fSingle ? BlockLook{} : fBlocks.look(fText, this->lineStartOf(at));
  }
  // How far a line's text stands in: its paragraph's indent.
  [[nodiscard]] float indentOf(const Line &line) const { return this->lookAt(line.fStart).indent; }
  // What a line is drawn and measured with: the monospace face where its
  // paragraph is code, else the field's own.
  [[nodiscard]] skiff::paint::Painter painterFor(const skiff::paint::Painter &p, const Line &line) const {
    return skiff::paint::Painter(p.canvas(), *skiff::paint::defaultFont(), this->lookAt(line.fStart).monospace);
  }
  // A line: its text, its atoms' pieces as pills in the accent.
  // Where the line an offset is on starts, after the newline before it.
  [[nodiscard]] std::size_t lineStartOf(std::size_t at) const {
    const std::size_t before = at == 0 ? std::string::npos : fText.rfind('\n', at - 1);
    return before == std::string::npos ? 0 : before + 1;
  }
  void drawLine(skia::SkCanvas *canvas, const skiff::paint::Painter &field, const Line &line, float left, float y,
                float alpha) const {
    const skiff::paint::Painter p = this->painterFor(field, line);
    std::size_t at = line.fStart;
    // The text up to `to`, run by run, each as its formats look.
    const auto plain = [&](std::size_t to) {
      for (const auto &[from, till, look] : this->runsOf(at, to)) {
        const float x = left + this->xAt(p, line, from);
        const float width = this->xAt(p, line, till) - this->xAt(p, line, from);
        if (look.plate) {
          p.fillRounded(skia::SkRect::MakeXYWH(x - 1.0f, y - fFontSize, width + 2.0f, fFontSize * 1.3f), 3.0f,
                        fTheme.fTextFaint, alpha * 0.3f);
        }
        const skia::SkColor colour = look.accent ? fTheme.fAccent : fTheme.fText;
        this->painterOf(p, look).text(this->shown(from, till), x, y, fFontSize, colour, alpha, look.face);
        if (look.underline) {
          p.fillRect(skia::SkRect::MakeXYWH(x, y + fFontSize * 0.14f, width, 1.0f), colour, alpha);
        }
        if (look.strike) {
          p.fillRect(skia::SkRect::MakeXYWH(x, y - fFontSize * 0.3f, width, 1.0f), colour, alpha);
        }
      }
    };
    if (!fMasked) {
      for (const Atom &one : fAtoms) {
        if (one.last <= line.fStart || one.first >= line.fEnd) {
          continue;
        }
        const std::size_t from = std::max(one.first, line.fStart), to = std::min(one.last, line.fEnd);
        plain(from);
        const float x = left + this->xAt(p, line, from);
        const float width = this->xAt(p, line, to) - this->xAt(p, line, from);
        at = to;
        if (one.picture) {
          // Its picture, square, as a message's text draws one.
          if (const skia::Sp<skia::SkImage> *image = fPictures.picture(one.target); image && *image) {
            const float side = fFontSize * 1.2f;
            skia::SkPaint paint;
            paint.setAlphaf(alpha);
            canvas->drawImageRect(*image, skia::SkRect::MakeXYWH(x + (width - side) * 0.5f, y - fFontSize * 0.98f, side, side),
                                  skia::SkSamplingOptions(skia::SkFilterMode::kLinear), &paint);
          }
          continue;
        }
        skiff::nodes::drawPill(canvas, p, x, y, width, fFontSize, fTheme.fAccent,
                               from == one.first ? fPictures.pill(one.target) : std::nullopt, alpha);
        p.text(this->shown(from, to), x, y, fFontSize, fTheme.fAccent, alpha);
      }
    }
    plain(line.fEnd);
  }
  // An offset out of any atom it falls inside: to the atom's end it goes
  // towards, or its nearer.
  [[nodiscard]] std::size_t outOfAtoms(std::size_t offset, std::optional<bool> forward = std::nullopt) const {
    for (const Atom &one : fAtoms) {
      if (offset > one.first && offset < one.last) {
        const bool after = forward ? *forward : offset - one.first > one.last - offset;
        return after ? one.last : one.first;
      }
    }
    // Never before or inside a paragraph's hidden marks: to after them, or
    // -- going back from them -- to the end of the line before. A paragraph
    // all marks, or not laid out, has no place for the caret at all: it goes
    // on past it, to the next paragraph or back to the one before -- the
    // caret stood still on a code block's first line, sent back into its
    // hidden fence each time it left.
    const std::size_t start = this->lineStartOf(offset);
    const BlockLook look = this->lookAt(start);
    const std::size_t marks = start + look.hidden;
    const std::size_t newline = fText.find('\n', start);
    const std::size_t end = newline == std::string::npos ? fText.size() : newline;
    const bool back = forward.has_value() && !*forward;
    if (look.collapsed || (look.hidden > 0 && marks >= end)) {
      if (back && start > 0)
        return this->outOfAtoms(start - 1, false);
      if (newline != std::string::npos)
        return this->outOfAtoms(newline + 1, true);
      return start > 0 ? this->outOfAtoms(start - 1, false) : offset;
    }
    if (offset < marks)
      return back && start > 0 ? start - 1 : marks;
    return offset;
  }
  // An atom unmarked: its text what it reads plain, the caret after it.
  void unmark(std::vector<Atom>::iterator atom) {
    const Atom one = *atom;
    fAtoms.erase(atom);
    fAnchor = fCaret = one.first;
    this->eraseText(one.first, one.last);
    const std::string &text = one.unmarked.empty() ? one.plain : one.unmarked;
    fText.insert(one.first, text);
    this->shiftAtoms(one.first, static_cast<std::ptrdiff_t>(text.size()));
    fCaret = fAnchor = one.first + text.size();
    this->edited();
  }
  // Atoms at or past `from` moved by `by` bytes.
  void shiftAtoms(std::size_t from, std::ptrdiff_t by) {
    // Text put in: the spans too. (Text taken out moves them in eraseText.)
    if (by > 0) {
      this->spansInserted(from, static_cast<std::size_t>(by));
    }
    for (Atom &one : fAtoms) {
      if (one.first >= from) {
        one.first = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(one.first) + by);
        one.last = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(one.last) + by);
      }
    }
  }
  // Text taken out: atoms it cuts into gone, those after it moved back.
  void eraseText(std::size_t from, std::size_t to) {
    fText.erase(from, to - from);
    this->spansErased(from, to);
    std::erase_if(fAtoms, [&](const Atom &one) { return one.first < to && one.last > from; });
    this->shiftAtoms(to, -static_cast<std::ptrdiff_t>(to - from));
  }
  [[nodiscard]] float heightFor(int lines) const {
    return static_cast<float>(lines) * fFontSize * kLineSpacing + 2.0f * kPadY;
  }
  [[nodiscard]] std::size_t low() const noexcept { return std::min(fCaret, fAnchor); }
  [[nodiscard]] std::size_t high() const noexcept { return std::max(fCaret, fAnchor); }
  [[nodiscard]] std::string selected() const { return fText.substr(this->low(), this->high() - this->low()); }

  // What is drawn for the text between two offsets: itself, or a dot for
  // each character.
  [[nodiscard]] std::string shown(std::size_t from, std::size_t to) const {
    if (!fMasked) {
      // A paragraph's marks, as its block says, are not drawn and take no
      // room: the block shows what they mean instead.
      std::string out;
      std::size_t at = from;
      while (at < to) {
        const std::size_t start = this->lineStartOf(at);
        if (const std::size_t marks = start + this->lookAt(start).hidden; at < marks) {
          at = std::min(to, marks);
          continue;
        }
        const std::size_t newline = fText.find('\n', at);
        const std::size_t stop = newline == std::string::npos ? to : std::min(to, newline + 1);
        out.append(fText, at, stop - at);
        at = stop;
      }
      return out;
    }
    std::string out;
    for (std::size_t at = from; at < to; at = next(fText, at)) {
      out += "•";
    }
    return out;
  }
  [[nodiscard]] float xAt(const skiff::paint::Painter &field, const Line &line, std::size_t offset) const {
    const skiff::paint::Painter p = this->painterFor(field, line);
    return this->indentOf(line) + this->widthOf(p, line.fStart, std::min(offset, line.fEnd));
  }
  // How wide the text between two offsets is drawn: run by run, each in its
  // face.
  [[nodiscard]] float widthOf(const skiff::paint::Painter &p, std::size_t from, std::size_t to) const {
    if (fSpans.empty() || fMasked) {
      return p.measure(this->shown(from, to), fFontSize);
    }
    float width = 0.0f;
    for (const auto &[a, b, look] : this->runsOf(from, to)) {
      width += this->painterOf(p, look).measure(this->shown(a, b), fFontSize, look.face);
    }
    return width;
  }
  // A run's painter: the monospace face where its look asks for it.
  [[nodiscard]] skiff::paint::Painter painterOf(const skiff::paint::Painter &p, const RunLook &look) const {
    return look.monospace ? skiff::paint::Painter(p.canvas(), *skiff::paint::defaultFont(), true) : p;
  }
  // A run: [from, to) all of one look.
  struct Run {
    std::size_t from = 0;
    std::size_t to = 0;
    RunLook look;
  };
  // The runs between two offsets: cut where a span starts or ends.
  [[nodiscard]] std::vector<Run> runsOf(std::size_t from, std::size_t to) const {
    if (to <= from) {
      return {};
    }
    if (fSpans.empty() || fMasked) {
      return {Run{from, to, {}}};
    }
    std::vector<std::size_t> cuts{from, to};
    for (const Span &one : fSpans) {
      for (const std::size_t at : {one.first, one.last}) {
        if (at > from && at < to) {
          cuts.push_back(at);
        }
      }
    }
    std::ranges::sort(cuts);
    cuts.erase(std::ranges::unique(cuts).begin(), cuts.end());
    return std::ranges::to<std::vector<Run>>(std::views::transform(std::views::pairwise(cuts), [this](const auto &pair) {
      const auto &[a, b] = pair;
      return Run{a, b, this->lookOf(a)};
    }));
  }
  // How the character at an offset looks: all its spans' looks.
  [[nodiscard]] RunLook lookOf(std::size_t at) const {
    RunLook look;
    for (const Span &one : fSpans) {
      if (one.first <= at && at < one.last) {
        look = joined(look, runLookBy(fBlocks, one.format));
      }
    }
    return look;
  }
  // The field as a hook sees it.
  [[nodiscard]] FieldView<Format> view() const { return {fText, fCaret, fAnchor, fSpans}; }
  // An offset in the text, as it is in plainText(): atoms before it as long
  // as they read; one inside an atom at its start.
  [[nodiscard]] std::size_t plainOffset(std::size_t at) const {
    std::size_t out = at;
    for (const Atom &one : fAtoms) {
      if (one.last <= at) {
        out = out + one.plain.size() - (one.last - one.first);
      } else if (one.first < at) {
        out -= at - one.first;
      }
    }
    return out;
  }
  // Spans moved for text put in at `at`: on past it, or longer where it went
  // into one -- at a span's end too, where the format grows.
  void spansInserted(std::size_t at, std::size_t size) {
    for (Span &one : fSpans) {
      if (one.first >= at) {
        one.first += size;
        one.last += size;
      } else if (one.last > at || (one.last == at && growsBy(fBlocks, one.format))) {
        one.last += size;
      }
    }
  }
  // Spans moved for [from, to) taken out: what was in it gone from them, and
  // a span left empty dropped.
  void spansErased(std::size_t from, std::size_t to) {
    const auto moved = [&](std::size_t at) { return at <= from ? at : at >= to ? at - (to - from) : from; };
    for (Span &one : fSpans) {
      one.first = moved(one.first);
      one.last = moved(one.last);
    }
    std::erase_if(fSpans, [](const Span &one) { return one.first >= one.last; });
  }
  // Spans kept within the text: cut to its end, those left empty dropped.
  void keepSpansIn() {
    for (Span &one : fSpans) {
      one.last = std::min(one.last, fText.size());
      one.first = std::min(one.first, one.last);
    }
    std::erase_if(fSpans, [](const Span &one) { return one.first >= one.last; });
  }
  // Where the text goes: its bounds within its padding at the sides, as a
  // field's plate keeps its text off its edges.
  [[nodiscard]] skia::SkRect textBox() const {
    const skia::SkRect &b = fState.fBounds;
    return skia::SkRect::MakeLTRB(b.fLeft + fState.fPadding.fLeft, b.fTop, b.fRight - fState.fPadding.fRight,
                                  b.fBottom);
  }
  // How far a field of one line is scrolled, so the caret stays in it.
  [[nodiscard]] float scrollX(const skiff::paint::Painter &p) const {
    if (!fSingle || fLines.empty()) {
      return 0.0f;
    }
    const float caret = this->xAt(p, fLines.front(), fCaret);
    const float room = this->textBox().width() - 4.0f;
    return caret > room ? caret - room : 0.0f;
  }

  // What Ctrl+Z goes back to, as Qt's text edit keeps it -- tdesktop's field:
  // the text as it was before each step of editing, with its atoms and its
  // caret; and what Ctrl+Shift+Z or Ctrl+Y goes forward to again. Typing
  // that runs on from where the last typing ended is one step, as is
  // erasing that runs back from where the last erasing began; anything else
  // -- a paste, a cut, a selection replaced, the caret moved -- is a step of
  // its own. Text set by the program is a new start.
  struct Snapshot {
    std::string text;
    std::vector<Atom> atoms;
    std::vector<Span> spans;
    std::size_t caret = 0, anchor = 0;
  };
  std::vector<Snapshot> fUndo, fRedo;
  std::optional<std::size_t> fTypingAt, fErasingAt;
  // The input method's composition in the text, while there is one: from, to.
  std::optional<std::pair<std::size_t, std::size_t>> fComposed;
  static constexpr std::size_t kMostSteps = 200;
  void remember(bool runsOn) {
    if (!runsOn) {
      fUndo.push_back({fText, fAtoms, fSpans, fCaret, fAnchor});
      if (fUndo.size() > kMostSteps) {
        fUndo.erase(fUndo.begin());
      }
    }
    fRedo.clear();
  }
  void breakRun() {
    fTypingAt.reset();
    fErasingAt.reset();
  }
  void restore(Snapshot to) {
    fText = std::move(to.text);
    fAtoms = std::move(to.atoms);
    fSpans = std::move(to.spans);
    fCaret = std::min(to.caret, fText.size());
    fAnchor = std::min(to.anchor, fText.size());
    this->breakRun();
    this->edited();
  }
  void undo() {
    if (fUndo.empty()) {
      return;
    }
    fRedo.push_back({fText, fAtoms, fSpans, fCaret, fAnchor});
    Snapshot back = std::move(fUndo.back());
    fUndo.pop_back();
    this->restore(std::move(back));
  }
  void redo() {
    if (fRedo.empty()) {
      return;
    }
    fUndo.push_back({fText, fAtoms, fSpans, fCaret, fAnchor});
    Snapshot forward = std::move(fRedo.back());
    fRedo.pop_back();
    this->restore(std::move(forward));
  }

  void insert(std::string typed) {
    if (fSingle) {
      std::erase(typed, '\n');
    }
    // A character typed where the last one went: the same step.
    // As tdesktop's field (lib_ui input_field.cpp): Space, Enter, Backspace and
    // Delete each an edit block of their own; letters typed in a row one.
    const bool one = !typed.empty() && typed.size() <= 4 && typed != "\n" && typed != " " && !this->hasSelection();
    this->remember(one && fTypingAt == fCaret);
    if (this->hasSelection()) {
      this->erase(this->low(), this->high());
    }
    // Typed inside an atom: it is text again.
    std::erase_if(fAtoms, [&](const Atom &one) { return fCaret > one.first && fCaret < one.last; });
    fText.insert(fCaret, typed);
    this->shiftAtoms(fCaret, static_cast<std::ptrdiff_t>(typed.size()));
    fCaret += typed.size();
    fAnchor = fCaret;
    fTypingAt = one ? std::optional<std::size_t>(fCaret) : std::nullopt;
    fErasingAt.reset();
    this->edited();
  }
  void erase(std::size_t from, std::size_t to) {
    // Each erasing a step of its own, as tdesktop's Backspace and Delete.
    this->remember(false);
    this->eraseText(from, to);
    fCaret = fAnchor = from;
    fErasingAt = from;
    fTypingAt.reset();
    this->edited();
  }
  void edited() {
    this->showCaret();
    this->invalidateLayout();
    this->markDamaged();
  }
  void showCaret() {
    fCaretSinceMs = fNowMs;
    fCaretShown = this->focused();
    this->markDamaged();
  }

  // The lines of the text at `width`.
  void wrap(float width) {
    fLines.clear();
    skia::SkFont *font = skiff::paint::defaultFont();
    if (fSingle || font == nullptr || width <= 0.0f) {
      fLines.push_back({0, fText.size()});
      return;
    }
    // A block's lines narrower: in by their indent, its room at the right
    // kept; code measured in the monospace face.
    float room = width;
    bool monospace = false;
    const auto fits = [&](std::size_t from, std::size_t to) {
      const skiff::paint::Painter p(nullptr, *font, monospace);
      return this->widthOf(p, from, to) <= room;
    };
    std::size_t start = 0;
    while (true) {
      const BlockLook look = this->lookAt(start);
      // Not laid out: on to the next paragraph, no line made for it.
      if (look.collapsed) {
        const std::size_t newline = fText.find('\n', start);
        if (newline == std::string::npos) {
          if (fLines.empty())
            fLines.push_back({start, fText.size()});
          break;
        }
        start = newline + 1;
        continue;
      }
      monospace = look.monospace;
      room = look.indent + look.right > 0.0f ? std::max(width - look.indent - look.right, fFontSize * 2.0f) : width;
      const std::size_t newline = fText.find('\n', start);
      const std::size_t stop = newline == std::string::npos ? fText.size() : newline;
      std::size_t from = start;
      while (!fits(from, stop)) {
        std::size_t cut = from;
        for (std::size_t at = from; at < stop; at = next(fText, at)) {
          if (fText[at] == ' ' && at > from && fits(from, at)) {
            cut = at;
          }
        }
        if (cut == from) {
          cut = next(fText, from);
          while (cut < stop && fits(from, next(fText, cut))) {
            cut = next(fText, cut);
          }
          fLines.push_back({from, cut});
          from = cut;
        } else {
          fLines.push_back({from, cut});
          from = cut + 1;
        }
      }
      fLines.push_back({from, stop});
      if (newline == std::string::npos) {
        break;
      }
      start = newline + 1;
    }
  }

  [[nodiscard]] std::size_t lineOf(std::size_t offset) const {
    std::size_t at = 0;
    for (std::size_t i = 0; i < fLines.size(); ++i) {
      if (fLines[i].fStart <= offset) {
        at = i;
      }
    }
    return at;
  }
  // The first line in view: where the wheel left it, while the caret stays
  // where it was; when the caret moves, as little further as brings it in.
  [[nodiscard]] int firstShown() const {
    if (fLines.empty()) {
      return 0;
    }
    if (fCaret != fShownFor) {
      const int caret = static_cast<int>(this->lineOf(fCaret));
      fFirst = std::clamp(fFirst, caret - fMaxLines + 1, caret);
      fShownFor = fCaret;
    }
    fFirst = std::clamp(fFirst, 0, std::max(0, static_cast<int>(fLines.size()) - fMaxLines));
    return fFirst;
  }
  [[nodiscard]] std::size_t offsetAt(float x, float y) const {
    if (fLines.empty()) {
      return fText.size();
    }
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return fText.size();
    }
    const skiff::paint::Painter p(nullptr, *font);
    const float lineHeight = fFontSize * kLineSpacing;
    const int row = this->firstShown() +
                    static_cast<int>(std::floor((y - fState.fBounds.fTop - kPadY) / lineHeight));
    const Line &line =
        fLines[static_cast<std::size_t>(std::clamp(row, 0, static_cast<int>(fLines.size()) - 1))];
    return this->offsetIn(p, line, x - this->textBox().fLeft + this->scrollX(p));
  }
  [[nodiscard]] std::size_t offsetIn(const skiff::paint::Painter &p, const Line &line, float x) const {
    std::size_t best = line.fStart;
    float bestDistance = std::numeric_limits<float>::max();
    for (std::size_t at = line.fStart;; at = next(fText, at)) {
      const float distance = std::abs(this->xAt(p, line, at) - x);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = at;
      }
      if (at >= line.fEnd) {
        break;
      }
    }
    return best;
  }
  [[nodiscard]] std::size_t lineMoved(int by) const {
    if (fLines.empty()) {
      return fCaret;
    }
    const std::size_t at = this->lineOf(fCaret);
    const int to = static_cast<int>(at) + by;
    if (to < 0) {
      return 0;
    }
    if (to >= static_cast<int>(fLines.size())) {
      return fText.size();
    }
    skia::SkFont *font = skiff::paint::defaultFont();
    if (font == nullptr) {
      return fCaret;
    }
    const skiff::paint::Painter p(nullptr, *font);
    return this->offsetIn(p, fLines[static_cast<std::size_t>(to)], this->xAt(p, fLines[at], fCaret));
  }

  [[nodiscard]] static std::size_t previous(std::string_view text, std::size_t at) {
    if (at == 0) {
      return 0;
    }
#ifdef SKIFF_TEXT_SHAPING
    return static_cast<std::size_t>(
        alef::prev_grapheme_boundary(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(at)) - text.begin());
#endif
    --at;
    while (at > 0 && (static_cast<unsigned char>(text[at]) & 0xC0u) == 0x80u) {
      --at;
    }
    return at;
  }
  [[nodiscard]] static std::size_t next(std::string_view text, std::size_t at) {
    if (at >= text.size()) {
      return text.size();
    }
#ifdef SKIFF_TEXT_SHAPING
    return static_cast<std::size_t>(
        alef::next_grapheme_boundary(text.begin() + static_cast<std::ptrdiff_t>(at), text.end()) - text.begin());
#endif
    ++at;
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0u) == 0x80u) {
      ++at;
    }
    return at;
  }
  [[nodiscard]] static bool blank(char c) { return c == ' ' || c == '\n' || c == '\t'; }
  // The start of the word before an offset, and the end of the one after it.
  [[nodiscard]] static std::size_t wordBefore(std::string_view text, std::size_t at) {
    while (at > 0 && blank(text[at - 1])) {
      --at;
    }
    while (at > 0 && !blank(text[at - 1])) {
      --at;
    }
    return at;
  }
  [[nodiscard]] static std::size_t wordAfter(std::string_view text, std::size_t at) {
    while (at < text.size() && blank(text[at])) {
      ++at;
    }
    while (at < text.size() && !blank(text[at])) {
      ++at;
    }
    return at;
  }

  std::string fText;
  std::vector<Atom> fAtoms;
  std::vector<Span> fSpans;
  std::string fPlaceholder;
  [[no_unique_address]] OnSubmit fOnSubmit;
  [[no_unique_address]] Pictures fPictures{};
  [[no_unique_address]] Blocks fBlocks{};
  Theme fTheme;
  float fFontSize = 15.0f;
  int fMaxLines = 8;
  bool fSingle = false;
  bool fMasked = false;
  bool fDragging = false;
  std::vector<Line> fLines;
  std::size_t fCaret = 0;
  // What firstShown() keeps: the first line in view, and the caret it was
  // for -- kept as the view is drawn, so mutable.
  mutable int fFirst = 0;
  mutable std::size_t fShownFor = 0;
  std::size_t fAnchor = 0;
  bool fCaretShown = false;
  // Where the caret was last drawn: what a blink repaints.
  skia::SkRect fCaretRect = skia::SkRect::MakeEmpty();
  double fCaretSinceMs = 0.0;
  double fNowMs = 0.0;
};
} // namespace internal

// The field over an erased call, AnyPictures and AnyBlocks, taking its own and
// erasing them: all of its code but this is that field's, made once.
template <class OnSubmit, class Pictures, class Blocks>
class ErasedTextArea
    : public internal::TextArea<AnyCallFor<void(std::string_view), OnSubmit>, skiff::nodes::AnyPictures, AnyBlocksOf<FormatOf<Blocks>>> {
  using Base = internal::TextArea<AnyCallFor<void(std::string_view), OnSubmit>, skiff::nodes::AnyPictures, AnyBlocksOf<FormatOf<Blocks>>>;

public:
  // Its own handlers, as the wrapper's own: brought in here, so that they
  // are taken for it. Else Node's defaults -- deducing `this`, an exact
  // match for the wrapper -- beat the widget's own, which reach it through
  // the base, and the widget took no key, text or press.
  using Base::onPointer;
  using Base::onKey;
  using Base::onText;
  using Base::onSemantic;
  explicit ErasedTextArea(std::string placeholder = {}, OnSubmit onSubmit = {})
      : Base(std::move(placeholder), AnyCallFor<void(std::string_view), OnSubmit>(std::move(onSubmit))) {
    this->erase();
  }
  ErasedTextArea(Theme theme, std::string placeholder, OnSubmit onSubmit = {})
      : Base(std::move(theme), std::move(placeholder), AnyCallFor<void(std::string_view), OnSubmit>(std::move(onSubmit))) {
    this->erase();
  }

private:
  void erase() {
    this->setPictures(skiff::nodes::AnyPictures::of<Pictures>());
    this->setBlocks(AnyBlocksOf<FormatOf<Blocks>>::template of<Blocks>());
  }
};
// The field: made for its own in a release build, over the erased ones
// otherwise.
template <class OnSubmit = skiff::scene::NoAction, class Pictures = skiff::nodes::NoPictures, class Blocks = NoBlocks>
using TextArea = std::conditional_t<kErasedActions, ErasedTextArea<OnSubmit, Pictures, Blocks>,
                                    internal::TextArea<OnSubmit, Pictures, Blocks>>;


} // namespace skiff::widgets
