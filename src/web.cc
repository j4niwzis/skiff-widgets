// skiff.widgets.web: the controls of skiff-widgets on skiff.web -- a model
// widget's same words (ToggleField, SliderField, TextField, ChoiceTabs,
// ChoiceList, SendButton; read() and onPress()) on the browser's own
// elements, dressed in the widgets' Theme by a stylesheet (themeCss). Each
// shows a part of the model and says what a press changes, as the Skia
// widgets do; what it is on the page is a native control, and what it was
// done to comes in through onDom.
export module skiff.widgets.web;

import std;
import skiff.core;
import skiff.model;
import skiff.bind;
export import skiff.web;
export import skiff.widgets.theme;

export namespace skiff::widgets::web {

using namespace skiff::web;
using skiff::web::Node;

// A colour brought towards white by a share, as skiff.paint lightens one.
[[nodiscard]] inline scene::Colour lighten(scene::Colour c, float amount) {
  const auto up = [&](unsigned v) { return static_cast<unsigned>(std::lround(v + (255.0f - v) * amount)); };
  return (c & 0xff000000u) | (up((c >> 16) & 0xffu) << 16) | (up((c >> 8) & 0xffu) << 8) | up(c & 0xffu);
}
[[nodiscard]] inline std::string themeCss(const Theme &t) {
  const std::string surface = rgba(t.fSurface), hover = rgba(t.fSurfaceHover), active = rgba(t.fSurfaceActive), text = rgba(t.fText),
                    label = rgba(t.fLabel), dim = rgba(t.fTextDim), faint = rgba(t.fTextFaint), accent = rgba(t.fAccent),
                    onAccent = rgba(t.fOnAccent), corner = px(t.fCorner), font = px(t.fFontSize), rowH = px(t.fRowHeight),
                    padX = px(t.fPaddingX), font600 = "600 " + px(t.fFontSize) + " system-ui, sans-serif";
  const std::string chevron =
      "url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6' viewBox='0 0 10 6'%3E%3Cpath d='M0 0l5 6 5-6z' fill='white'/%3E%3C/svg%3E\")";
  std::string css;
  css += ".skiff-text{color:" + label + ";font:" + font + " system-ui,sans-serif;line-height:1.3}.skiff-canvas{display:block}\n";
  css += ".skiff-button{height:" + rowH + ";padding:0 " + padX + ";border:0;border-radius:" + corner + ";background:" + surface + ";color:" + text +
         ";font:" + font600 + ";cursor:pointer}\n";
  css += ".skiff-button:hover{background:" + hover + "}\n";
  css += ".skiff-button:focus-visible{outline:1.5px solid " + accent + ";outline-offset:-1.5px}\n";
  css += ".skiff-button[disabled]{opacity:.5;cursor:default}\n";
  css += ".skiff-tabs{display:inline-flex;gap:" + px(t.fTabGap) + ";border-radius:3px}\n";
  css += ".skiff-tab{background:none;border:0;padding:" + px(t.fTabPadding) + " 0;font:" + px(t.tabFontSize()) +
         " system-ui,sans-serif;color:" + rgba(t.tabTextDim()) + ";cursor:pointer}\n";
  css += ".skiff-tab:hover{color:" + rgba(lighten(t.tabTextDim(), 0.2f)) + "}\n";
  css += ".skiff-tab[aria-selected=true]{color:" + rgba(t.tabText()) + ";font-weight:600" +
         (t.fTabUnderline ? ";box-shadow:inset 0 -2px " + rgba(*t.fTabUnderline) : std::string()) + "}\n";
  css += ".skiff-tabs:focus-within{outline:1px solid " + accent + "}\n";
  css += ".skiff-textbox{height:" + rowH + ";box-sizing:border-box;padding:0 " + padX + ";border:1px solid transparent;border-radius:" + corner +
         ";background:" + surface + ";color:" + text + ";font:" + font + " system-ui,sans-serif;outline:none}\n";
  css += ".skiff-textbox::placeholder{color:" + faint + "}\n";
  css += ".skiff-textbox:focus{border-color:" + accent + "}\n";
  css += ".skiff-select{height:30px;box-sizing:border-box;padding:0 28px 0 " + padX + ";border:1px solid " + active + ";border-radius:" + corner +
         ";background:" + surface + " " + chevron + " no-repeat right 10px center;color:" + text + ";font:" + font +
         " system-ui,sans-serif;appearance:none;-webkit-appearance:none;cursor:pointer}\n";
  css += ".skiff-select:hover{background-color:" + hover + "}\n";
  css += ".skiff-select:focus{border-color:" + accent + ";outline:none}\n";
  css += ".skiff-toggle{appearance:none;-webkit-appearance:none;width:40px;height:22px;border-radius:11px;background:" + surface +
         ";position:relative;margin:0;cursor:pointer;flex:none;transition:background .12s}\n";
  css += ".skiff-toggle::before{content:'';position:absolute;top:3px;left:3px;width:16px;height:16px;border-radius:50%;background:" + text +
         ";transition:left .12s}\n";
  css += ".skiff-toggle:checked{background:" + accent + "}\n";
  css += ".skiff-toggle:checked::before{left:21px}\n";
  css += ".skiff-toggle:focus-visible{outline:1.5px solid " + accent + ";outline-offset:1px}\n";
  css += ".skiff-slider{appearance:none;-webkit-appearance:none;height:16px;background:transparent;margin:0;cursor:pointer;width:100%;min-width:80px;--skiff-track:" +
         surface + "}\n";
  css += ".skiff-slider::-webkit-slider-runnable-track{height:6px;border-radius:3px;background:var(--skiff-track)}\n";
  css += ".skiff-slider::-moz-range-track{height:6px;border-radius:3px;background:" + surface + "}\n";
  css += ".skiff-slider::-moz-range-progress{height:6px;border-radius:3px;background:" + accent + "}\n";
  css += ".skiff-slider::-webkit-slider-thumb{appearance:none;-webkit-appearance:none;width:14px;height:14px;border-radius:50%;background:" + text +
         ";margin-top:-4px}\n";
  css += ".skiff-slider::-moz-range-thumb{width:14px;height:14px;border:0;border-radius:50%;background:" + text + "}\n";
  css += ".skiff-slider:focus-visible{outline:1.5px solid " + accent + ";outline-offset:2px;border-radius:7px}\n";
  css += ".skiff-button.skiff-primary{background:" + accent + ";color:" + onAccent + "}\n";
  css += ".skiff-button.skiff-outlined{background:transparent;box-shadow:inset 0 0 0 2px " + accent + "}\n";
  css += ".skiff-textarea{box-sizing:border-box;min-height:60px;padding:6px " + padX + ";border:1px solid transparent;border-radius:" + corner +
         ";background:" + surface + ";color:" + text + ";font:" + font + " system-ui,sans-serif;resize:vertical;outline:none}\n";
  css += ".skiff-textarea::placeholder{color:" + faint + "}\n";
  css += ".skiff-textarea:focus{border-color:" + accent + "}\n";
  css += ".skiff-textbox.skiff-search{padding-left:28px;background-image:url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' "
         "height='14' viewBox='0 0 24 24' fill='none' stroke='%23999' stroke-width='2'%3E%3Ccircle cx='11' cy='11' r='7'/%3E%3Cpath d='M20 20l-4-4'/%3E%3C/svg%3E\");"
         "background-repeat:no-repeat;background-position:8px center}\n";
  css += ".skiff-pill{display:inline-flex;align-items:center;justify-content:center;white-space:nowrap;font:" + font + " system-ui,sans-serif}\n";
  css += ".skiff-avatar{position:relative;display:block;overflow:hidden;flex:none}\n";
  css += ".skiff-avatar>img{display:block;width:100%;height:100%}\n";
  css += ".skiff-loader{display:inline-block;flex:none;border-radius:50%;background:conic-gradient(" + accent +
         " var(--skiff-done,25%),rgba(255,255,255,.18) 0);animation:skiff-spin 1s linear infinite}\n";
  css += ".skiff-loader[data-stopped]{animation:none}\n@keyframes skiff-spin{to{transform:rotate(360deg)}}\n";
  css += ".skiff-menu{display:inline-flex;align-items:center;gap:8px;color:" + label + ";font:" + font + " system-ui,sans-serif}\n";
  css += ".skiff-scroll{min-height:0}\n";
  css += ".skiff-drawer,.skiff-slideover,.skiff-wallpaper{position:relative;overflow:hidden}\n";
  css += ".skiff-scrim{position:absolute;inset:0;background:rgba(0,0,0,.45)}\n";
  css += ".skiff-sheet{position:absolute;top:0;bottom:0;left:0;transition:transform .22s;box-shadow:0 0 24px rgba(0,0,0,.4)}\n";
  css += ".skiff-slideover>.skiff-panel{position:absolute;inset:0}\n";
  // The page's own rules -- placement, hover, tails -- after the widgets'.
  css += pageCss();
  return css;
}


// The theme's stylesheet put into the document, before a page is mounted
// under `into`: the element it was put in, to drop or replace.
inline dom::Handle mountTheme(dom::Handle into, const Theme &t = theme()) {
  const dom::Handle style = dom::create("style");
  dom::text(style, themeCss(t));
  dom::append(into, style);
  return style;
}


// A checkbox showing a bool part -- or an optional one, unsaid is off -- and
// setting it to the other when changed.
template <class T = bool> class ToggleField : public Node {
public:
  explicit ToggleField(scene::Spec spec = {}) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-toggle");
    to.attr("type", "checkbox");
    to.listen("change");
    to.checked(fNow);
  }
  void read(const T &now) {
    const bool on = onOf(now);
    if (on != fNow || !fShown) {
      fNow = on;
      markDirty();
    }
    fShown = true;
  }
  [[nodiscard]] bool on() const noexcept { return fNow; }
  auto onPress() { return bind::own(model::setTo(T(!fNow))); }

private:
  static bool onOf(bool now) { return now; }
  static bool onOf(const std::optional<bool> &now) { return now.value_or(false); }
  bool fNow = false;
  bool fShown = false;
};

