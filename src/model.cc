export module skiff.widgets.model;

import std;
import skiff.scene;
import skiff.model;
import skiff.bind;
import skiff.nodes;
import skiff.compose;
export import skiff.widgets.button;
export import skiff.widgets.textbox;
export import skiff.widgets.sliderbar;
export import skiff.widgets.tabbar;
export import skiff.widgets.textarea;
export import skiff.widgets.dropdown;

// skiff's widgets, bound to a skiff.model: a text field showing a part and
// setting it as it is typed into; a button sending an event when pressed.
//
// Each answers what it is done to with a change of the part it shows
// (bind::Own): the press said, and the change made where the part is as
// it is answered -- nothing kept for a drain.

export namespace skiff::widgets {

// What a field was typed into: its part set to the text.
template <class T> struct Typed {
  using Answer = skiff::bind::Own<skiff::model::SetTo<T>>;
  Answer operator()(std::string_view text) const { return skiff::bind::own(skiff::model::setTo(T{std::string(text)})); }
};

// A text box showing a part of the model -- a Named<..., std::string>, or
// any type whose one member is its text -- and setting it as it is typed
// into. Wrapped in bind::Bound by compose::bound<T>(TextField<T>(...)).
template <class T> class TextField : public internal::TextBox<Typed<T>> {
public:
  explicit TextField(std::string placeholder = {})
      : internal::TextBox<Typed<T>>(std::move(placeholder), Typed<T>{}) {}
  TextField(Theme theme, std::string placeholder, bool masked = false)
      : internal::TextBox<Typed<T>>(std::move(theme), std::move(placeholder), Typed<T>{}) {
    this->setMasked(masked);
  }

  void read(const T &now) {
    const std::string &shown = textOf(now);
    if (shown != this->text())
      this->setText(shown);
  }

private:
  static const std::string &textOf(const std::string &value) { return value; }
  template <class Wrapped> static const std::string &textOf(const Wrapped &value) { return value.value; }
};

// What a model widget's own action is: a press said, the widget's own
// onPress() making the change -- from what it shows.
struct Answers {
  using Answer = void;
  void operator()(auto &&...) const {}
};

// A toggle showing a bool part -- a plain field, Field<&Settings::sound> --
// or an optional one (unsaid is off), and setting it to the other when
// pressed: twice, and it is as it was.
template <class T = bool> class ToggleField : public internal::Toggle<Answers> {
public:
  ToggleField() : internal::Toggle<Answers>(Answers{}) {}
  explicit ToggleField(Theme theme) : internal::Toggle<Answers>(std::move(theme), Answers{}) {}

  void read(const T &now) {
    fNow = onOf(now);
    // The state it opens in shown there and then; a change after, slid to.
    if (std::exchange(fShown, true))
      this->setOn(fNow);
    else
      this->setOnNow(fNow);
  }
  // Pressed: set to the other.
  auto onPress() { return skiff::bind::own(skiff::model::setTo(T(!fNow))); }

private:
  static bool onOf(bool now) { return now; }
  static bool onOf(const std::optional<bool> &now) { return now.value_or(false); }
  bool fNow = false;
  bool fShown = false;
};
ToggleField() -> ToggleField<bool>;
explicit ToggleField(Theme) -> ToggleField<bool>;

// A slider showing a number part between two bounds, and setting it as it
// is dragged: a whole number rounded to the nearest.
template <class T> class SliderField : public internal::SliderBar<Answers> {
public:
  SliderField(T low, T high) : internal::SliderBar<Answers>(Answers{}), fLow(low), fHigh(high) {}
  SliderField(Theme theme, T low, T high)
      : internal::SliderBar<Answers>(std::move(theme), Answers{}), fLow(low), fHigh(high) {}

  void read(const T &now) {
    const double span = static_cast<double>(fHigh) - static_cast<double>(fLow);
    this->setFraction(span == 0.0 ? 0.0f
                                  : static_cast<float>((static_cast<double>(now) - static_cast<double>(fLow)) / span));
  }
  // Dragged: set to the value it is at.
  auto onPress() { return skiff::bind::own(skiff::model::setTo(valueAt(this->fraction(), fLow, fHigh))); }

private:
  template <std::integral N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(std::lround(static_cast<double>(low) + fraction * (static_cast<double>(high) - low)));
  }
  template <std::floating_point N> static N valueAt(float fraction, N low, N high) {
    return static_cast<N>(low + fraction * (high - low));
  }
  T fLow;
  T fHigh;
};

