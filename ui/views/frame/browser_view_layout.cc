// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/overlay_container.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/web_contents_modal_dialog_host.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/point.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview.h"

using views::View;

namespace {

// The visible height of the shadow above the tabs. Clicks in this area are
// treated as clicks to the frame, rather than clicks to the tab.
const int kTabShadowSize = 2;
// The number of pixels the bookmark bar should overlap the spacer by if the
// spacer is visible.
const int kSpacerBookmarkBarOverlap = 1;
// The number of pixels the metro switcher is offset from the right edge.
const int kWindowSwitcherOffsetX = 7;
// The number of pixels the constrained window should overlap the bottom
// of the omnibox.
const int kConstrainedWindowOverlap = 3;

// Combines View::ConvertPointToTarget and View::HitTest for a given |point|.
// Converts |point| from |src| to |dst| and hit tests it against |dst|. The
// converted |point| can then be retrieved and used for additional tests.
bool ConvertedHitTest(views::View* src, views::View* dst, gfx::Point* point) {
  DCHECK(src);
  DCHECK(dst);
  DCHECK(point);
  views::View::ConvertPointToTarget(src, dst, point);
  return dst->HitTestPoint(*point);
}

}  // namespace

class BrowserViewLayout::WebContentsModalDialogHostViews
    : public WebContentsModalDialogHost {
 public:
  explicit WebContentsModalDialogHostViews(
      BrowserViewLayout* browser_view_layout)
          : browser_view_layout_(browser_view_layout) {
  }

  void NotifyPositionRequiresUpdate() {
    FOR_EACH_OBSERVER(WebContentsModalDialogHostObserver,
                      observer_list_,
                      OnPositionRequiresUpdate());
  }

 private:
  virtual gfx::NativeView GetHostView() const OVERRIDE {
    gfx::NativeWindow native_window =
        browser_view_layout_->browser()->window()->GetNativeWindow();
    return views::Widget::GetWidgetForNativeWindow(native_window)->
        GetNativeView();
  }

  // Center horizontally over the content area, with the top overlapping the
  // browser chrome.
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE {
    int top_y = browser_view_layout_->web_contents_modal_dialog_top_y_;
    gfx::Rect content_area =
        browser_view_layout_->browser_view_->GetClientAreaBounds();
    int middle_x = content_area.x() + content_area.width() / 2;
    return gfx::Point(middle_x - size.width() / 2, top_y);
  }

  // Add/remove observer.
  virtual void AddObserver(
      WebContentsModalDialogHostObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }
  virtual void RemoveObserver(
      WebContentsModalDialogHostObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
  }

  BrowserViewLayout* const browser_view_layout_;

  ObserverList<WebContentsModalDialogHostObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogHostViews);
};

// static
const int BrowserViewLayout::kToolbarTabStripVerticalOverlap = 3;

////////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, public:

BrowserViewLayout::BrowserViewLayout()
    : delegate_(NULL),
      browser_(NULL),
      browser_view_(NULL),
      top_container_(NULL),
      tab_strip_(NULL),
      toolbar_(NULL),
      bookmark_bar_(NULL),
      infobar_container_(NULL),
      contents_split_(NULL),
      contents_container_(NULL),
      overlay_container_(NULL),
      window_switcher_button_(NULL),
      download_shelf_(NULL),
      immersive_mode_controller_(NULL),
      dialog_host_(new WebContentsModalDialogHostViews(this)),
      web_contents_modal_dialog_top_y_(-1) {
}

BrowserViewLayout::~BrowserViewLayout() {
}

