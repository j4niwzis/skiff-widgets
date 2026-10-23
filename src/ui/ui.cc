// skiff.ui: the one vocabulary a program writes its UI in, for both
// backends at once. A tree said in it is a description -- what each node
// is, its spec, what it reads and sends -- and nothing of either backend;
// mounted (mount), it is made into the page's nodes (skiff.web: the
// browser's elements, laid out by the browser) or the scene's (skiff.compose
// and the widgets: laid out by skiff, drawn through its painter). One
// source, compiled once, one program holding both if it likes: a page that
// is also drawn on a canvas, one model under both (ModelOf).
//
// What the model is bound to is skiff.bind's, written once for both
// backends: bound, state, each, handlers are said here and made the
// backend's as the rest is. A node of a program's own says how it is made
// on each, by a member on(OnPage) and on(OnScene).
export module skiff.ui;

export import skiff.core;
export import skiff.model;
export import skiff.bind;
export import skiff.widgets.theme;
#ifdef SKIFF_UI_ON_SCENE
export import skia;
export import skiff.paint;
export import skiff.scene;
export import skiff.nodes;
export import skiff.compose;
export import skiff.widgets;
#endif
#ifdef SKIFF_UI_ON_PAGE
export import skiff.web;
export import skiff.widgets.web;
#endif

import std;

export namespace skiff::ui {

namespace scene = skiff::scene;
namespace model = skiff::model;

// ---- where a tree is made -----------------------------------------------

// The page: skiff.web's nodes, the document's elements.
struct OnPage {};
// The scene: skiff.compose's nodes and skiff-widgets', on the scene.
struct OnScene {};
inline constexpr OnPage onPage{};
inline constexpr OnScene onScene{};
template <class Tag>
concept Backend = std::same_as<Tag, OnPage> || std::same_as<Tag, OnScene>;
#ifdef SKIFF_UI_ON_PAGE
inline constexpr bool kOnPage = true;
#else
inline constexpr bool kOnPage = false;
#endif
#ifdef SKIFF_UI_ON_SCENE
inline constexpr bool kOnScene = true;
#else
inline constexpr bool kOnScene = false;
#endif

// A view made into the backend's nodes: by its own on(tag).
template <class Tag, class D>
  requires requires(D d, Tag tag) { std::move(d).on(tag); }
[[nodiscard]] auto mount(Tag tag, D view) {
  return std::move(view).on(tag);
}
// What a view is made into there.
template <class Tag, class D> using Mounted = decltype(mount(std::declval<Tag>(), std::declval<D>()));

// ---- what answers an event: handlers --------------------------------------
//
// Written where they are used: handle<E>(...) -- given the event and the
// place, the place, the event, or nothing -- several put together,
// handlers(...). The same for both backends: a scope asks them, whatever
// its nodes are.
template <class E, class F> struct Handle {
  F fAnswer;
  template <class P>
    requires std::invocable<const F &, const E &, const P &>
  auto on(const E &event, const P &here) const {
    return fAnswer(event, here);
  }
  template <class P>
    requires(!std::invocable<const F &, const E &, const P &> && std::invocable<const F &, const P &>)
  auto on(const E &, const P &here) const {
    return fAnswer(here);
  }
  template <class P>
    requires(!std::invocable<const F &, const E &, const P &> && !std::invocable<const F &, const P &> &&
             std::invocable<const F &, const E &>)
  auto on(const E &event, const P &) const {
    return fAnswer(event);
  }
  template <class P>
    requires(!std::invocable<const F &, const E &, const P &> && !std::invocable<const F &, const P &> &&
             !std::invocable<const F &, const E &> && std::invocable<const F &>)
  auto on(const E &, const P &) const {
    return fAnswer();
  }
};
template <class E, class F> [[nodiscard]] Handle<E, F> handle(F answer) { return {std::move(answer)}; }
template <class... H> struct Handlers : H... {
  using H::on...;
};
template <class... H> [[nodiscard]] Handlers<H...> handlers(H... each) { return {std::move(each)...}; }

// ---- stacks ---------------------------------------------------------------

// How a stack lays its parts out: across or down, the gap between them, its
// own spec.
struct Look {
  bool fHorizontal = false;
  float fGap = 0.0f;
  scene::Spec fSpec{};
};
[[nodiscard]] inline Look vbox(float gap = 0.0f, scene::Spec spec = {}) { return {false, gap, std::move(spec)}; }
[[nodiscard]] inline Look hbox(float gap = 0.0f, scene::Spec spec = {}) { return {true, gap, std::move(spec)}; }

namespace detail {
template <class Tag> auto lookOn(Tag, const Look &look) {
  if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
    return look.fHorizontal ? skiff::web::hbox(look.fGap, look.fSpec) : skiff::web::vbox(look.fGap, look.fSpec);
#endif
  } else {
#ifdef SKIFF_UI_ON_SCENE
    return look.fHorizontal ? skiff::compose::hbox(look.fGap, look.fSpec) : skiff::compose::vbox(look.fGap, look.fSpec);
#endif
  }
}
// What a node is given after it is made: its specs, in order.
struct Applied {
  std::vector<scene::Spec> fSpecs;
  void apply(const scene::Spec &spec) { fSpecs.push_back(spec); }
  template <class N> void onto(N &node) const {
    for (const scene::Spec &spec : fSpecs)
      node.apply(spec);
  }
};
} // namespace detail

