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

Controls use the scene router rather than screen-owned drag state: sliders
capture the pointer, buttons and tabs activate from the keyboard, and text
boxes receive committed UTF-8 and provisional IME composition. Their semantic
roles, labels, values, selection and focus are available through the root's
accessibility tree.

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
