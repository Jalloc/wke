
#include <WebCore/FileIconLoader.h>
#include <WebCore/Icon.h>

namespace wke
{
    class EmptyPopupMenu : public WebCore::PopupMenu {
    public:
        virtual void show(const WebCore::IntRect&, WebCore::FrameView*, int) {}
        virtual void hide() {}
        virtual void updateFromElement() {}
        virtual void disconnectClient() {}
    };

    class EmptySearchPopupMenu : public WebCore::SearchPopupMenu {
    public:
        virtual WebCore::PopupMenu* popupMenu() { return m_popup.get(); }
        virtual void saveRecentSearches(const AtomicString&, const Vector<String>&) {}
        virtual void loadRecentSearches(const AtomicString&, Vector<String>&) {}
        virtual bool enabled() { return false; }

    private:
        RefPtr<EmptyPopupMenu> m_popup;
    };

    class ToolTip
    {
    public:
        ToolTip(IWebView* webView)
            :webView_(webView)
            ,pixels_(NULL)
        {}

        void set(const String& title, const WebCore::IntPoint& point)
        {
            title_ = title;
            point_ = point;
            pixels_ = NULL;
        }

        const String& title() const
        {
            return title_;
        }

        void paint(void* dst, int pitch)
        {
            if (!pixels_)
            {
                if (title_.isEmpty())
                    return;

                if (!hdc_)
                {
                    hdc_ = adoptPtr(::CreateCompatibleDC(0));
                    ::SelectObject(hdc_.get(), GetStockObject(DEFAULT_GUI_FONT));
                    ::SetTextColor(hdc_.get(), RGB(0x64, 0x64, 0x64));
                }

                int width = webView_->width();
                int height = webView_->height();

                const int marginH = 4;
                const int marginV = 4;
                if (marginH * 2 >= width || marginV * 2 >= height)
                    return;

                RECT rcText;
                rcText.left = marginH;
                rcText.right = rcText.left + width / 2;
                rcText.top = marginV;
                rcText.bottom = rcText.top + height / 2;
                ::DrawText(hdc_.get(), title_.characters(), title_.length(), &rcText, DT_WORDBREAK|DT_CALCRECT);

                if (rcText.right > width - marginH)
                    rcText.right = width - marginH;

                if (rcText.bottom > height - marginV)
                    rcText.bottom = height - marginV;

                rect_.left = 0;
                rect_.top = 0;
                rect_.right = rcText.right + marginH;
                rect_.bottom = rcText.bottom + marginV;

                WebCore::BitmapInfo bmp = WebCore::BitmapInfo::createBottomUp(WebCore::IntSize(rect_.right, rect_.bottom));
                HBITMAP hbmp = ::CreateDIBSection(0, &bmp, DIB_RGB_COLORS, &pixels_, NULL, 0);
                SelectObject(hdc_.get(), hbmp);
                hbmp_ = adoptPtr(hbmp);

                unsigned int* ptr = (unsigned int*)pixels_;

                //background
                memset(ptr, 0xFA, rect_.right * rect_.bottom * 4);
                
                //top
                memset(ptr, 0x64, rect_.right * 4);
                
                //bottom
                ptr = (unsigned int*)pixels_ + rect_.right * (rect_.bottom - 1);
                memset(ptr, 0x64, rect_.right * 4);

                //left and right
                ptr = (unsigned int*)pixels_ + rect_.right;
                for (int i = 1; i < rect_.bottom; ++i)
                {
                    ptr[0] = 0x64646464;
                    ptr[-1] = 0x64646464;
                    ptr += rect_.right;
                }

                //corner
                ptr = (unsigned int*)pixels_;
                ptr[rect_.right + 1] = 0x64646464; 
                ptr[rect_.right * 2 - 2] = 0x64646464;
                ptr[rect_.right * (rect_.bottom - 1) - 2] = 0x64646464;
                ptr[rect_.right * (rect_.bottom - 2) + 1] = 0x64646464;

                ::DrawText(hdc_.get(), title_.characters(), title_.length(), &rcText, DT_WORDBREAK);

                for (int i = 0; i < rect_.right * rect_.bottom; ++i)
                {
                    ((unsigned char*)pixels_)[i*4 + 3] = 255;
                }

                const int offset = 10;
                if (point_.x() + offset + rect_.right < width)
                    rect_.left = point_.x() + offset;
                else if (point_.x() > rect_.right + offset)
                    rect_.left = point_.x() - rect_.right - offset;
                else
                    rect_.left = width - rect_.right;

                if (point_.y() + offset + rect_.bottom < height)
                    rect_.top = point_.y() + offset;
                else if (point_.y() > rect_.top + offset)
                    rect_.top = point_.y() - rect_.bottom - offset;
                else
                    rect_.top = height - rect_.bottom;

                rect_.right += rect_.left;
                rect_.bottom += rect_.top;
            }

            //copy to dst
            int w = rect_.right - rect_.left;
            int h = rect_.bottom - rect_.top;
            unsigned char* pixels = (unsigned char*)pixels_; 
            unsigned char* dst_pixels = (unsigned char*)dst + rect_.top * pitch + rect_.left*4;

            *(int*)pixels = *(int*)dst_pixels;
            *((int*)pixels + w - 1) = *((int*)dst_pixels + w - 1);

            *((int*)pixels + w * (h - 1)) = *(int*)(dst_pixels + pitch * (h - 1));
            *((int*)pixels + w * h - 1) = *((int*)(dst_pixels + pitch * h - 4) - 1);

            for (int i = 0; i < h; ++i)
            {
                memcpy(dst_pixels, pixels, w*4);
                pixels += w*4;
                dst_pixels += pitch;
            }
        }

