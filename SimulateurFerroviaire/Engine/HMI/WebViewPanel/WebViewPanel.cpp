#include "WebViewPanel.h"

#include <stdexcept>

// =============================================================================
// Construction
// =============================================================================

WebViewPanel::WebViewPanel(Logger& logger)
    : m_logger(logger)
{
}

WebViewPanel::~WebViewPanel()
{
    close();
}


// =============================================================================
// API publique
// =============================================================================

void WebViewPanel::create(HWND parentHwnd)
{
    if (!parentHwnd)
    {
        LOG_ERROR(m_logger, "WebViewPanel::create - HWND invalide");
        return;
    }

    m_parentHwnd = parentHwnd;

    LOG_INFO(m_logger, "Initialisation WebView2...");
    initializeWebView();
}

void WebViewPanel::resize()
{
    if (!m_controller || !m_parentHwnd)
    {
        return;
    }

    RECT bounds;
    GetClientRect(m_parentHwnd, &bounds);

    m_controller->put_Bounds(bounds);
}

void WebViewPanel::navigate(const std::wstring& url)
{
    if (!m_webview)
    {
        LOG_ERROR(m_logger, "navigate() appelé avant initialisation");
        return;
    }

    m_webview->Navigate(url.c_str());
}

void WebViewPanel::navigateToString(const std::wstring& htmlContent)
{
    if (!m_webview)
    {
        LOG_ERROR(m_logger, "navigateToString() appelé avant initialisation");
        return;
    }

    m_webview->NavigateToString(htmlContent.c_str());
}

void WebViewPanel::executeScript(const std::wstring& script)
{
    if (!m_webview)
    {
        LOG_ERROR(m_logger, "executeScript appelé avant initialisation");
        return;
    }

    m_webview->ExecuteScript(
        script.c_str(),
        Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this, script](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT
            {
                if (FAILED(errorCode))
                {
                    LOG_ERROR(m_logger, "Échec ExecuteScript() : " + errorCode);
                    LOG_INFO(m_logger, "Script exécuté : " + std::string(script.begin(), script.end()));
                    return errorCode;
                }
                LOG_DEBUG(m_logger, "Script exécuté : " + std::string(script.begin(), script.end()));
                return S_OK;
            }).Get());
}

bool WebViewPanel::isInitialized() const
{
    return m_isInitialized;
}

void WebViewPanel::setOnInitialized(std::function<void()> callback)
{
    m_onInitialized = std::move(callback);
}

void WebViewPanel::close()
{
    // Clear any stored callback before tearing down the WebView.
    m_onInitialized = nullptr;

    if (m_controller)
    {
        m_controller->Close();
        m_controller.Reset();
    }

    m_webview.Reset();
    m_parentHwnd = nullptr;
    m_isInitialized = false;

    LOG_INFO(m_logger, "WebView2 fermé et ressources libérées");
}

void WebViewPanel::setVirtualHostMapping(const std::wstring& hostname, const std::wstring& folderPath)
{
    if (!m_webview)
    {
        LOG_ERROR(m_logger, "setVirtualHostMapping() appelé avant initialisation");
        return;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_3> webview3;
    if (FAILED(m_webview.As(&webview3)))
    {
        LOG_ERROR(m_logger, "ICoreWebView2_3 non disponible");
        return;
    }

    webview3->SetVirtualHostNameToFolderMapping(
        hostname.c_str(),
        folderPath.c_str(),
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
    );
}

// =============================================================================
// Initialisation interne
// =============================================================================

void WebViewPanel::initializeWebView()
{
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, // runtime installé (Evergreen)
        nullptr,
        nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (!env)
                {
                    LOG_ERROR(m_logger, "Échec création environnement WebView2");
                    return E_FAIL;
                }

                env->CreateCoreWebView2Controller(
                    m_parentHwnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (!controller)
                            {
                                LOG_ERROR(m_logger, "Échec création controller WebView2");
                                return E_FAIL;
                            }

                            m_controller = controller;
                            m_controller->get_CoreWebView2(&m_webview);

                            resize();

                            m_isInitialized = true;

                            LOG_INFO(m_logger, "WebView2 initialisé avec succès");

                            if (m_onInitialized)
                            {
                                m_onInitialized();
                            }

                            return S_OK;
                        }).Get());

                return S_OK;
            }).Get());

    if (FAILED(hr))
    {
        LOG_ERROR(m_logger, "CreateCoreWebView2EnvironmentWithOptions FAILED");
    }
}