// A slider selecting one of a discrete list, in order.
template <class T>
class ChoiceSliderField
    : public internal::SliderBar<skiff::scene::NoAction, Answers> {
public:
  ChoiceSliderField(Theme theme, std::vector<T> choices)
      : internal::SliderBar<skiff::scene::NoAction, Answers>(
            std::move(theme), skiff::scene::NoAction{}, Answers{}),
        fChoices(std::move(choices)) {}
  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now);
    const auto last = fChoices.size() > 1 ? fChoices.size() - 1 : 1;
    this->setFraction(found == fChoices.end()
                          ? 0.0f
                          : static_cast<float>(found - fChoices.begin()) /
                                static_cast<float>(last));
  }
  auto onPress() -> std::optional<skiff::bind::Own<skiff::model::SetTo<T>>> {
    if (fChoices.empty())
      return std::nullopt;
    const auto index = static_cast<std::size_t>(
        std::lround(std::clamp(this->fraction(), 0.0f, 1.0f) *
                    static_cast<float>(fChoices.size() - 1)));
    return skiff::bind::own(skiff::model::setTo(fChoices[index]));
  }

private:
  std::vector<T> fChoices;
};

// A radio row showing one choice of a model part. Input and semantics
// agree with the value read; the screen never updates its mark by hand.
template <class T> struct ChoiceRowField : skiff::compose::Stacked {
  Theme fTheme;
  T fChoice;
  struct Parts {
    skiff::nodes::Text label;
    skiff::nodes::Icon mark;
  } parts;
  ChoiceRowField(Theme theme, std::string label, T choice)
      : Stacked(skiff::compose::hbox(16.0f,
                                     {.fillX = true,
                                      .height = 46.0f,
                                      .padding = {0.0f, 20.0f, 0.0f, 20.0f},
                                      .hoverBackground = theme.fSurfaceHover,
                                      .focusBackground = theme.fSurfaceHover})),
        fTheme(std::move(theme)), fChoice(std::move(choice)),
        parts{.label = skiff::compose::styled(
                  {.grow = skiff::scene::axes::kX,
                   .alignSelf = skiff::scene::align::kMiddle},
                  skiff::nodes::Text(std::move(label), 15.0f, fTheme.fText)),
              .mark = skiff::compose::styled(
                  {.width = 20.0f,
                   .height = 20.0f,
                   .alignSelf = skiff::scene::align::kMiddle},
                  skiff::nodes::Icon(shape(false), fTheme.fTextDim))} {}
  static skiff::nodes::IconShape shape(bool chosen) {
    skiff::nodes::IconShape result{
        {{skiff::nodes::mark::circle{0.0f, 0.0f, 8.0f}, 2.0f}}};
    if (chosen)
      result.marks.push_back(
          {skiff::nodes::mark::circle{0.0f, 0.0f, 4.0f}, 0.0f, true});
    return result;
  }
  void read(const T &now) {
    const bool chosen = now == fChoice;
    this->apply({.selected = chosen});
    parts.mark.setShape(shape(chosen));
    parts.mark.setColour(chosen ? fTheme.fAccent : fTheme.fTextDim);
  }
  auto onPress() const {
    return skiff::bind::own(skiff::model::setTo(fChoice));
  }
  bool acceptsInput() const { return true; }
  bool hoverChangesAppearance() const { return true; }
  bool focusChangesAppearance() const { return true; }
  skiff::scene::Semantics semantics() const {
    skiff::scene::Semantics result;
    result.fRole = skiff::scene::semantic_role::button{};
    result.fLabel = parts.label.text();
    result.fSelected = this->fState.selected();
    result.fActions = {skiff::scene::semantic_action::focus{},
                       skiff::scene::semantic_action::activate{}};
    return result;
  }
};

