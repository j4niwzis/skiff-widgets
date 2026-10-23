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
using skiff::web::Canvas;
using skiff::web::Figure;
using skiff::web::figure;
using skiff::web::Layer;
using skiff::web::layer;
using skiff::web::Polygon;
using skiff::web::Polyline;
using skiff::web::Line;
using skiff::web::Circle;
using skiff::web::each;
using skiff::web::group;
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
using skiff::web::Flow;
using skiff::web::ScrollContainer;
using skiff::web::Image;
using skiff::web::Icon;
using skiff::web::Stack;
using skiff::web::pageCss;
using skiff::widgets::web::Button;
using skiff::widgets::web::Toggle;
using skiff::widgets::web::TextBox;
using skiff::widgets::web::TextArea;
using skiff::widgets::web::SubmitArea;
using skiff::widgets::web::SliderBar;
using skiff::widgets::web::Pill;
using skiff::widgets::web::Avatar;
using skiff::widgets::web::RadialLoader;
using skiff::widgets::web::ChoiceMenu;
using skiff::widgets::web::Collapsible;
using skiff::widgets::web::Drawer;
using skiff::widgets::web::SlideOver;
using skiff::widgets::web::Wallpaper;
namespace dismiss = skiff::widgets::web::dismiss;
using skiff::web::styled;
using skiff::web::projected;
using skiff::web::spec_for;
using skiff::web::text_for;
using skiff::web::derived;
using skiff::web::erased;
using skiff::web::flicked;

} // namespace skiff::ui