// Parts one after another, down (column) or across (row).
template <bool Across, class... P> struct Stack : detail::Applied {
  Look fLook;
  std::tuple<P...> fParts;
  Stack(Look look, P... parts) : fLook(std::move(look)), fParts(std::move(parts)...) { fLook.fHorizontal = Across; }
  template <Backend Tag> auto on(Tag tag) && {
    auto made = std::apply(
        [&](P &...each) {
          if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
            if constexpr (Across)
              return skiff::web::row(detail::lookOn(tag, fLook), mount(tag, std::move(each))...);
            else
              return skiff::web::column(detail::lookOn(tag, fLook), mount(tag, std::move(each))...);
#endif
          } else {
#ifdef SKIFF_UI_ON_SCENE
            if constexpr (Across)
              return skiff::compose::row(detail::lookOn(tag, fLook), mount(tag, std::move(each))...);
            else
              return skiff::compose::column(detail::lookOn(tag, fLook), mount(tag, std::move(each))...);
#endif
          }
        },
        fParts);
    this->onto(made);
    return made;
  }
};
template <class... P> [[nodiscard]] Stack<false, P...> column(Look look, P... parts) {
  return Stack<false, P...>(std::move(look), std::move(parts)...);
}
template <class... P>
  requires(!std::same_as<std::remove_cvref_t<P>, Look> && ...)
[[nodiscard]] Stack<false, P...> column(P... parts) {
  return Stack<false, P...>(vbox(), std::move(parts)...);
}
template <class... P> [[nodiscard]] Stack<true, P...> row(Look look, P... parts) {
  return Stack<true, P...>(std::move(look), std::move(parts)...);
}
template <class... P>
  requires(!std::same_as<std::remove_cvref_t<P>, Look> && ...)
[[nodiscard]] Stack<true, P...> row(P... parts) {
  return Stack<true, P...>(hbox(), std::move(parts)...);
}

// Parts in lines, as many to a line as there is room for, wrapped where
// it says so.
template <class... P> struct Flow : detail::Applied {
  scene::FlowOptions fOptions;
  std::tuple<P...> fParts;
  explicit Flow(scene::FlowOptions options, P... parts) : fOptions(options), fParts(std::move(parts)...) {}
  template <Backend Tag> auto on(Tag tag) && {
    auto made = std::apply(
        [&](P &...each) {
          if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
            return skiff::web::Flow(fOptions, mount(tag, std::move(each))...);
#endif
          } else {
#ifdef SKIFF_UI_ON_SCENE
            return skiff::nodes::Flow(fOptions, mount(tag, std::move(each))...);
#endif
          }
        },
        fParts);
    this->onto(made);
    return made;
  }
};

