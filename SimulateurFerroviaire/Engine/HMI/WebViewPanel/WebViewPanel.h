#pragma once

#include <windows.h>
#include <string>
#include <wrl.h>
#include <WebView2.h>
#include <functional>

#include "Engine/Core/Logger/Logger.h"

/**
 * @class WebViewPanel
 * @brief Wrapper Win32 autour de WebView2 pour affichage HTML embarqué.
 *
 * Responsabilités :
 *  - Initialisation du runtime WebView2
 *  - Gestion du cycle de vie (controller + webview)
 *  - Resize automatique dans une fenêtre Win32
 *  - Navigation (URL / HTML string)
 *
 * Usage typique :
 * @code
 *   WebViewPanel panel(logger);
 *   panel.create(hwndParent);
 *   panel.navigate("https://google.com");
 * @endcode
 */
class WebViewPanel
{
public:

    // -------------------------------------------------------------------------
    // Construction / destruction
    // -------------------------------------------------------------------------
    /**
    * @brief Construit un WebViewPanel avec un logger pour les messages d'erreur et d'information.
     *
     * Le logger doit être fourni par le module parent (ex. MainWindow) pour centraliser les logs.
     *
     * @param logger  Référence à un Logger déjà initialisé, utilisé pour journaliser les événements du WebViewPanel.
     *               Doit rester valide pendant toute la durée de vie du WebViewPanel.
    */
    explicit WebViewPanel(Logger& logger);

    /**
    * @brief Destructeur — ferme proprement le WebView et libère les ressources associées.
     *
     * Appelle close() pour s'assurer que le WebView est fermé et que les ressources COM sont libérées.
     * Après destruction, le WebViewPanel ne doit plus être utilisé.
     *
     * Note : Si la fenêtre parent est détruite avant le WebViewPanel, assurez-vous d'appeler close()
     * manuellement pour éviter les fuites de ressources.
    */
    ~WebViewPanel();

    /**
    * @brief interdit la copie.
     *
     * Chaque WebViewPanel est unique et gère ses propres ressources COM. La copie
     * pourrait entraîner des fuites ou des conflits de ressources. Par conséquent,
     * l'opérateur d'affectation est supprimé pour garantir que chaque instance
     * est non copiable.
     *
     * @param other  L'autre instance de WebViewPanel à copier (non utilisé).
     * @return Référence à l'instance actuelle (non utilisée).
    */
    WebViewPanel(const WebViewPanel&) = delete;

    /**
    * @brief Opérateur d'affectation — interdit la copie.
     *
     * Chaque WebViewPanel est unique et gère ses propres ressources COM. La copie
     * pourrait entraîner des fuites ou des conflits de ressources. Par conséquent,
     * l'opérateur d'affectation est supprimé pour garantir que chaque instance
     * est non copiable.
     *
     * @param other  L'autre instance de WebViewPanel à copier (non utilisé).
     * @return Référence à l'instance actuelle (non utilisée).
    */
    WebViewPanel& operator=(const WebViewPanel&) = delete;

    // -------------------------------------------------------------------------
    // API publique
    // -------------------------------------------------------------------------

    /**
     * @brief Initialise WebView2 dans une fenêtre Win32 existante.
     * @param parentHwnd  Handle de la fenêtre parent.
     */
    void create(HWND parentHwnd);

    /**
     * @brief Redimensionne le WebView pour s'adapter à la fenêtre.
     */
    void resize();

    /**
     * @brief Navigue vers une URL.
     */
    void navigate(const std::wstring& url);

    /**
     * @brief Injecte du HTML directement.
     */
    void navigateToString(const std::wstring& htmlContent);

    /**
     * @brief Vérifie si le WebView est prêt.
     */
    bool isInitialized() const;

    /**
    * @brief Définit un callback à appeler une fois l'initialisation terminée.
     * @param callback Fonction à appeler après initialisation.
    */
    void setOnInitialized(std::function<void()> callback);

    /**
    * @brief Ferme le WebView et libère les ressources associées.
     *
     * Doit être appelé avant la destruction de la fenêtre parent pour éviter les fuites.
     * Après appel, le WebViewPanel ne doit plus être utilisé.
    */
    void close(); // Ferme proprement le WebView et libère les ressources


private:
    // -------------------------------------------------------------------------
    // Méthodes privées
    // -------------------------------------------------------------------------

    /**
    * @brief Initialise le runtime WebView2 et crée le controller et le webview.
     *
     * Cette méthode est appelée par create() après avoir stocké le handle de la fenêtre parent.
     * En cas d'échec, elle log une erreur et laisse m_isInitialized à false.
    */
    void initializeWebView();

    // -------------------------------------------------------------------------
    // Membres privés
    // -------------------------------------------------------------------------
    /*fichier log*/
    Logger& m_logger;

    /*Handle de l'instance Win32 de l'application parente*/
    HWND m_parentHwnd = nullptr;

    /*Pointeurs intelligents COM pour le controller et le webview*/
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;

    /*Pointeur intelligent COM pour l'instance WebView2 elle-même*/
    Microsoft::WRL::ComPtr<ICoreWebView2>           m_webview;

    /*Indique si le WebView est prêt à être utilisé (controller + webview créés) */
    bool m_isInitialized = false;

    /*Callback optionnel à appeler une fois l'initialisation terminée (ex. pour lancer la navigation) */
    std::function<void()> m_onInitialized;   
};