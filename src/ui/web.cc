// skiff.ui on the page: the one vocabulary a program writes its UI in,
// answered here by skiff.web and skiff.widgets.web, laid out by the browser.
// The same module name is answered by src/ui/scene.cc for a scene on Skia;
// the build picks one (SKIFF_UI_BACKEND), and the program is written once.
export module skiff.ui;

export import skiff.core;
export import skiff.model;
export import skiff.bind;
export import skiff.web;
export import skiff.widgets.web;

import std;

export namespace skiff::ui {

using skiff::web::Look;
using skiff::web::vbox;
using skiff::web::hbox;
using skiff::web::column;
using skiff::web::row;
using skiff::web::bound;
using skiff::web::scoped;
using skiff::web::local;
using skiff::web::handle;
using skiff::web::handlers;
using skiff::web::Named;
using skiff::web::Text;
using skiff::web::Box;
using skiff::web::Page;
using skiff::web::fire;

using skiff::widgets::Theme;
using skiff::widgets::theme;
using skiff::widgets::web::ToggleField;
using skiff::widgets::web::SliderField;
using skiff::widgets::web::TextField;
using skiff::widgets::web::ChoiceTabs;
using skiff::widgets::web::ChoiceList;
using skiff::widgets::web::SendButton;
using skiff::widgets::web::themeCss;
using skiff::widgets::web::mountTheme;

} // namespace skiff::ui