// ---- leaves ---------------------------------------------------------------

// Words: a size, a colour, a weight -- or the theme's, said by a spec alone.
class Text : public detail::Applied {
public:
  explicit Text(std::string text, scene::Spec spec = {}) : fText(std::move(text)) { this->apply(spec); }
  Text(std::string text, float size, scene::Colour colour, bool bold = false)
      : fText(std::move(text)), fSize(size), fColour(colour), fBold(bold) {}
  void setWrapped(bool wrapped) { fWrapped = wrapped; }
  void setElided(bool elided) { fElided = elided; }
  [[nodiscard]] const std::string &text() const noexcept { return fText; }
  template <Backend Tag> auto on(Tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      skiff::web::Text made = fSize ? skiff::web::Text(fText, *fSize, fColour, fBold) : skiff::web::Text(fText);
      return finish(made);
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      skiff::nodes::Text made = fSize ? skiff::nodes::Text(fText, *fSize, fColour, fBold)
                                      : skiff::nodes::Text(fText, skiff::widgets::theme().fFontSize, skiff::widgets::theme().fLabel);
      return finish(made);
#endif
    }
  }

private:
  template <class N> N finish(N &made) {
    this->onto(made);
    if (fWrapped)
      made.setWrapped(*fWrapped);
    if constexpr (requires { made.setElided(true); })
      if (fElided)
        made.setElided(*fElided);
    return std::move(made);
  }
  std::string fText;
  std::optional<float> fSize;
  scene::Colour fColour = 0;
  bool fBold = false;
  std::optional<bool> fWrapped, fElided;
};

// A box of a colour, its parts in it.
template <class... P> struct Box : detail::Applied {
  scene::Colour fColour;
  std::tuple<P...> fParts;
  explicit Box(scene::Colour colour, P... parts) : fColour(colour), fParts(std::move(parts)...) {}
  template <Backend Tag> auto on(Tag tag) && {
    auto made = std::apply(
        [&](P &...each) {
          if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
            return skiff::web::Box(fColour, mount(tag, std::move(each))...);
#endif
          } else {
#ifdef SKIFF_UI_ON_SCENE
            return skiff::nodes::Box<Mounted<Tag, P>...>(fColour, mount(tag, std::move(each))...);
#endif
          }
        },
        fParts);
    this->onto(made);
    return made;
  }
};

// A link followed: what the program does with it -- on the page the
// browser follows the <a>; on the scene the press sends this up, for the
// program's sink to take.
struct OpenLink {
  std::string href;
};
#ifdef SKIFF_UI_ON_PAGE
namespace detail {
// On the page, a link is an <a>: its words, and where it goes.
struct PageLink : skiff::web::Text {
  PageLink(std::string label, std::string href, float size, scene::Colour colour, bool bold)
      : skiff::web::Text(std::move(label), size, colour, bold), fHref(std::move(href)) {}
  [[nodiscard]] static constexpr std::string_view tag() { return "a"; }
  void mounted() {
    skiff::web::Text::mounted();
    skiff::web::dom::attr(fState.fHandle, "class", "skiff-text skiff-link");
    skiff::web::dom::attr(fState.fHandle, "href", fHref);
  }
  std::string fHref;
};
} // namespace detail
#endif
// Words that go somewhere when clicked.
struct Link : detail::Applied {
  std::string fLabel, fHref;
  float fSize = 16.0f;
  scene::Colour fColour = 0;
  bool fBold = false;
  Link(std::string label, std::string href, float size, scene::Colour colour, bool bold = false)
      : fLabel(std::move(label)), fHref(std::move(href)), fSize(size), fColour(colour), fBold(bold) {}
  template <Backend Tag> auto on(Tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      detail::PageLink made(std::move(fLabel), std::move(fHref), fSize, fColour, fBold);
      this->onto(made);
      return made;
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      skiff::nodes::Text words(std::move(fLabel), fSize, fColour, fBold);
      this->onto(words);
      return skiff::compose::onClick(OpenLink{std::move(fHref)}, std::move(words), std::string("link"));
#endif
    }
  }
};