// A range input showing a number part between two bounds and setting it as
// it is dragged: a whole number rounded to the nearest.
template <class T> class SliderField : public Node {
public:
  SliderField(T low, T high, scene::Spec spec = {}) : fLow(low), fHigh(high) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-slider");
    to.attr("type", "range");
    to.attr("min", "0");
    to.attr("max", std::to_string(kSteps));
    to.attr("step", "1");
    to.listen("input");
    to.value(std::to_string(static_cast<int>(std::lround(fFraction * kSteps))));
    // The track filled up to the knob, as the scene draws it: the gradient is
    // the element's own, where the browser's track cannot be told the value.
    const std::string at = number(fFraction * 100.0f) + "%";
    to.style("--skiff-track", "linear-gradient(to right," + rgba(fTheme.fAccent) + " 0 " + at + "," + rgba(fTheme.fSurface) + " " + at + " 100%)");
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void read(const T &now) {
    const double span = static_cast<double>(fHigh) - static_cast<double>(fLow);
    const float fraction = span == 0.0 ? 0.0f : static_cast<float>((static_cast<double>(now) - static_cast<double>(fLow)) / span);
    if (fraction != fFraction) {
      fFraction = fraction;
      markDirty();
    }
  }
  [[nodiscard]] float fraction() const noexcept { return fFraction; }
  [[nodiscard]] T value() const { return valueAt(fFraction, fLow, fHigh); }
  bool onDom(Part, std::string_view event, std::string_view value) {
    if (event != "input" && event != "change") return false;
    int steps = 0;
    std::from_chars(value.data(), value.data() + value.size(), steps);
    fFraction = std::clamp(static_cast<float>(steps) / kSteps, 0.0f, 1.0f);
    return true;
  }
  auto onPress() { return bind::own(model::setTo(valueAt(fFraction, fLow, fHigh))); }

private:
  static constexpr int kSteps = 1000;
  template <std::integral N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(std::lround(static_cast<double>(low) + fraction * (static_cast<double>(high) - low)));
  }
  template <std::floating_point N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(low + fraction * (high - low));
  }
  T fLow;
  T fHigh;
  float fFraction = 0.0f;
  Theme fTheme = theme();
};