void BrowserViewLayout::Init(
    BrowserViewLayoutDelegate* delegate,
    Browser* browser,
    BrowserView* browser_view,
    views::View* top_container,
    TabStrip* tab_strip,
    views::View* toolbar,
    InfoBarContainerView* infobar_container,
    views::View* contents_split,
    ContentsContainer* contents_container,
    OverlayContainer* overlay_container,
    ImmersiveModeController* immersive_mode_controller) {
  delegate_.reset(delegate);
  browser_ = browser;
  browser_view_ = browser_view;
  top_container_ = top_container;
  tab_strip_ = tab_strip;
  toolbar_ = toolbar;
  infobar_container_ = infobar_container;
  contents_split_ = contents_split;
  contents_container_ = contents_container;
  overlay_container_ = overlay_container;
  immersive_mode_controller_ = immersive_mode_controller;
}

WebContentsModalDialogHost*
    BrowserViewLayout::GetWebContentsModalDialogHost() {
  return dialog_host_.get();
}

gfx::Size BrowserViewLayout::GetMinimumSize() {
  gfx::Size tabstrip_size(
      browser()->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) ?
      tab_strip_->GetMinimumSize() : gfx::Size());
  BrowserNonClientFrameView::TabStripInsets tab_strip_insets(
      browser_view_->frame()->GetTabStripInsets(false));
  gfx::Size toolbar_size(
      (browser()->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
       browser()->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR)) ?
           toolbar_->GetMinimumSize() : gfx::Size());
  if (tabstrip_size.height() && toolbar_size.height())
    toolbar_size.Enlarge(0, -kToolbarTabStripVerticalOverlap);
  gfx::Size bookmark_bar_size;
  if (bookmark_bar_ &&
      bookmark_bar_->visible() &&
      browser()->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)) {
    bookmark_bar_size = bookmark_bar_->GetMinimumSize();
    bookmark_bar_size.Enlarge(0,
        -(views::NonClientFrameView::kClientEdgeThickness +
            bookmark_bar_->GetToolbarOverlap(true)));
  }
  // TODO: Adjust the minimum height for the find bar.

  gfx::Size contents_size(contents_split_->GetMinimumSize());

  int min_height = tabstrip_size.height() + toolbar_size.height() +
      bookmark_bar_size.height() + contents_size.height();
  int widths[] = {
        tabstrip_size.width() + tab_strip_insets.left + tab_strip_insets.right,
        toolbar_size.width(),
        bookmark_bar_size.width(),
        contents_size.width() };
  int min_width = *std::max_element(&widths[0], &widths[arraysize(widths)]);
  return gfx::Size(min_width, min_height);
}

gfx::Rect BrowserViewLayout::GetFindBarBoundingBox() const {
  // This function returns the area the Find Bar can be laid out within. This
  // basically implies the "user-perceived content area" of the browser
  // window excluding the vertical scrollbar. The "user-perceived content area"
  // excludes the detached bookmark bar (in the New Tab case) and any infobars
  // since they are not _visually_ connected to the Toolbar.

  // First determine the bounding box of the content area in Widget
  // coordinates.
  gfx::Rect bounding_box = contents_container_->ConvertRectToWidget(
      contents_container_->GetLocalBounds());

  gfx::Rect top_container_bounds = top_container_->ConvertRectToWidget(
      top_container_->GetLocalBounds());

  // The find bar is positioned 1 pixel above the bottom of the top container so
  // that it occludes the border between the content area and the top container
  // and looks connected to the top container.
  int find_bar_y = top_container_bounds.bottom() - 1;

  // Grow the height of |bounding_box| by the height of any elements between
  // the top container and |contents_container_| such as the detached bookmark
  // bar and any infobars.
  int height_delta = bounding_box.y() - find_bar_y;
  bounding_box.set_y(find_bar_y);
  bounding_box.set_height(std::max(0, bounding_box.height() + height_delta));

  // Finally decrease the width of the bounding box by the width of
  // the vertical scroll bar.
  int scrollbar_width = gfx::scrollbar_size();
  bounding_box.set_width(std::max(0, bounding_box.width() - scrollbar_width));
  if (base::i18n::IsRTL())
    bounding_box.set_x(bounding_box.x() + scrollbar_width);

  return bounding_box;
}

