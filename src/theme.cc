export module skiff.widgets.theme;

import std;
import skiff.core;

namespace skiff::widgets {
using skiff::scene::Anchor;
using skiff::scene::Axes;
using skiff::scene::Easing;
using skiff::scene::Margin;
using skiff::scene::Spec;
} // namespace skiff::widgets

export namespace skiff::widgets {

struct Theme {
  skiff::scene::Colour fSurface = 0xff2e3538u;
  skiff::scene::Colour fSurfaceHover = 0xff394246u;
  skiff::scene::Colour fSurfaceActive = 0xff454f54u;
  skiff::scene::Colour fText = 0xffffffffu;
  // A caption beside a control, as distinct from the control's own text.
  skiff::scene::Colour fLabel = 0xffdbe9f0u;
  // Secondary -- icons, unselected tabs -- and fainter still, placeholders.
  skiff::scene::Colour fTextDim = 0xffb2bec4u;
  skiff::scene::Colour fTextFaint = 0xff8f9ca3u;
  skiff::scene::Colour fAccent = 0xff66ccffu;
  skiff::scene::Colour fOnAccent = 0xff171a1cu;

  float fCorner = 5.0f;
  float fFontSize = 16.0f;
  float fRowHeight = 40.0f;
  float fPaddingX = 12.0f;

  friend bool operator==(const Theme &, const Theme &) = default;
};


// The process-wide default a widget copies where it is given none: set once
// at startup, by the program.
[[nodiscard]] inline Theme &theme() {
  static Theme kept;
  return kept;
}

} // namespace skiff::widgets
