# skiff-widgets

Ready-made controls on top of [skiff](https://github.com/j4niwzis/skiff): the
things a screen is actually built out of, as opposed to the boxes and stacks
it is drawn with.

Every widget is its own module, so a screen compiles the ones it uses:

```cpp
import skiff.widgets.textarea;   // just this one
import skiff.widgets;            // or all of them
```

| module | what it is |
| --- | --- |
| `skiff.widgets.theme` | the palette and metrics every widget draws from |
| `skiff.widgets.textbox` | one line of editable text, with a caret that stops blinking off screen |
| `skiff.widgets.textarea` | editable text of one line or several, as every field of a screen: the caret steps over whole graphemes (UAX #29), IME composition, formats and blocks |
| `skiff.widgets.button` | a labelled rectangle that sends something and lightens under the pointer |
| `skiff.widgets.tabbar` | a row of text tabs, wrapping, with hover tracking and a hook for decorations |
| `skiff.widgets.dropdown` | the closed half of a dropdown: label, current value and open state are data; hover, hit testing and the chevron are the widget's |
| `skiff.widgets.menu` | a menu bar's menus: a title, brighter under the pointer, an accent line under it while open |
| `skiff.widgets.sliderbar` | a track with a filled part and a knob; the value is a fraction, so the bar never sees the units it stands for |
| `skiff.widgets.avatar` | a round picture, or until there is one a gradient with initials (AdwAvatar) |
| `skiff.widgets.pill` | a short label on a rounded plate: a count, a tag, a hint |
| `skiff.widgets.loader` | Telegram Desktop's radial loader: an arc as long as the progress, or turning where it is not known |
| `skiff.widgets.motion` | panels that take every press on them, and `collapsible()`: a child that folds open and shut, its height eased |
| `skiff.widgets.wallpaper` | a chat wallpaper's pattern, drawn from its shapes |
| `skiff.widgets.model` | the widgets bound to a `skiff.model`: a field showing a part and setting it as it is typed into; a button sending an event |
| `skiff.widgets.erased` | what a widget is told to do, held erased outside a release build, so each widget is one class for every action it is given |
| `skiff.widgets.web` | the same controls on a page, as the browser's own elements: a toggle a checkbox, a slider a range input, a choice tabs or a select, dressed by the theme's CSS |
| `skiff.ui` | the one vocabulary a UI is written in: answered by the scene (`skiff.compose` and the widgets) or by the page (`skiff.web` and its widgets), picked at configure time |

Controls use the scene's router rather than screen-owned drag state: sliders
capture the pointer, buttons and tabs activate from the keyboard, and text
fields receive committed UTF-8 and provisional IME composition. Their roles,
labels, values, selection and focus are in the root's accessibility tree.

## Builders

The widgets take part in skiff's builders like any combinator: a widget
function given a builder for a part gives a builder back, its part built
under the rules the view is built under. `collapsible(child)` folds a child
open and shut whether the child is a node or a builder, and a rule finds the
collapsible as itself through what is made over it. `skiff.ui` stacks its
parts in a `compose::Box` of its own.

## One UI, two backends

A screen written against `skiff.ui` is built for either: the words are the
same -- `column`, `row`, `bound<>`, `scoped<>`, `handle<>`, `Text`, `Box`,
`ToggleField`, `SliderField`, `TextField`, `ChoiceTabs`, `ChoiceList`,
`SendButton`, `SKIFF_NAMED` -- and what answers them is one of two source
files of the module. `SKIFF_UI_BACKEND=scene` is `skiff.compose` and the
widgets on Skia; `SKIFF_UI_BACKEND=web` is `skiff.web` and
`skiff.widgets.web`, the browser's own elements with the theme as CSS
(`mountTheme`), each named view a `data-view` in the document. Left unsaid,
it is the scene where Skia is built and the page where it is not.

```cpp
import skiff.ui;
#include <skiff/named.h>
using namespace skiff::ui;

auto settings() {
  return scoped<Root>(
      SKIFF_NAMED(Grid, column(vbox(8.0f),
          row(hbox(10.0f), Text("Columns"), bound<model::Field<&Grid::columns>>(SliderField<int>(1, 128))),
          row(hbox(10.0f), Text("Visualize"), bound<model::Field<&Grid::visualize>>(ToggleField())))));
}
```

## Theming

A widget draws itself from a `Theme` rather than from constants compiled into
it, so a screen restyles by handing one over. `theme()` is the process-wide
default every widget copies at construction; set it once at startup. A
screen that draws two greys where the theme has one keeps two Themes, which
is cheaper than a knob per widget.

The `Theme` is deliberately small. Each widget owns its copy, so overriding
one field for one control does not mean inventing a scheme for the rest.

## Building and testing

`skiff-widgets` is the library project, `test` is a standalone test consumer
and `all` is the aggregate project. skiff itself comes in from its own
repository, pinned to a commit.

```sh
cmake -S all -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

`-G Ninja` is not a preference: CMake only supports C++20 modules under
Ninja. You also need a compiler that can do `import std` -- clang 19 or newer
with libc++, or gcc 15 -- and Skia discoverable through `pkg-config` as
`skia`.