// A text input showing a string part -- a std::string, or a type whose one
// member is its text -- and setting it as it is typed into.
template <class T = std::string> class TextField : public Node {
public:
  explicit TextField(std::string placeholder = {}, scene::Spec spec = {}) : fPlaceholder(std::move(placeholder)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-textbox");
    to.attr("type", "text");
    if (!fPlaceholder.empty()) to.attr("placeholder", fPlaceholder);
    to.listen("input");
    to.value(fText);
  }
  void read(const T &now) {
    const std::string &shown = textOf(now);
    if (shown != fText) {
      fText = shown;
      markDirty();
    }
  }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  bool onDom(Part, std::string_view event, std::string_view value) {
    if (event != "input" && event != "change") return false;
    fText = std::string(value);  // the element shows it already
    return true;
  }
  auto onPress() { return bind::own(model::setTo(T{fText})); }

private:
  static const std::string &textOf(const std::string &value) { return value; }
  template <class Wrapped> static const std::string &textOf(const Wrapped &value) { return value.value; }
  std::string fText;
  std::string fPlaceholder;
};

// A row of tabs, one for each value a part can be, showing the one it is and
// setting it to the one clicked.
template <class T> class ChoiceTabs : public Node {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceTabs(std::vector<Choice> choices, scene::Spec spec = {}) : fChoices(std::move(choices)) { fState.apply(spec); }
  // Each choice a tab: Part 1, 2, ... in the choices' order.
  void markup(Markup &to) {
    to.attr("class", "skiff-tabs");
    to.attr("role", "tablist");
    for (std::size_t i = 0; i < fChoices.size(); ++i)
      to.inner("button", Part{static_cast<std::uint32_t>(i + 1)}, [&](Markup &tab) {
        tab.attr("class", "skiff-tab");
        tab.attr("role", "tab");
        tab.attr("type", "button");
        tab.attr("aria-selected", static_cast<int>(i) == fSelected ? "true" : "false");
        tab.text(fChoices[i].first);
        tab.listen("click");
      });
  }
  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    const int selected = found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin());
    if (selected != fSelected) {
      fSelected = selected;
      markDirty();
    }
  }
  [[nodiscard]] int selected() const noexcept { return fSelected; }
  bool onDom(Part where, std::string_view event, std::string_view) {
    if (event != "click" || where.index == 0 || where.index > fChoices.size()) return false;
    fPicked = static_cast<int>(where.index) - 1;
    return true;
  }
  auto onPress() -> std::optional<bind::Own<model::SetTo<T>>> {
    if (fPicked < 0 || static_cast<std::size_t>(fPicked) >= fChoices.size()) return std::nullopt;
    return bind::own(model::setTo(fChoices[static_cast<std::size_t>(fPicked)].second));
  }

private:
  std::vector<Choice> fChoices;
  int fSelected = -1;
  int fPicked = -1;
};

// A select, one option for each value a part can be: the scene's ChoiceList, a dropdown's list.
template <class T> class ChoiceList : public Node {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceList(std::vector<Choice> choices, scene::Spec spec = {}) : fChoices(std::move(choices)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "select"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-select");
    for (std::size_t i = 0; i < fChoices.size(); ++i)
      to.inner("option", Part{static_cast<std::uint32_t>(i + 1)}, [&](Markup &option) {
        option.attr("value", std::to_string(i));
        if (static_cast<int>(i) == fSelected) option.attr("selected", "");
        option.text(fChoices[i].first);
      });
    to.listen("change");
    to.value(fSelected < 0 ? std::string() : std::to_string(fSelected));
  }
  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    const int selected = found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin());
    if (selected != fSelected) {
      fSelected = selected;
      markDirty();
    }
  }
  [[nodiscard]] int selected() const noexcept { return fSelected; }
  bool onDom(Part, std::string_view event, std::string_view value) {
    if (event != "change" && event != "input") return false;
    int picked = -1;
    std::from_chars(value.data(), value.data() + value.size(), picked);
    fPicked = picked;
    return fPicked >= 0 && static_cast<std::size_t>(fPicked) < fChoices.size();
  }
  auto onPress() -> std::optional<bind::Own<model::SetTo<T>>> {
    if (fPicked < 0 || static_cast<std::size_t>(fPicked) >= fChoices.size()) return std::nullopt;
    return bind::own(model::setTo(fChoices[static_cast<std::size_t>(fPicked)].second));
  }

private:
  std::vector<Choice> fChoices;
  int fSelected = -1;
  int fPicked = -1;
};