    private:
        String title_;
        WebCore::IntPoint point_;

        OwnPtr<HDC> hdc_;
        OwnPtr<HBITMAP> hbmp_;
        void* pixels_;
        RECT rect_;

        IWebView* webView_;
    };

    class ChromeClient : public WebCore::ChromeClient
    {
    public:
        ChromeClient(IWebView* webView)
            :webView_(webView)
            ,toolTip_(webView)
        {}

        virtual void chromeDestroyed() override
        {
            dbgMsg(L"frameLoaderDestroyed\n");
            delete this;
        }
        
        virtual void setWindowRect(const WebCore::FloatRect& rect) override
        {
            rect_ = rect;
        }

        virtual WebCore::FloatRect windowRect() override
        {
            return rect_;
        }

        virtual WebCore::FloatRect pageRect() override
        {
            return rect_;
        }
        
        virtual void focus() override
        {
        }

        virtual void unfocus() override
        {
        }

        virtual bool canTakeFocus(WebCore::FocusDirection) override
        {
            return true;
        }

        virtual void takeFocus(WebCore::FocusDirection) override
        {
        }

        virtual void focusedNodeChanged(WebCore::Node*) override
        {
        }

        void focusedFrameChanged(WebCore::Frame*) override
        {
        }

        virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) override
        {
            dbgMsg(L"createWindow\n");
            return NULL;
        }

        virtual void show() override
        {
        }

        virtual bool canRunModal() override
        {
            return true;
        }

        virtual void runModal() override
        {
        }

        virtual void setToolbarsVisible(bool) override
        {
        }

        virtual bool toolbarsVisible() override
        {
            return false;
        }
        
        virtual void setStatusbarVisible(bool) override
        {
        }

        virtual bool statusbarVisible() override
        {
            return false;
        }
        
        virtual void setScrollbarsVisible(bool) override
        {
        }

        virtual bool scrollbarsVisible() override
        {
            return false;
        }
        
        virtual void setMenubarVisible(bool) override
        {
        }

        virtual bool menubarVisible() override
        {
            return false;
        }

        virtual void setResizable(bool) override
        {
        }
        
        virtual void addMessageToConsole(WebCore::MessageSource, WebCore::MessageType, WebCore::MessageLevel, const WTF::String& message, unsigned int lineNumber, const WTF::String& url) override
        {
            outputMsg(L"console message %s %d %s\n", CSTR(message), lineNumber, CSTR(url));
        }

        virtual bool canRunBeforeUnloadConfirmPanel() override
        {
            return true;
        }

        virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame* frame) override
        {
            return true;
        }

        virtual void closeWindowSoon() override
        {
        }
        
        virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String& msg) override
        {
            outputMsg(L"JavaScript Alert %s\n", CSTR(msg));
        }

        virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String& msg) override
        {
            outputMsg(L"JavaScript Confirm %s\n", CSTR(msg));
            return true;
        }

        virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& msg, const WTF::String& defaultValue, WTF::String& result) override
        {
            outputMsg(L"JavaScript Prompt %s %s\n", CSTR(msg), CSTR(defaultValue));
            return true;
        }

        virtual void setStatusbarText(const WTF::String& text) override
        {
            dbgMsg(L"setStatusbarText %s\n", CSTR(text));
        }

        virtual bool shouldInterruptJavaScript() override
        {
            return false;
        }

        WebCore::KeyboardUIMode keyboardUIMode() override
        {
            return WebCore::KeyboardAccessDefault;
        }

        virtual WebCore::IntRect windowResizerRect() const override
        {
            return WebCore::IntRect();
        }

        virtual void invalidateWindow(const WebCore::IntRect& rect, bool immediate) override
        {
            webView_->addDirtyArea(rect.x(), rect.y(), rect.width(), rect.height());
            //dbgMsg(L"invalidateWindow\n");
        }

        virtual void invalidateContentsAndWindow(const WebCore::IntRect& rect, bool immediate) override
        {
            webView_->addDirtyArea(rect.x(), rect.y(), rect.width(), rect.height());
            //dbgMsg(L"invalidateContentsAndWindow\n");
        }

        virtual void invalidateContentsForSlowScroll(const WebCore::IntRect& rect, bool immediate) override
        {
            webView_->addDirtyArea(rect.x(), rect.y(), rect.width(), rect.height());
            //dbgMsg(L"invalidateContentsForSlowScroll\n");
        }

        virtual void scroll(const WebCore::IntSize&, const WebCore::IntRect&, const WebCore::IntRect&) override
        {
        }

        virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint& pt) const override
        {
            return pt;
        }

        virtual WebCore::IntRect windowToScreen(const WebCore::IntRect& pt) const override
        {
            return pt;
        }

        virtual PlatformPageClient platformPageClient() const override
        {
            //dbgMsg(L"platformPageClient\n");
            return GetDesktopWindow();
        }

        virtual void scrollbarsModeDidChange() const override
        {
        }

        virtual void setCursor(const WebCore::Cursor& cursor) override
        {
            HCURSOR platformCursor = cursor.platformCursor()->nativeCursor();
            if (!platformCursor)
                return;

            ::SetCursor(platformCursor);
        }

        virtual void setCursorHiddenUntilMouseMoves(bool) override
        {
        }

        virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override
        {
        }

        virtual void mouseDidMoveOverElement(const WebCore::HitTestResult& result, unsigned modifierFlags) override
        {
            WebCore::TextDirection dir;
            String title = result.title(dir);
            if (title != toolTip_.title())
            {
                WebCore::IntPoint point = ((CWebView*)webView())->mainFrame()->eventHandler()->currentMousePosition();
                toolTip_.set(title, point);
                webView_->setDirty(true);
            }
        }

        virtual void setToolTip(const WTF::String& toolTip, WebCore::TextDirection) override
        {
        }

        virtual void print(WebCore::Frame*) override
        {
        }

#if ENABLE(SQL_DATABASE)
        virtual void exceededDatabaseQuota(WebCore::Frame*, const WTF::String& databaseName) override
        {
        }
#endif

        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) override
        {
        }

        virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t totalSpaceNeeded) override
        {
        }

#if ENABLE(CONTEXT_MENUS)
        virtual void showContextMenu() override { }
#endif

#if ENABLE(NOTIFICATIONS)
        virtual WebCore::NotificationPresenter* notificationPresenter() override
        {
            return 0;
        }
#endif
        virtual void requestGeolocationPermissionForFrame(WebCore::Frame*, WebCore::Geolocation*) override
        {
        }

        virtual void cancelGeolocationPermissionRequestForFrame(WebCore::Frame*, WebCore::Geolocation*) override
        {
        }

        virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override
        {
        }

        virtual void loadIconForFiles(const Vector<WTF::String>& filenames, WebCore::FileIconLoader* loader) override
        {
            loader->notifyFinished(WebCore::Icon::createIconForFiles(filenames));
        }

        virtual void formStateDidChange(const WebCore::Node*) override
        {
        }

#if USE(ACCELERATED_COMPOSITING)
        virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override
        {
        }

        virtual void setNeedsOneShotDrawingSynchronization() override
        {
        }

        virtual void scheduleCompositingLayerSync() override
        {
        }
#endif


#if PLATFORM(WIN)
        virtual void setLastSetCursorToCurrentCursor() override
        {
        }
#endif

        virtual bool selectItemWritingDirectionIsNatural() override
        {
            return true;
        }

        bool selectItemAlignmentFollowsMenuWritingDirection() override
        {
            return false;
        }

        virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient* client) const override
        {
            return adoptRef(new EmptyPopupMenu);
        }

        virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient* client) const override
        {
            return adoptRef(new EmptySearchPopupMenu);
        }

        virtual bool shouldRubberBandInDirection(WebCore::ScrollDirection) const override 
        { 
            return true; 
        }

        virtual void numWheelEventHandlersChanged(unsigned) override 
        { 
        }

        virtual void* webView() const override 
        { 
            return webView_; 
        }

        void paintPopupMenu(void* dst, int pitch)
        {
        }

        void paintToolTip(void* dst, int pitch)
        {
            toolTip_.paint(dst, pitch);
        }

    protected:
        WebCore::FloatRect rect_;
        IWebView* webView_;
        ToolTip toolTip_;
    };
}