// A view said differently on each backend: the first where it is mounted on
// the page, the second where it is mounted on the scene -- what a tree says
// of itself, its words, where they differ.
template <class P, class S> struct Either {
  P fOnPage;
  S fOnScene;
  template <Backend Tag> auto on(Tag tag) && {
    if constexpr (std::same_as<Tag, OnPage>)
      return mount(tag, std::move(fOnPage));
    else
      return mount(tag, std::move(fOnScene));
  }
};
template <class P, class S> [[nodiscard]] Either<P, S> either(P onPage, S onScene) { return {std::move(onPage), std::move(onScene)}; }

// ---- the model's kinds, around a view ------------------------------------

// What wraps one view: made the backend's wrapper, of the view made there.
#define SKIFF_UI_WRAP(Name, page_call, scene_call)                                                                     \
  template <Backend Tag> auto on(Tag tag) && {                                                                         \
    if constexpr (std::same_as<Tag, OnPage>) {                                                                         \
      SKIFF_UI_IF_PAGE(return page_call;)                                                                              \
    } else {                                                                                                           \
      SKIFF_UI_IF_SCENE(return scene_call;)                                                                            \
    }                                                                                                                  \
  }
#ifdef SKIFF_UI_ON_PAGE
#define SKIFF_UI_IF_PAGE(...) __VA_ARGS__
#else
#define SKIFF_UI_IF_PAGE(...)
#endif
#ifdef SKIFF_UI_ON_SCENE
#define SKIFF_UI_IF_SCENE(...) __VA_ARGS__
#else
#define SKIFF_UI_IF_SCENE(...)
#endif

// Reading a part of the model.
template <class Want, class D> struct Bound {
  D fView;
  SKIFF_UI_WRAP(Bound, skiff::web::bound<Want>(mount(tag, std::move(fView))),
                skiff::compose::bound<Want>(mount(tag, std::move(fView))))
};
template <class Want, class D> [[nodiscard]] Bound<Want, D> bound(D view) { return {std::move(view)}; }

// The part a view keeps, where it is made (skiff.bind's state): the model
// is the parts the tree says (ModelOf).
template <class T, class H, class D> struct State {
  H fHandlers;
  D fView;
  SKIFF_UI_WRAP(State, skiff::web::state<T>(std::move(fHandlers), mount(tag, std::move(fView))),
                skiff::compose::state<T>(std::move(fHandlers), mount(tag, std::move(fView))))
};
template <class T, class H, class D> [[nodiscard]] State<T, H, D> state(H handlers, D view) {
  return {std::move(handlers), std::move(view)};
}
template <class T, class D> [[nodiscard]] State<T, Handlers<>, D> state(D view) { return {Handlers<>{}, std::move(view)}; }

// A scope at a part, its handlers taking what is sent within.
template <class Within, class H, class D> struct Scoped {
  H fHandlers;
  D fView;
  SKIFF_UI_WRAP(Scoped, skiff::web::scoped<Within>(std::move(fHandlers), mount(tag, std::move(fView))),
                skiff::compose::scoped<Within>(std::move(fHandlers), mount(tag, std::move(fView))))
};
template <class Within, class H, class D> [[nodiscard]] Scoped<Within, H, D> scoped(H handlers, D view) {
  return {std::move(handlers), std::move(view)};
}

// A state of the view's own, not the model's.
template <class T, class H, class D> struct Local {
  H fHandlers;
  D fView;
  T fInitial;
  SKIFF_UI_WRAP(Local, skiff::web::local<T>(std::move(fHandlers), mount(tag, std::move(fView)), std::move(fInitial)),
                skiff::compose::local<T>(std::move(fHandlers), mount(tag, std::move(fView)), std::move(fInitial)))
};
template <class T, class H, class D> [[nodiscard]] Local<T, H, D> local(H handlers, D view, T initial = {}) {
  return {std::move(handlers), std::move(view), std::move(initial)};
}