// A button sending a copy of its event each time it is clicked, up through
// the scopes it is in, as the press is delivered.
template <class E> class SendButton : public Node {
public:
  using Out = model::Types<E>;
  SendButton(std::string label, E event, scene::Spec spec = {}) : fLabel(std::move(label)), fEvent(std::move(event)) { fState.apply(spec); }
  [[nodiscard]] static constexpr std::string_view tag() { return "button"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-button");
    to.attr("type", "button");
    to.listen("click");
    to.text(fLabel);
  }
  void read(E event) { fEvent = std::move(event); }
  void read(std::pair<std::string, E> value) {
    setLabel(std::move(value.first));
    fEvent = std::move(value.second);
  }
  void setLabel(std::string label) {
    if (label != fLabel) {
      fLabel = std::move(label);
      markDirty();
    }
  }
  bool onDom(Part, std::string_view event, std::string_view) { return event == "click"; }
  E onPress() { return fEvent; }

private:
  std::string fLabel;
  E fEvent;
};


// ---- the scene's widgets, on the browser's elements ----------------------------
//
// The same names and the same words as skiff.widgets (setLabel, setOn,
// setText, setFraction...), each a native control; an action given is
// called as the press is delivered (onPress), with what the control is now.

namespace detail {
// An action called, with its answer where it has one (the binding takes it).
template <class Action, class... Args> auto called(Action &action, Args &&...args) {
  if constexpr (std::same_as<std::remove_cvref_t<Action>, scene::NoAction>) {
    return;
  } else if constexpr (std::is_void_v<std::invoke_result_t<Action &, Args...>>) {
    std::invoke(action, std::forward<Args>(args)...);
  } else {
    return std::invoke(action, std::forward<Args>(args)...);
  }
}
} // namespace detail

