export module skiff.widgets.textarea;

import std;
import skia;
import skiff.paint;
import skiff.scene;
import skiff.nodes.text;
export import skiff.widgets.theme;
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
};
template <class OnSubmit = skiff::scene::NoAction, class Pictures = skiff::nodes::NoPictures>
class TextArea : public skiff::scene::Node {
public:
  using Atom = TextAtom;
  explicit TextArea(std::string placeholder = {}, OnSubmit onSubmit = {})
      : fPlaceholder(std::move(placeholder)), fOnSubmit(std::move(onSubmit)) {
    fState.fHeight = this->heightFor(1);
    fState.setCursor(skiff::scene::cursor::text{});
  }

  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  // Text put in at the caret, over what is selected, as if typed: what a
  // picker gives the field (an emoji, say).
  void insertText(std::string text) { this->insert(std::move(text)); }
  void setText(std::string text) {
    fText = std::move(text);
    fAtoms.clear();
    fCaret = fAnchor = fText.size();
    this->edited();
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
  void insertAtom(std::string shown, std::string target, std::string plain, bool picture = false) {
    if (fSingle) {
      std::erase(shown, '\n');
    }
    const std::size_t size = shown.size();
    this->insert(std::move(shown));
    fAtoms.push_back({fCaret - size, fCaret, std::move(target), std::move(plain), picture});
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
    if (!typed.text.empty()) {
      this->insert(std::string(typed.text));
    }
    reply.handle();
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
    if (press.key == keys::kEnter) {
      if (!fSingle && (shift || !skiff::scene::acts(fOnSubmit))) {
        // In a quote: the next line quoted too; an empty quoted line ends
        // the quote instead, as a chat's field does.
        const std::size_t start = this->lineStartOf(fCaret);
        if (!this->hasSelection() && this->quotedAt(fCaret)) {
          if (fCaret == start + 2 && (fCaret == fText.size() || fText[fCaret] == '\n')) {
            this->erase(start, start + 2);
          } else {
            this->insert("\n> ");
          }
        } else {
          this->insert("\n");
        }
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
      } else if (const std::size_t start = this->lineStartOf(fCaret);
                 fCaret == start + 2 && this->quotedAt(fCaret)) {
        // Right after a quote's "> ": the line a quote no more, its text
        // kept -- not one character of the mark taken.
        this->erase(start, start + 2);
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
      // A line of a quote -- one that starts "> ", and what it wraps onto --
      // drawn as one: a bar at its start on a faint plate in the accent,
      // as a message draws its quotes.
      if (!fMasked && this->quotedAt(line.fStart)) {
        p.fillRect(skia::SkRect::MakeXYWH(box.fLeft, top, box.width(), lineHeight),
                   (fTheme.fAccent & 0x00FFFFFFu) | 0x1F000000u, alpha);
        p.fillRect(skia::SkRect::MakeXYWH(box.fLeft, top, 3.0f, lineHeight), fTheme.fAccent, alpha);
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

  // A line: its text, its atoms' pieces as pills in the accent.
  // Where the line an offset is on starts, after the newline before it.
  [[nodiscard]] std::size_t lineStartOf(std::size_t at) const {
    const std::size_t before = at == 0 ? std::string::npos : fText.rfind('\n', at - 1);
    return before == std::string::npos ? 0 : before + 1;
  }
  // Whether the line an offset is on is a quote: it starts "> ".
  [[nodiscard]] bool quotedAt(std::size_t at) const {
    const std::size_t start = this->lineStartOf(at);
    return fText.compare(start, 2, "> ") == 0;
  }
  void drawLine(skia::SkCanvas *canvas, const skiff::paint::Painter &p, const Line &line, float left, float y,
                float alpha) const {
    std::size_t at = line.fStart;
    const auto plain = [&](std::size_t to) {
      if (to > at) {
        p.text(this->shown(at, to), left + this->xAt(p, line, at), y, fFontSize, fTheme.fText, alpha);
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
          if (const skia::Sp<skia::SkImage> *image = Pictures::picture(one.target); image && *image) {
            const float side = fFontSize * 1.2f;
            skia::SkPaint paint;
            paint.setAlphaf(alpha);
            canvas->drawImageRect(*image, skia::SkRect::MakeXYWH(x + (width - side) * 0.5f, y - fFontSize * 0.98f, side, side),
                                  skia::SkSamplingOptions(skia::SkFilterMode::kLinear), &paint);
          }
          continue;
        }
        skiff::nodes::drawPill(canvas, p, x, y, width, fFontSize, fTheme.fAccent,
                               from == one.first ? Pictures::pill(one.target) : std::nullopt, alpha);
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
    return offset;
  }
  // An atom unmarked: its text what it reads plain, the caret after it.
  void unmark(std::vector<Atom>::iterator atom) {
    const Atom one = *atom;
    fAtoms.erase(atom);
    fAnchor = fCaret = one.first;
    this->eraseText(one.first, one.last);
    fText.insert(one.first, one.plain);
    this->shiftAtoms(one.first, static_cast<std::ptrdiff_t>(one.plain.size()));
    fCaret = fAnchor = one.first + one.plain.size();
    this->edited();
  }
  // Atoms at or past `from` moved by `by` bytes.
  void shiftAtoms(std::size_t from, std::ptrdiff_t by) {
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
      return fText.substr(from, to - from);
    }
    std::string out;
    for (std::size_t at = from; at < to; at = next(fText, at)) {
      out += "•";
    }
    return out;
  }
  [[nodiscard]] float xAt(const skiff::paint::Painter &p, const Line &line, std::size_t offset) const {
    return p.measure(this->shown(line.fStart, std::min(offset, line.fEnd)), fFontSize);
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

  void insert(std::string typed) {
    if (fSingle) {
      std::erase(typed, '\n');
    }
    if (this->hasSelection()) {
      this->erase(this->low(), this->high());
    }
    // Typed inside an atom: it is text again.
    std::erase_if(fAtoms, [&](const Atom &one) { return fCaret > one.first && fCaret < one.last; });
    fText.insert(fCaret, typed);
    this->shiftAtoms(fCaret, static_cast<std::ptrdiff_t>(typed.size()));
    fCaret += typed.size();
    fAnchor = fCaret;
    this->edited();
  }
  void erase(std::size_t from, std::size_t to) {
    this->eraseText(from, to);
    fCaret = fAnchor = from;
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
    const skiff::paint::Painter p(nullptr, *font);
    const auto fits = [&](std::size_t from, std::size_t to) {
      return p.measure(this->shown(from, to), fFontSize) <= width;
    };
    std::size_t start = 0;
    while (true) {
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
  std::string fPlaceholder;
  [[no_unique_address]] OnSubmit fOnSubmit;
  Theme fTheme = theme();
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

} // namespace skiff::widgets