// A row of tabs, one for each value a part can be -- a variant's
// alternatives, an enumeration's names -- showing the one it is and setting
// it to the one picked.
template <class T> class ChoiceTabs : public internal::TabBar<Answers> {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceTabs(std::vector<Choice> choices)
      : internal::TabBar<Answers>(Answers{}), fChoices(std::move(choices)) {
    std::vector<typename internal::TabBar<Answers>::Tab> tabs;
    for (std::size_t i = 0; i < fChoices.size(); ++i)
      tabs.push_back({fChoices[i].first, static_cast<int>(i)});
    this->setTabs(std::move(tabs));
  }

  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    this->setSelected(found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin()));
  }
  // A tab picked: set to its value.
  auto onPress() -> std::optional<skiff::bind::Own<skiff::model::SetTo<T>>> {
    const int index = this->picked();
    if (index < 0 || static_cast<std::size_t>(index) >= fChoices.size())
      return std::nullopt;
    return skiff::bind::own(skiff::model::setTo(fChoices[static_cast<std::size_t>(index)].second));
  }

private:
  std::vector<Choice> fChoices;
};

// A dropdown's rows, one for each value a part can be, showing the one it
// is and setting it to the one chosen.
template <class T> class ChoiceList : public internal::DropdownList<Answers> {
public:
  using Choice = std::pair<std::string, T>;
  explicit ChoiceList(std::vector<Choice> choices)
      : internal::DropdownList<Answers>(Answers{}), fChoices(std::move(choices)) {
    this->setOptions(std::ranges::to<std::vector>(std::views::transform(fChoices, &Choice::first)));
  }

  void read(const T &now) {
    const auto found = std::ranges::find(fChoices, now, &Choice::second);
    this->setCurrent(found == fChoices.end() ? -1 : static_cast<int>(found - fChoices.begin()));
  }
  // A row chosen: set to its value.
  auto onPress() -> std::optional<skiff::bind::Own<skiff::model::SetTo<T>>> {
    const int index = this->chosen();
    if (index < 0 || static_cast<std::size_t>(index) >= fChoices.size())
      return std::nullopt;
    return skiff::bind::own(skiff::model::setTo(fChoices[static_cast<std::size_t>(index)].second));
  }

private:
  std::vector<Choice> fChoices;
};

// A text area sending an E made of its text each time it is submitted --
// a message from a composer -- and emptied for the next.
template <class E> class SubmitArea : public internal::TextArea<Answers> {
public:
  explicit SubmitArea(std::string placeholder = {})
      : internal::TextArea<Answers>(std::move(placeholder), Answers{}) {}
  SubmitArea(Theme theme, std::string placeholder)
      : internal::TextArea<Answers>(std::move(theme), std::move(placeholder), Answers{}) {}
  // Sent: what is written, and the area emptied for the next.
  E onPress() {
    E sent{std::string(this->plainText())};
    this->setText({});
    return sent;
  }
};

// The events a button sent, kept until they are taken.
// A button's press answered with a copy of its event: nothing kept.
template <class E> struct Sent {
  using Answer = E;
  E fEvent;
  E operator()() const { return fEvent; }
};

// A button sending a copy of its event each time it is pressed, up through
// the scopes it is in: as the press is delivered (skiff::bind::press).
template <class E> class SendButton : public internal::Button<Sent<E>> {
public:
  using Out = skiff::model::Types<E>;
  SendButton(std::string label, E event)
      : internal::Button<Sent<E>>(std::move(label), Sent<E>{std::move(event)}) {}
  SendButton(Theme theme, std::string label, E event)
      : internal::Button<Sent<E>>(std::move(theme), std::move(label), Sent<E>{std::move(event)}) {}
  // A bound action follows model edits without replacing the focused button.
  void read(E event) { this->action().fEvent = std::move(event); }
  void read(std::pair<std::string, E> value) {
    this->setLabel(std::move(value.first));
    this->read(std::move(value.second));
  }
};

} // namespace skiff::widgets