// A button with a label, calling its action when clicked; primary (the
// accent's), outlined, or plain; enabled or not.
template <class Action = scene::NoAction> class Button : public Node {
public:
  explicit Button(std::string label, Action action = {}, scene::Spec spec = {}) : fLabel(std::move(label)), fAction(std::move(action)) {
    fState.apply(spec);
  }
  Button(Theme theme, std::string label, Action action = {}) : Button(std::move(label), std::move(action)) { fTheme = std::move(theme); }
  [[nodiscard]] static constexpr std::string_view tag() { return "button"; }
  void markup(Markup &to) {
    std::string classes = "skiff-button";
    if (fPrimary) classes += " skiff-primary";
    if (fOutlined) classes += " skiff-outlined";
    to.attr("class", classes);
    to.attr("type", "button");
    to.listen("click");
    to.text(fLabel);
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void setLabel(std::string label) {
    if (label != fLabel) {
      fLabel = std::move(label);
      markDirty();
    }
  }
  [[nodiscard]] const std::string &label() const noexcept { return fLabel; }
  void setPrimary(bool primary) {
    if (primary != fPrimary) {
      fPrimary = primary;
      markDirty();
    }
  }
  [[nodiscard]] bool primary() const noexcept { return fPrimary; }
  void setOutlined(bool outlined) {
    if (outlined != fOutlined) {
      fOutlined = outlined;
      markDirty();
    }
  }
  [[nodiscard]] bool outlined() const noexcept { return fOutlined; }
  void setAccent(scene::Colour accent) {
    fAccent = accent;
    markDirty();
  }
  void setEnabled(bool enabled) { setDisabled(!enabled); }
  [[nodiscard]] bool enabled() const noexcept { return !disabled(); }
  [[nodiscard]] Action &action() noexcept { return fAction; }
  bool onDom(Part, std::string_view event, std::string_view) { return event == "click" && !disabled(); }
  auto onPress() { return detail::called(fAction); }

private:
  std::string fLabel;
  Action fAction;
  Theme fTheme = theme();
  std::optional<scene::Colour> fAccent;
  bool fPrimary = false;
  bool fOutlined = false;
};

// A switch, on or off, calling its action with its new state when flipped.
template <class OnToggle = scene::NoAction> class Toggle : public Node {
public:
  // Made with nothing only where what it calls is made so: a lambda that
  // captures is not, and its default argument, read as libstdc++ asks
  // whether this is (a tuple of them: a column holding it), was an error
  // there rather than an answer.
  explicit Toggle()
    requires(std::default_initializable<OnToggle>)
      : Toggle(OnToggle{}) {}
  explicit Toggle(OnToggle onToggle, scene::Spec spec = {}) : fOnToggle(std::move(onToggle)) { fState.apply(spec); }
  Toggle(Theme theme, OnToggle onToggle) : Toggle(std::move(onToggle)) { fTheme = std::move(theme); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-toggle");
    to.attr("type", "checkbox");
    to.listen("change");
    to.checked(fOn);
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void setOn(bool on) {
    if (on != fOn) {
      fOn = on;
      markDirty();
    }
  }
  void setOnNow(bool on) { setOn(on); }
  [[nodiscard]] bool on() const noexcept { return fOn; }
  bool onDom(Part, std::string_view event, std::string_view) {
    if (event != "change") return false;
    fOn = !fOn;
    return true;
  }
  auto onPress() { return detail::called(fOnToggle, fOn); }

private:
  OnToggle fOnToggle;
  Theme fTheme = theme();
  bool fOn = false;
};

// A line of text typed into, its action called with the text as it changes;
// masked for a password, with a search icon, or plain.
template <class OnChanged = scene::NoAction> class TextBox : public Node {
public:
  // Made with nothing only where what it calls is made so: a lambda that
  // captures is not, and its default argument, read as libstdc++ asks
  // whether this is (a tuple of them: a column holding it), was an error
  // there rather than an answer.
  explicit TextBox()
    requires(std::default_initializable<OnChanged>)
      : TextBox(std::string{}) {}
  explicit TextBox(std::string placeholder, OnChanged onChanged = {}, scene::Spec spec = {})
      : fPlaceholder(std::move(placeholder)), fOnChanged(std::move(onChanged)) {
    fState.apply(spec);
  }
  TextBox(Theme theme, std::string placeholder, OnChanged onChanged = {}) : TextBox(std::move(placeholder), std::move(onChanged)) {
    fTheme = std::move(theme);
  }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void markup(Markup &to) {
    if (!fPlaceholder.empty()) to.attr("placeholder", fPlaceholder);
    to.listen("input");
    to.attr("class", fSearch ? "skiff-textbox skiff-search" : "skiff-textbox");
    to.attr("type", fMasked ? "password" : "text");
    if (fTrailingInset > 0.0f) to.style("padding-right", px(fTrailingInset));
    to.value(fText);
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void setSearchIcon(bool enabled) {
    if (enabled != fSearch) {
      fSearch = enabled;
      markDirty();
    }
  }
  void setMasked(bool masked) {
    if (masked != fMasked) {
      fMasked = masked;
      markDirty();
    }
  }
  [[nodiscard]] bool masked() const noexcept { return fMasked; }
  void setTrailingInset(float inset) {
    fTrailingInset = inset;
    markDirty();
  }
  void setText(std::string text) {
    if (text != fText) {
      fText = std::move(text);
      markDirty();
    }
  }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  bool onDom(Part, std::string_view event, std::string_view value) {
    if (event != "input" && event != "change") return false;
    fText = std::string(value);
    return true;
  }
  auto onPress() { return detail::called(fOnChanged, std::string_view(fText)); }

private:
  std::string fText;
  std::string fPlaceholder;
  OnChanged fOnChanged;
  Theme fTheme = theme();
  float fTrailingInset = 0.0f;
  bool fMasked = false;
  bool fSearch = false;
};

// A text area: lines of plain text typed into (the scene's rich text is
// plain here), its action called with the text as it changes.
template <class OnChanged = scene::NoAction> class TextArea : public Node {
public:
  // Made with nothing only where what it calls is made so: a lambda that
  // captures is not, and its default argument, read as libstdc++ asks
  // whether this is (a tuple of them: a column holding it), was an error
  // there rather than an answer.
  explicit TextArea()
    requires(std::default_initializable<OnChanged>)
      : TextArea(std::string{}) {}
  explicit TextArea(std::string placeholder, OnChanged onChanged = {}, scene::Spec spec = {})
      : fPlaceholder(std::move(placeholder)), fOnChanged(std::move(onChanged)) {
    fState.apply(spec);
  }
  TextArea(Theme theme, std::string placeholder, OnChanged onChanged = {}) : TextArea(std::move(placeholder), std::move(onChanged)) {
    fTheme = std::move(theme);
  }
  [[nodiscard]] static constexpr std::string_view tag() { return "textarea"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-textarea");
    if (!fPlaceholder.empty()) to.attr("placeholder", fPlaceholder);
    to.listen("input");
    to.listen("keydown");
    to.value(fText);
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void setText(std::string text) {
    if (text != fText) {
      fText = std::move(text);
      markDirty();
    }
  }
  [[nodiscard]] std::string_view plainText() const noexcept { return fText; }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  // Typed into: the text as it is. Enter alone (keydown "Enter|00"): a
  // submission, for an area that sends (SubmitArea); Shift+Enter is a line.
  bool onDom(Part, std::string_view event, std::string_view value) {
    if (event == "input" || event == "change") {
      fText = std::string(value);
      fSubmitted = false;
      return true;
    }
    if (event == "keydown" && value.starts_with("Enter|0")) {
      fSubmitted = true;
      return true;
    }
    return false;
  }
  [[nodiscard]] bool submitted() const noexcept { return fSubmitted; }
  auto onPress() { return detail::called(fOnChanged, std::string_view(fText)); }

protected:
  std::string fText;
  bool fSubmitted = false;

private:
  std::string fPlaceholder;
  OnChanged fOnChanged;
  Theme fTheme = theme();
};

// A text area sending an E made of its text each time it is submitted --
// a message from a composer -- and emptied for the next.
template <class E> class SubmitArea : public TextArea<> {
public:
  using Out = model::Types<E>;
  explicit SubmitArea(std::string placeholder = {}, scene::Spec spec = {}) : TextArea<>(std::move(placeholder), {}, spec) {}
  SubmitArea(Theme theme, std::string placeholder) : TextArea<>(std::move(theme), std::move(placeholder)) {}
  bool onDom(Part where, std::string_view event, std::string_view value) {
    TextArea<>::onDom(where, event, value);
    return event == "keydown" && this->submitted();
  }
  E onPress() {
    E sent{std::string(this->plainText())};
    this->setText({});
    this->fSubmitted = false;
    return sent;
  }
};

// A slider's bar: a fraction of its length, dragged, its actions called
// with the fraction as it moves (onSet) and when it is let go (onDone).
template <class OnSet = scene::NoAction, class OnDone = scene::NoAction> class SliderBar : public Node {
public:
  // Made with nothing only where what it calls is made so: a lambda that
  // captures is not, and its default argument, read as libstdc++ asks
  // whether this is (a tuple of them: a column holding it), was an error
  // there rather than an answer.
  explicit SliderBar()
    requires(std::default_initializable<OnSet> && std::default_initializable<OnDone>)
      : SliderBar(OnSet{}) {}
  explicit SliderBar(OnSet onSet, OnDone onDone = {}, scene::Spec spec = {}) : fOnSet(std::move(onSet)), fOnDone(std::move(onDone)) {
    fState.apply(spec);
  }
  SliderBar(Theme theme, OnSet onSet = {}, OnDone onDone = {}) : SliderBar(std::move(onSet), std::move(onDone)) { fTheme = std::move(theme); }
  [[nodiscard]] static constexpr std::string_view tag() { return "input"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-slider");
    to.attr("type", "range");
    to.attr("min", "0");
    to.attr("max", std::to_string(kSteps));
    to.attr("step", "1");
    to.listen("input");
    to.listen("change");
    to.value(std::to_string(static_cast<int>(std::lround(fFraction * kSteps))));
    const std::string at = number(fFraction * 100.0f) + "%";
    to.style("--skiff-track", "linear-gradient(to right," + rgba(fTheme.fAccent) + " 0 " + at + "," + rgba(fTheme.fSurface) + " " + at + " 100%)");
  }
  void setTheme(Theme value) {
    fTheme = std::move(value);
    markDirty();
  }
  void setFraction(float fraction) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    if (fraction != fFraction) {
      fFraction = fraction;
      markDirty();
    }
  }
  [[nodiscard]] float fraction() const noexcept { return fFraction; }
  [[nodiscard]] bool dragging() const noexcept { return false; }
  bool onDom(Part, std::string_view event, std::string_view value) {
    if (event != "input" && event != "change") return false;
    int steps = 0;
    std::from_chars(value.data(), value.data() + value.size(), steps);
    fFraction = std::clamp(static_cast<float>(steps) / kSteps, 0.0f, 1.0f);
    if (event == "change") detail::called(fOnDone, fFraction);
    return true;
  }
  auto onPress() { return detail::called(fOnSet, fFraction); }

private:
  static constexpr int kSteps = 1000;
  OnSet fOnSet;
  OnDone fOnDone;
  Theme fTheme = theme();
  float fFraction = 0.0f;
};

// A pill: a word on a plate of its own colour.
class Pill : public Node {
public:
  struct Look {
    scene::Colour plate = 0;
    scene::Colour text = 0xffffffffu;
    float size = 12.0f;  // the text's
    float height = 20.0f;
    float padX = 8.0f;
    bool bold = false;
  };
  Pill(std::string label, Look look) : fLabel(std::move(label)), fLook(look) {
    fState.apply({.height = look.height,
                  .minWidth = look.height,
                  .padding = {0.0f, look.padX, 0.0f, look.padX},
                  .cornerRadius = look.height * 0.5f,
                  .background = look.plate});
  }
  [[nodiscard]] static constexpr std::string_view tag() { return "span"; }
  void markup(Markup &to) {
    to.attr("class", "skiff-pill");
    to.text(fLabel);
    to.style("color", rgba(fLook.text));
    to.style("font-size", px(fLook.size));
    to.style("font-weight", fLook.bold ? "600" : "400");
  }
  void setLabel(std::string label) {
    if (label != fLabel) {
      fLabel = std::move(label);
      markDirty();
    }
  }
  [[nodiscard]] const std::string &label() const noexcept { return fLabel; }
  void setColours(scene::Colour plate, scene::Colour text) {
    fLook.plate = plate;
    fLook.text = text;
    fState.apply({.background = plate});
    markDirty();
  }

private:
  std::string fLabel;
  Look fLook;
};

// An avatar: a round picture, or -- until there is one -- initials on a
// gradient of two colours.
template <PictureSource Source> class Avatar : public Node {
public:
  struct parts_t {
    Text initials;
    Image<Source> picture;
  } parts;
  Avatar(std::string initials, float size, Source picture, scene::Gradient colours)
      : parts{.initials = Text(std::move(initials), {.place = scene::anchor::kCentre}), .picture = Image<Source>(std::move(picture))}, fColours(colours) {
    fState.apply({.width = size, .height = size, .cornerRadius = size * 0.5f, .gradient = colours, .masking = true});
    parts.picture.apply({.fillX = true, .fillY = true, .cornerRadius = size * 0.5f});
    parts.picture.keepBox();
    fSize = size;
  }
  [[nodiscard]] bool wantsSync() const { return true; }
  // The picture where it has one, else the initials: shown by its parts'
  // own states, written as they are.
  void markup(Markup &to) {
    to.attr("class", "skiff-avatar");
    const bool pictured = parts.picture.picture() != nullptr;
    parts.initials.setVisible(!pictured);
    parts.picture.setVisible(pictured);
    parts.initials.setStyle("font-size", px(fSize * 0.4f));
    parts.initials.setStyle("font-weight", "600");
    parts.initials.setStyle("color", "rgba(255,255,255,1)");
  }
  void show(std::string initials, Source picture, scene::Gradient colours) {
    parts.initials.setText(std::move(initials));
    parts.picture.setSource(std::move(picture));
    fColours = colours;
    fState.apply({.gradient = colours});
    markDirty();
  }

private:
  scene::Gradient fColours;
  float fSize = 0.0f;
};

// A round loader: turning while something is awaited, a ring filled to the
// share done where that is known; pressed, where it has an action (to stop).
template <class OnPress = scene::NoAction> class RadialLoader : public Node {
public:
  // Made with nothing only where what it calls is made so: a lambda that
  // captures is not, and its default argument, read as libstdc++ asks
  // whether this is (a tuple of them: a column holding it), was an error
  // there rather than an answer.
  explicit RadialLoader()
    requires(std::default_initializable<OnPress>)
      : RadialLoader(44.0f) {}
  explicit RadialLoader(float size, OnPress onPress = {}) : fOnPress(std::move(onPress)) {
    fState.apply({.width = size, .height = size, .cornerRadius = size * 0.5f});
  }
  void markup(Markup &to) {
    to.attr("class", "skiff-loader");
    if constexpr (!std::same_as<OnPress, scene::NoAction>) to.listen("click");
    if (fProgress) to.style("--skiff-done", number(std::clamp(*fProgress, 0.0f, 1.0f) * 100.0f) + "%");
    if (fStopped) to.attr("data-stopped", "");
  }
  void setProgress(std::optional<float> done) {
    fProgress = done;
    markDirty();
  }
  void setStopped(bool stopped) {
    if (fStopped != stopped) {
      fStopped = stopped;
      markDirty();
    }
  }
  bool onDom(Part, std::string_view event, std::string_view) { return event == "click"; }
  auto onPress() { return detail::called(fOnPress); }

private:
  OnPress fOnPress;
  std::optional<float> fProgress;
  bool fStopped = false;
};

// A menu of choices under a label: a select, its action called with the
// index chosen. The scene's words: names, current, picked, open (never, here:
// the browser opens its own).
template <class Choose> class ChoiceMenu : public Node {
public:
  Choose choose;
  std::vector<std::string> names;
  std::size_t current = 0, picked = 0;
  bool open = false;
  ChoiceMenu(Theme theme, std::string label, std::vector<std::string> options, std::size_t selected, Choose action)
      : choose(std::move(action)), names(std::move(options)), current(selected), fLabel(std::move(label)), fTheme(std::move(theme)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "label"; }
  // Its caption (Part 1), and a select (Part 2) of the options (Parts 3, ...).
  static constexpr Part kSelect{2};
  void markup(Markup &to) {
    to.attr("class", "skiff-menu");
    to.inner("span", Part{1}, [&](Markup &caption) { caption.text(fLabel); });
    to.inner("select", kSelect, [&](Markup &select) {
      select.attr("class", "skiff-select");
      for (std::size_t i = 0; i < names.size(); ++i)
        select.inner("option", Part{static_cast<std::uint32_t>(i + 3)}, [&](Markup &option) {
          option.attr("value", std::to_string(i));
          if (i == current) option.attr("selected", "");
          option.text(names[i]);
        });
      select.listen("change");
      select.value(current < names.size() ? std::to_string(current) : std::string());
    });
  }
  void read(std::size_t selected) {
    if (selected != current) {
      current = selected;
      markDirty();
    }
  }
  void show_options(bool) {}
  bool onDom(Part where, std::string_view event, std::string_view value) {
    if (where != kSelect || (event != "change" && event != "input")) return false;
    int index = -1;
    std::from_chars(value.data(), value.data() + value.size(), index);
    if (index < 0 || static_cast<std::size_t>(index) >= names.size()) return false;
    picked = static_cast<std::size_t>(index);
    read(picked);
    return true;
  }
  auto onPress() { return detail::called(choose, picked); }

private:
  std::string fLabel;
  Theme fTheme = theme();
};

// A child shown or hidden: open, it is there; closed, it is not (the scene
// slides it; the page shows and hides).
template <class Child> class Collapsible : public Node {
public:
  template <class... Args> explicit Collapsible(Args &&...args) : fChild(std::forward<Args>(args)...) { fState.fVisible = false; }
  [[nodiscard]] Child &child() noexcept { return fChild; }
  [[nodiscard]] const Child &child() const noexcept { return fChild; }
  [[nodiscard]] bool isOpen() const noexcept { return fOpen; }
  void setOpen(bool open) {
    if (open != fOpen) {
      fOpen = open;
      setVisible(open);
      markDirty();
    }
  }
  void setOpenNow(bool open) { setOpen(open); }
  [[nodiscard]] bool settling() const noexcept { return false; }
  void forEachChild(auto &&f) { f(fChild); }
  void markup(Markup &to) { to.attr("class", "skiff-collapsible"); }

private:
  Child fChild;
  bool fOpen = false;
};

// ---- what slides over a base ---------------------------------------------------

namespace dismiss {
struct closes {};
struct pressed {};
} // namespace dismiss

// A navigation drawer: a panel pulled out from the left edge over a base,
// the rest of the base dimmed under a scrim; a click on the scrim pushes it
// back -- or, with dismiss::pressed (or a type of the program's), is a
// press of the drawer's, answered with that type.
template <class Base, class Content, class Dismiss = dismiss::closes> class Drawer : public Node {
public:
  template <class... BaseArgs, class... ContentArgs>
  Drawer(std::piecewise_construct_t, std::tuple<BaseArgs...> base, std::tuple<ContentArgs...> content)
      : fBase(std::make_from_tuple<Base>(std::move(base))), fPanel(vbox(), std::make_from_tuple<Content>(std::move(content))) {
    fState.apply({.fill = true});
    fPanel.apply({.fillY = true, .width = fWidth});
  }
  [[nodiscard]] Base &base() noexcept { return fBase; }
  [[nodiscard]] Content &content() noexcept { return std::get<0>(fPanel.fParts); }
  [[nodiscard]] bool isOpen() const noexcept { return fOpen; }
  void setSheetColour(scene::Colour colour) {
    fSheet = colour;
    markDirty();
  }
  void setWidth(float width) {
    fWidth = width;
    fPanel.apply({.width = width});
    markDirty();
  }
  void open() { setOpen(true); }
  void close() { setOpen(false); }
  void closeNow() { setOpen(false); }
  void setOpen(bool open) {
    fOpen = open;
    markDirty();
  }
  void forEachChild(auto &&f) {
    f(fBase);
    f(fPanel);
  }
  // What covers the base -- the scrim, an element of the drawer's own (Part
  // 1), taking the click so that a press off the panel closes the drawer
  // rather than reaching what is under it -- and the sheet, given its class
  // and its slide.
  static constexpr Part kScrim{1};
  void markup(Markup &to) {
    to.attr("class", "skiff-drawer");
    to.inner("div", kScrim, [&](Markup &scrim) {
      scrim.attr("class", "skiff-scrim");
      scrim.listen("click");
      if (!fOpen) scrim.style("display", "none");
    });
    fPanel.addClass("skiff-sheet");
    fPanel.setStyle("transform", fOpen ? "translateX(0)" : "translateX(-100%)");
    fPanel.setStyle("background-color", rgba(fSheet));
    fPanel.setStyle("z-index", "1");
  }
  // The scrim clicked: the drawer pushed back, or the program told.
  bool onDom(Part where, std::string_view event, std::string_view) {
    if (where != kScrim || event != "click" || !fOpen) return false;
    if constexpr (std::same_as<Dismiss, dismiss::closes>) {
      close();
      return false;
    } else {
      return true;
    }
  }
  auto onPress() {
    if constexpr (std::same_as<Dismiss, dismiss::pressed>) {
      close();
    } else if constexpr (!std::same_as<Dismiss, dismiss::closes>) {
      return Dismiss{};
    }
  }

private:
  Base fBase;
  Stack<Content> fPanel;
  scene::Colour fSheet = 0xff202020u;
  float fWidth = 300.0f;
  bool fOpen = false;
};

// A base, and over it a stack of panels, each over what is under it -- a
// settings page, a detail view, a page opened from a page, in the one
// window. The scene slides them; the page shows them (a transition of the
// sheet's, where the browser gives one).
template <class Base, class Over> class SlideOver : public Node {
public:
  template <class... Args> explicit SlideOver(Args &&...args) : fBase(std::forward<Args>(args)...) { fState.apply({.fill = true}); }
  [[nodiscard]] Base &base() noexcept { return fBase; }
  [[nodiscard]] Over *shown() noexcept { return fRows.empty() ? nullptr : &fRows.back(); }
  void setSheetColour(scene::Colour colour) { fSheet = colour; }
  template <class... Args> Over &open(Args &&...args) {
    Over &made = fRows.emplace_back(std::forward<Args>(args)...);
    made.apply({.place = scene::anchor::kTopLeft, .fillX = true, .fillY = true, .background = fSheet});
    markDirty();
    return made;
  }
  void back() {
    if (!fRows.empty()) fRows.pop_back();
    markDirty();
  }
  void close() {
    fRows.clear();
    markDirty();
  }
  void dropClosed() {}
  [[nodiscard]] std::size_t depth() const noexcept { return fRows.size(); }
  void forEachChild(auto &&f) {
    f(fBase);
    f(fRows);
  }
  void markup(Markup &to) { to.attr("class", "skiff-slideover"); }
  std::vector<Over> fRows;
  std::vector<dom::Handle> fShownRows;  // the rows' elements as last appended: the page keeps it

private:
  Base fBase;
  scene::Colour fSheet = 0xff202020u;
};

// A wallpaper under a chat: a colour or a gradient, a tint over it, blurred
// where the scene blurs a picture (the browser's filter). The scene's
// patterns are its own drawing: here the pattern's colour stands for it.
template <class Out = scene::NoAction> class Wallpaper : public Node {
public:
  // Made with nothing only where what it calls is made so: a lambda that
  // captures is not, and its default argument, read as libstdc++ asks
  // whether this is (a tuple of them: a column holding it), was an error
  // there rather than an answer.
  explicit Wallpaper()
    requires(std::default_initializable<Out>)
      : Wallpaper(Out{}) {}
  explicit Wallpaper(Out out) : fOut(std::move(out)) { fState.apply({.fill = true}); }
  void setOut(Out out) { fOut = std::move(out); }
  void setGradient(std::optional<scene::Gradient> gradient) {
    fGradient = gradient;
    markDirty();
  }
  template <class Pattern> void setPattern(std::shared_ptr<const Pattern>, scene::Colour colour) {
    fColour = colour;
    markDirty();
  }
  void setTint(std::optional<scene::Colour> tint) {
    fTint = tint;
    markDirty();
  }
  void setOpacity(float opacity) {
    fOpacity = opacity;
    markDirty();
  }
  void setBlur(float amount) {
    fBlur = amount;
    markDirty();
  }
  void setBlurs(std::vector<float> amounts) { setBlur(amounts.empty() ? 0.0f : amounts.front()); }
  void markup(Markup &to) {
    to.attr("class", "skiff-wallpaper");
    if (fGradient) to.style("background-image", "linear-gradient(" + rgba(fGradient->top) + "," + rgba(fGradient->bottom) + ")");
    if (fColour) to.style("background-color", rgba(*fColour));
    to.style("opacity", number(fOpacity));
    to.style("filter", fBlur > 0.0f ? "blur(" + px(fBlur) + ")" : std::string());
    to.style("box-shadow", fTint ? "inset 0 0 0 100vmax " + rgba(*fTint) : std::string());
  }

private:
  Out fOut;
  std::optional<scene::Gradient> fGradient;
  std::optional<scene::Colour> fColour;
  std::optional<scene::Colour> fTint;
  float fOpacity = 1.0f;
  float fBlur = 0.0f;
};

} // namespace skiff::widgets::web
