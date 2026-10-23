# skiff-widgets

Ready-made controls on top of [skiff](../skiff): the things a screen is
actually built out of, as opposed to the boxes and flows it is drawn with.

Every widget is its own module, so a screen compiles the ones it uses:

```cpp
import skiff.widgets.textbox;   // just this one
import skiff.widgets;           // or all of them
```

| module | what it is |
| --- | --- |
| `skiff.widgets.theme` | the palette and metrics every widget draws from |
| `skiff.widgets.textbox` | one line of editable text, with a caret that stops blinking off screen |
| `skiff.widgets.button` | a labelled rectangle that calls something and lightens under the pointer |
| `skiff.widgets.tabbar` | a row of text tabs, wrapping, with hover tracking and a hook for decorations |
| `skiff.widgets.web` | the same controls on the page, as the browser's own elements: a toggle a checkbox, a slider a range input, a choice tabs or a select, dressed by the theme's CSS |
| `skiff.ui` | the one vocabulary a UI is written in: answered by the scene (`skiff.compose` and the widgets) or by the page (`skiff.web` and its widgets), picked at configure time |

Controls use the scene router rather than screen-owned drag state: sliders
capture the pointer, buttons and tabs activate from the keyboard, and text
boxes receive committed UTF-8 and provisional IME composition. Their semantic
roles, labels, values, selection and focus are available through the root's
accessibility tree.

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

`demo/mazes` is that: a maze visualizer's settings, one `page.h`, a page
under Emscripten (`SKIFF_WIDGETS_DEMO=ON` with `emcmake`, `index.html`) and a
scene elsewhere.

## Theming

A widget draws itself from a `Theme` rather than from constants compiled into
it, so a screen restyles by handing one over:

```cpp
auto *box = column->add<widgets::TextBox>({.fillX = true}, "search...");
box->setTheme(kMyTheme);
box->setSearchIcon(true);
```

`theme()` is the process-wide default every widget copies at construction; set
it once at startup. A screen that draws two greys where the theme has one
keeps two Themes, which is cheaper than a knob per widget.

The `Theme` is deliberately small. Each widget owns its copy, so overriding
one field for one control does not mean inventing a scheme for the rest.

## Building and testing

`skiff-widgets` is the library project, `test` is a standalone test consumer
and `all` is the aggregate project. skiff itself comes in from its own
repository through CPM, pinned to a commit.

```sh
cmake -S all -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

`-G Ninja` is not a preference. CMake only supports C++20 modules under
Ninja; the Makefile generator does not scan for them, and the build fails
without saying why.

You also need a compiler that can do `import std` -- clang 19 or newer with
libc++, or gcc 15 -- and Skia discoverable through `pkg-config` as `skia`.
