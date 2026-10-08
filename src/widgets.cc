export module skiff.widgets;

// The whole widget set, for a screen that wants all of it. Each widget is its
// own module and can be imported on its own -- a screen that needs a text box
// and nothing else says `import skiff.widgets.textbox;` and does not compile
// the rest.
//
// A widget draws itself from a Theme rather than from constants baked into
// it, so a screen restyles by handing over a different Theme and not by
// subclassing. The default Theme is a dark neutral one; a client hands its
// own to each widget it makes, as it makes it.

export import skiff.widgets.theme;
export import skiff.widgets.erased;
export import skiff.widgets.textbox;
export import skiff.widgets.textarea;
export import skiff.widgets.button;
export import skiff.widgets.tabbar;
export import skiff.widgets.dropdown;
export import skiff.widgets.sliderbar;
export import skiff.widgets.motion;
export import skiff.widgets.menu;
export import skiff.widgets.avatar;
export import skiff.widgets.pill;
export import skiff.widgets.loader;
export import skiff.widgets.wallpaper;
export import skiff.widgets.model;