bool BrowserViewLayout::IsPositionInWindowCaption(
    const gfx::Point& point) {
  // Tab strip may transiently have no parent between the RemoveChildView() and
  // AddChildView() caused by reparenting during an immersive mode reveal.
  // During this window report that the point didn't hit a tab.
  if (!tab_strip_->parent())
    return true;
  gfx::Point tabstrip_point(point);
  views::View::ConvertPointToTarget(browser_view_, tab_strip_, &tabstrip_point);
  return tab_strip_->IsPositionInWindowCaption(tabstrip_point);
}

int BrowserViewLayout::NonClientHitTest(
    const gfx::Point& point) {
  // Since the TabStrip only renders in some parts of the top of the window,
  // the un-obscured area is considered to be part of the non-client caption
  // area of the window. So we need to treat hit-tests in these regions as
  // hit-tests of the titlebar.

  views::View* parent = browser_view_->parent();

  gfx::Point point_in_browser_view_coords(point);
  views::View::ConvertPointToTarget(
      parent, browser_view_, &point_in_browser_view_coords);
  gfx::Point test_point(point);

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip.
  if (browser_view_->IsTabStripVisible()) {
    // See if the mouse pointer is within the bounds of the TabStrip.
    if (ConvertedHitTest(parent, tab_strip_, &test_point)) {
      if (tab_strip_->IsPositionInWindowCaption(test_point))
        return HTCAPTION;
      return HTCLIENT;
    }

    // The top few pixels of the TabStrip are a drop-shadow - as we're pretty
    // starved of dragable area, let's give it to window dragging (this also
    // makes sense visually).
    if (!browser_view_->IsMaximized() &&
        (point_in_browser_view_coords.y() <
            (tab_strip_->y() + kTabShadowSize))) {
      // We return HTNOWHERE as this is a signal to our containing
      // NonClientView that it should figure out what the correct hit-test
      // code is given the mouse position...
      return HTNOWHERE;
    }
  }

  // If the point's y coordinate is below the top of the toolbar and otherwise
  // within the bounds of this view, the point is considered to be within the
  // client area.
  gfx::Rect bv_bounds = browser_view_->bounds();
  bv_bounds.Offset(0, toolbar_->y());
  bv_bounds.set_height(bv_bounds.height() - toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTCLIENT;

  // If the point's y coordinate is above the top of the toolbar, but not in
  // the tabstrip (per previous checking in this function), then we consider it
  // in the window caption (e.g. the area to the right of the tabstrip
  // underneath the window controls). However, note that we DO NOT return
  // HTCAPTION here, because when the window is maximized the window controls
  // will fall into this space (since the BrowserView is sized to entire size
  // of the window at that point), and the HTCAPTION value will cause the
  // window controls not to work. So we return HTNOWHERE so that the caller
  // will hit-test the window controls before finally falling back to
  // HTCAPTION.
  bv_bounds = browser_view_->bounds();
  bv_bounds.set_height(toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTNOWHERE;

  // If the point is somewhere else, delegate to the default implementation.
  return browser_view_->views::ClientView::NonClientHitTest(point);
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, views::LayoutManager implementation:

void BrowserViewLayout::Layout(views::View* browser_view) {
  vertical_layout_rect_ = browser_view->GetLocalBounds();
  int top = LayoutTabStripRegion(browser_view);
  if (delegate_->IsTabStripVisible()) {
    int x = tab_strip_->GetMirroredX() +
        browser_view_->GetMirroredX() +
        browser_view_->frame()->GetThemeBackgroundXInset();
    tab_strip_->SetBackgroundOffset(
        gfx::Point(x, browser_view_->frame()->GetTabStripInsets(false).top));
  }
  top = LayoutToolbar(top);

  web_contents_modal_dialog_top_y_ =
      top + browser_view->y() - kConstrainedWindowOverlap;

  // Overlay container requires updated toolbar bounds to determine its
  // position, and needs to be laid out before:
  // - GetTopMarginForActiveContent(), which calls GetInstantUIState() to check
  //   if overlay container is visible
  // - LayoutInfoBar(): children of infobar container will layout and call
  //   BrowserView::DrawInfoBarArrows(), which checks if overlay container is
  //   visible.
  LayoutOverlayContainer();

  top = LayoutBookmarkAndInfoBars(top);

  // Top container requires updated toolbar and bookmark bar to compute size.
  top_container_->SetSize(top_container_->GetPreferredSize());

  int bottom = LayoutDownloadShelf(browser_view->height());
  // Treat a detached bookmark bar as if the web contents container is shifted
  // upwards and overlaps it.
  top -= GetContentsOffsetForBookmarkBar();
  LayoutContentsSplitView(top, bottom);

  // Instant extended can put suggestions in a web view, which can require an
  // offset to align with the omnibox. This offset must be recomputed after
  // split view layout to account for infobar heights.
  int active_top_margin = GetTopMarginForActiveContent();
  if (contents_container_->SetActiveTopMargin(active_top_margin))
    contents_container_->Layout();

  // This must be done _after_ we lay out the WebContents since this
  // code calls back into us to find the bounding box the find bar
  // must be laid out within, and that code depends on the
  // TabContentsContainer's bounds being up to date.
  if (browser()->HasFindBarController()) {
    browser()->GetFindBarController()->find_bar()->MoveWindowIfNecessary(
        gfx::Rect(), true);
  }

  // Adjust any web contents modal dialogs.
  dialog_host_->NotifyPositionRequiresUpdate();
}

// Return the preferred size which is the size required to give each
// children their respective preferred size.
gfx::Size BrowserViewLayout::GetPreferredSize(views::View* host) {
  return gfx::Size();
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, private:

int BrowserViewLayout::LayoutTabStripRegion(views::View* browser_view) {
  if (!delegate_->IsTabStripVisible()) {
    tab_strip_->SetVisible(false);
    tab_strip_->SetBounds(0, 0, 0, 0);
    return 0;
  }
  // This retrieves the bounds for the tab strip based on whether or not we show
  // anything to the left of it, like the incognito avatar.
  gfx::Rect tabstrip_bounds(delegate_->GetBoundsForTabStrip(tab_strip_));
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  views::View::ConvertPointToTarget(
      browser_view->parent(), browser_view, &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);

  tab_strip_->SetVisible(true);
  tab_strip_->SetBoundsRect(tabstrip_bounds);
  int bottom = tabstrip_bounds.bottom();

  // The metro window switcher sits at the far right edge of the tabstrip
  // a |kWindowSwitcherOffsetX| pixels from the right edge.
  // Only visible if there is more than one type of window to switch between.
  // TODO(mad): update this code when more window types than just incognito
  // and regular are available.
  views::View* switcher_button = window_switcher_button_;
  if (switcher_button) {
    if (browser()->profile()->HasOffTheRecordProfile() &&
        chrome::FindBrowserWithProfile(
            browser()->profile()->GetOriginalProfile(),
            browser()->host_desktop_type()) != NULL) {
      switcher_button->SetVisible(true);
      int width = browser_view->width();
      gfx::Size ps = switcher_button->GetPreferredSize();
      if (width > ps.width()) {
        switcher_button->SetBounds(width - ps.width() - kWindowSwitcherOffsetX,
                                   0,
                                   ps.width(),
                                   ps.height());
      }
    } else {
      // We hide the button if the incognito profile is not alive.
      // Note that Layout() is not called to all browser windows automatically
      // when a profile goes away but we rely in the metro_driver.dll to call
      // ::SetWindowPos( , .. SWP_SHOWWINDOW) which causes this function to
      // be called again. This works both in showing or hidding the button.
      switcher_button->SetVisible(false);
    }
  }

  return bottom;
}

int BrowserViewLayout::LayoutToolbar(int top) {
  int browser_view_width = vertical_layout_rect_.width();
  bool toolbar_visible = delegate_->IsToolbarVisible();
  int y = top;
  y -= (toolbar_visible && delegate_->IsTabStripVisible()) ?
        kToolbarTabStripVerticalOverlap : 0;
  int height = toolbar_visible ? toolbar_->GetPreferredSize().height() : 0;
  toolbar_->SetVisible(toolbar_visible);
  toolbar_->SetBounds(vertical_layout_rect_.x(), y, browser_view_width, height);

  return y + height;
}

int BrowserViewLayout::LayoutBookmarkAndInfoBars(int top) {
  if (bookmark_bar_) {
    // If we're showing the Bookmark bar in detached style, then we
    // need to show any Info bar _above_ the Bookmark bar, since the
    // Bookmark bar is styled to look like it's part of the page.
    if (bookmark_bar_->IsDetached())
      return LayoutBookmarkBar(LayoutInfoBar(top));
    // Otherwise, Bookmark bar first, Info bar second.
    top = std::max(toolbar_->bounds().bottom(), LayoutBookmarkBar(top));
  }
  return LayoutInfoBar(top);
}

int BrowserViewLayout::LayoutBookmarkBar(int top) {
  int y = top;
  if (!delegate_->IsBookmarkBarVisible()) {
    bookmark_bar_->SetVisible(false);
    // TODO(jamescook): Don't change the bookmark bar height when it is
    // invisible, so we can use its height for layout even in that state.
    bookmark_bar_->SetBounds(0, y, browser_view_->width(), 0);
    return y;
  }

  bookmark_bar_->set_infobar_visible(InfobarVisible());
  int bookmark_bar_height = bookmark_bar_->GetPreferredSize().height();
  y -= views::NonClientFrameView::kClientEdgeThickness +
      bookmark_bar_->GetToolbarOverlap(false);
  bookmark_bar_->SetVisible(true);
  bookmark_bar_->SetBounds(vertical_layout_rect_.x(), y,
                                 vertical_layout_rect_.width(),
                                 bookmark_bar_height);
  return y + bookmark_bar_height;
}

int BrowserViewLayout::LayoutInfoBar(int top) {
  // In immersive fullscreen, the infobar always starts near the top of the
  // screen, just under the "light bar" rectangular stripes.
  if (immersive_mode_controller_->IsEnabled()) {
    top = immersive_mode_controller_->ShouldHideTabIndicators()
              ? browser_view_->y()
              : browser_view_->y() + TabStrip::GetImmersiveHeight();
  }
  // Raise the |infobar_container_| by its vertical overlap.
  infobar_container_->SetVisible(InfobarVisible());
  int height;
  int overlapped_top = top - infobar_container_->GetVerticalOverlap(&height);
  infobar_container_->SetBounds(vertical_layout_rect_.x(),
                                overlapped_top,
                                vertical_layout_rect_.width(),
                                height);
  return overlapped_top + height;
}

void BrowserViewLayout::LayoutContentsSplitView(int top, int bottom) {
  // |contents_split_| contains web page contents and devtools.
  // See browser_view.h for details.
  gfx::Rect contents_split_bounds(vertical_layout_rect_.x(),
                                  top,
                                  vertical_layout_rect_.width(),
                                  std::max(0, bottom - top));
  contents_split_->SetBoundsRect(contents_split_bounds);
}

void BrowserViewLayout::LayoutOverlayContainer() {
  bool full_height = overlay_container_->IsOverlayFullHeight();
  int preferred_height = 0;
  if (!full_height)
    preferred_height = overlay_container_->GetPreferredSize().height();
  overlay_container_->SetVisible(full_height || preferred_height > 0);
  if (!overlay_container_->visible())
    return;
  gfx::Point bottom_edge(0, toolbar_->bounds().bottom());
  views::View::ConvertPointToTarget(
      toolbar_->parent(), browser_view_, &bottom_edge);
  // Overlaps with the toolbar like the attached bookmark bar would, so as to
  // completely obscure the attached bookmark bar if it were visible.
  bottom_edge.Offset(0,
                     -(views::NonClientFrameView::kClientEdgeThickness +
                       BookmarkBarView::kToolbarAttachedBookmarkBarOverlap));
  gfx::Rect rect(vertical_layout_rect_);
  rect.Inset(0, bottom_edge.y(), 0, 0);
  if (!full_height && preferred_height < rect.height())
    rect.set_height(preferred_height);
  overlay_container_->SetBoundsRect(rect);
}

int BrowserViewLayout::GetContentsOffsetForBookmarkBar() {
  // If the bookmark bar is hidden or attached to the omnibox the web contents
  // will appear directly underneath it and does not need an offset.
  if (!bookmark_bar_ ||
      !browser_view_->IsBookmarkBarVisible() ||
      !bookmark_bar_->IsDetached()) {
    return 0;
  }

  // Dev tools.
  if (contents_split_->child_at(1) && contents_split_->child_at(1)->visible())
    return 0;

  // Offset for the detached bookmark bar.
  return bookmark_bar_->height() -
      views::NonClientFrameView::kClientEdgeThickness;
}

int BrowserViewLayout::GetTopMarginForActiveContent() {
  // During an immersive reveal, if instant extended is showing suggestions
  // in the main active web view, ensure that active web view appears aligned
  // with the bottom of the omnibox.
  InstantUIState instant_ui_state = GetInstantUIState();
  if (instant_ui_state == kInstantUIFullPageResults &&
      immersive_mode_controller_->IsRevealed())
    return GetTopMarginForImmersiveInstant();

  // Usually we only use a margin if there's a detached bookmarks bar.
  return GetContentsOffsetForBookmarkBar();
}

int BrowserViewLayout::GetTopMarginForImmersiveInstant() {
  // Compute the position of the bottom edge of the top container views,
  // expressed as an offset in the coordinates of |contents_container_|,
  // because the offset will be applied in |contents_container_| layout.
  // NOTE: This requires contents_split_ layout to be complete, as the
  // coordinate system conversion depends on the contents_split_ origin.
  gfx::Point bottom_edge(0, top_container_->height());
  views::View::ConvertPointToTarget(top_container_,
                                    contents_container_,
                                    &bottom_edge);
  return bottom_edge.y();
}

BrowserViewLayout::InstantUIState BrowserViewLayout::GetInstantUIState() {
  if (!browser()->search_model()->mode().is_search())
    return kInstantUINone;

  // If the search suggestions are already being displayed in the overlay
  // contents then return kInstantUIOverlay.
  if (overlay_container_->visible())
    return kInstantUIOverlay;

  // Top bars stay visible until the results page notifies Chrome it is ready.
  if (browser()->search_model()->top_bars_visible())
    return kInstantUINone;

  return kInstantUIFullPageResults;
}

int BrowserViewLayout::LayoutDownloadShelf(int bottom) {
  if (delegate_->DownloadShelfNeedsLayout()) {
    bool visible = browser()->SupportsWindowFeature(
        Browser::FEATURE_DOWNLOADSHELF);
    DCHECK(download_shelf_);
    int height = visible ? download_shelf_->GetPreferredSize().height() : 0;
    download_shelf_->SetVisible(visible);
    download_shelf_->SetBounds(vertical_layout_rect_.x(), bottom - height,
                               vertical_layout_rect_.width(), height);
    download_shelf_->Layout();
    bottom -= height;
  }
  return bottom;
}

bool BrowserViewLayout::InfobarVisible() const {
  // Cast to a views::View to access GetPreferredSize().
  views::View* infobar_container = infobar_container_;
  // NOTE: Can't check if the size IsEmpty() since it's always 0-width.
  return browser_->SupportsWindowFeature(Browser::FEATURE_INFOBAR) &&
      (infobar_container->GetPreferredSize().height() != 0);
}