// A view with a spec of its own.
template <class D> struct Styled {
  scene::Spec fSpec;
  D fView;
  SKIFF_UI_WRAP(Styled, skiff::web::styled(fSpec, mount(tag, std::move(fView))),
                skiff::compose::styled(fSpec, mount(tag, std::move(fView))))
};
template <class D> [[nodiscard]] Styled<D> styled(scene::Spec spec, D view) { return {std::move(spec), std::move(view)}; }

// A view read through a function of the part.
template <class Want, class F, class D> struct Projected {
  F fCompute;
  D fView;
  SKIFF_UI_WRAP(Projected, skiff::web::projected<Want>(std::move(fCompute), mount(tag, std::move(fView))),
                skiff::compose::projected<Want>(std::move(fCompute), mount(tag, std::move(fView))))
};
template <class Want, class F, class D> [[nodiscard]] Projected<Want, F, D> projected(F compute, D view) {
  return {std::move(compute), std::move(view)};
}
// A view whose spec is read from the part.
template <class Want, class F, class D> struct SpecFor {
  F fCompute;
  D fView;
  SKIFF_UI_WRAP(SpecFor, skiff::web::spec_for<Want>(std::move(fCompute), mount(tag, std::move(fView))),
                skiff::compose::spec_for<Want>(std::move(fCompute), mount(tag, std::move(fView))))
};
template <class Want, class F, class D> [[nodiscard]] SpecFor<Want, F, D> spec_for(F compute, D view) {
  return {std::move(compute), std::move(view)};
}
// Words read from the part.
template <class Want, class F> struct TextFor {
  F fCompute;
  Text fText;
  SKIFF_UI_WRAP(TextFor, skiff::web::text_for<Want>(std::move(fCompute), mount(tag, std::move(fText))),
                skiff::compose::text_for<Want>(std::move(fCompute), mount(tag, std::move(fText))))
};
template <class Want, class F> [[nodiscard]] TextFor<Want, F> text_for(F compute, Text text) {
  return {std::move(compute), std::move(text)};
}

namespace detail {
// A row of a list, made there.
template <class Tag, class F> struct RowOn {
  F fMake;
  template <class T> auto operator()(const T &value) const { return mount(Tag{}, fMake(value)); }
};
} // namespace detail
// A row for each element of a keyed list, made and gone with them.
template <class Key, class T, class F> struct Each {
  F fMake;
  Look fLook;
  SKIFF_UI_WRAP(Each, (skiff::web::each<Key, T>(detail::RowOn<Tag, F>{std::move(fMake)}, detail::lookOn(tag, fLook))),
                (skiff::compose::each<Key, T>(detail::RowOn<Tag, F>{std::move(fMake)}, detail::lookOn(tag, fLook))))
};
template <class Key, class T, class F> [[nodiscard]] Each<Key, T, F> each(F make, Look look = vbox()) {
  return {std::move(make), std::move(look)};
}

// ---- controls, bound to the model ------------------------------------------

// A button that sends a copy of its event when it is pressed; bound, it
// reads the event it sends.
template <class E> struct SendButton : detail::Applied {
  std::string fLabel;
  E fEvent;
  SendButton(std::string label, E event, scene::Spec spec = {}) : fLabel(std::move(label)), fEvent(std::move(event)) {
    this->apply(spec);
  }
  template <Backend Tag> auto on(Tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      skiff::widgets::web::SendButton<E> made(std::move(fLabel), std::move(fEvent));
      this->onto(made);
      return made;
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      skiff::widgets::SendButton<E> made(std::move(fLabel), std::move(fEvent));
      this->onto(made);
      return made;
#endif
    }
  }
};
// A field of words, of a T read from and written to the part it is bound to.
template <class T = std::string> struct TextField : detail::Applied {
  std::string fPlaceholder;
  explicit TextField(std::string placeholder = {}, scene::Spec spec = {}) : fPlaceholder(std::move(placeholder)) {
    this->apply(spec);
  }
  template <Backend Tag> auto on(Tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      skiff::widgets::web::TextField<T> made(std::move(fPlaceholder));
      this->onto(made);
      return made;
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      skiff::widgets::TextField<T> made(std::move(fPlaceholder));
      this->onto(made);
      return made;
#endif
    }
  }
};
// A switch, of a bool read from and written to its part.
template <class T = bool> struct ToggleField : detail::Applied {
  explicit ToggleField(scene::Spec spec = {}) { this->apply(spec); }
  template <Backend Tag> auto on(Tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      skiff::widgets::web::ToggleField<T> made;
      this->onto(made);
      return made;
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      skiff::widgets::ToggleField<T> made;
      this->onto(made);
      return made;
#endif
    }
  }
};
// A row of tabs, one for each value its part can be.
template <class T> struct ChoiceTabs : detail::Applied {
  using Choice = std::pair<std::string, T>;
  std::vector<Choice> fChoices;
  explicit ChoiceTabs(std::vector<Choice> choices, scene::Spec spec = {}) : fChoices(std::move(choices)) { this->apply(spec); }
  template <Backend Tag> auto on(Tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      skiff::widgets::web::ChoiceTabs<T> made(std::move(fChoices));
      this->onto(made);
      return made;
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      skiff::widgets::ChoiceTabs<T> made(std::move(fChoices));
      this->onto(made);
      return made;
#endif
    }
  }
};

// ---- a view named where it is written (SKIFF_NAMED) ------------------------

namespace detail {
#ifdef SKIFF_UI_ON_PAGE
template <class Self, class N> struct NamedOnPage : skiff::web::Named<NamedOnPage<Self, N>, N> {
  using Base = skiff::web::Named<NamedOnPage<Self, N>, N>;
  using Base::Base;
  using Base::onKey;
  using Base::onPointer;
  using Base::onText;
  using Base::onSemantic;
  static constexpr const char *viewName() { return Self::viewName(); }
};
#endif
#ifdef SKIFF_UI_ON_SCENE
template <class Self, class N> struct NamedOnScene : skiff::compose::Named<NamedOnScene<Self, N>, N> {
  using Base = skiff::compose::Named<NamedOnScene<Self, N>, N>;
  using Base::Base;
  using Base::onKey;
  using Base::onPointer;
  using Base::onText;
  using Base::onSemantic;
  static constexpr const char *viewName() { return Self::viewName(); }
};
#endif
} // namespace detail
// What SKIFF_NAMED makes a view over: the view, named Self, made the
// backend's named view where it is mounted. (The hooks the macro declares
// again are named here, never called.)
template <class Self, class D> struct Named {
  D fView;
  explicit Named(D view) : fView(std::move(view)) {}
  void onKey();
  void onPointer();
  void onText();
  void onSemantic();
  template <Backend Tag> auto on(Tag tag) && {
    if constexpr (std::same_as<Tag, OnPage>) {
#ifdef SKIFF_UI_ON_PAGE
      using Made = Mounted<Tag, D>;
      return detail::NamedOnPage<Self, Made>(mount(tag, std::move(fView)));
#endif
    } else {
#ifdef SKIFF_UI_ON_SCENE
      using Made = Mounted<Tag, D>;
      return detail::NamedOnScene<Self, Made>(mount(tag, std::move(fView)));
#endif
    }
  }
};

// ---- the tree, mounted ----------------------------------------------------

// The model a tree is bound to: the parts its views say they keep, the same
// on either backend.
template <class D>
using RootOf = skiff::bind::RootOf<Mounted<std::conditional_t<kOnPage, OnPage, OnScene>, D>>;
template <class D, class Reactions = skiff::bind::NoReactions> using ModelOf = skiff::model::Model<RootOf<D>, Reactions>;

// The theme both backends dress their controls in.
using skiff::widgets::Theme;
using skiff::widgets::theme;
#ifdef SKIFF_UI_ON_PAGE
using skiff::widgets::web::mountTheme;
using skiff::widgets::web::themeCss;
#endif

} // namespace skiff::ui
