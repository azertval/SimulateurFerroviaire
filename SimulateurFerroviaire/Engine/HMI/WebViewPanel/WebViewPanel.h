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
    * @brief Interdit la copie — chaque instance gère ses propres ressources COM.
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
    * @brief Interdit l'affectation — chaque instance gère ses propres ressources COM.
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
    * @brief Exécute un script JavaScript dans le contexte de la page chargée.
     *
     * Permet d'interagir dynamiquement avec le contenu HTML, par exemple pour
     * manipuler la carte Leaflet ou récupérer des données depuis la page.
     * 
     * Log erreur si le WebView n'est pas encore initialisé ou si l'exécution échoue.
     *
     * @param script  Code JavaScript à exécuter. Doit être une expression valide.
     *                Le résultat de l'exécution est ignoré (void).
    */
    void executeScript(const std::wstring& script);

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

    /**
     * @brief Mappe un nom d'hôte virtuel vers un dossier local.
     * Permet de servir des fichiers locaux via https://hostname/...
     */
    void setVirtualHostMapping(const std::wstring& hostname, const std::wstring& folderPath);

    /**
     * @brief Enregistre le handler des messages JS entrants.
     *
     * Appelé par MainWindow::create() AVANT l'initialisation du WebView.
     * Le callback reçoit le message brut UTF-8 envoyé depuis Leaflet via :
     *   window.chrome.webview.postMessage(JSON.stringify({...}))
     *
     * Exécuté sur le thread UI — accès aux objets Win32 et au modèle sûr.
     * Si non enregistré, les messages JS sont ignorés silencieusement.
     *
     * @param callback  Handler à appeler avec le message JSON en paramètre.
     */
    void setOnMessageReceived(std::function<void(const std::string&)> callback);

private:
    // -------------------------------------------------------------------------
    // Méthodes privées
    // -------------------------------------------------------------------------

    /**
    * @brief Initialise l'environnement WebView2 de manière asynchrone.
    *
    *Lance CreateCoreWebView2EnvironmentWithOptions avec les options par défaut
    * (runtime Evergreen installé sur le système).Le résultat est traité dans
    * onEnvironmentCreated via un callback WRL.
    */
    void initializeWebView();

    /**
     * @brief Callback déclenché à la fin de la création de l'environnement WebView2.
     *
     * Vérifie que l'environnement est valide puis lance la création du contrôleur.
     *
     * @param result  Code HRESULT retourné par l'API WebView2.
     * @param env     Pointeur vers l'environnement créé (nullptr en cas d'échec).
     */
    void onEnvironmentCreated(HRESULT result, ICoreWebView2Environment* env);

    /**
     * @brief Callback déclenché à la fin de la création du contrôleur WebView2.
     *
     * Initialise le contrôleur et la vue, enregistre le handler de messages,
     * redimensionne la vue et notifie l'appelant via m_onInitialized.
     *
     * @param result      Code HRESULT retourné par l'API WebView2.
     * @param controller  Pointeur vers le contrôleur créé (nullptr en cas d'échec).
     */
    void onControllerCreated(HRESULT result, ICoreWebView2Controller* controller);

    /**
     * @brief Callback statique déclenché à la réception d'un message JavaScript.
     *
     * Récupère le message UTF-16 envoyé depuis la page web, le convertit en
     * std::string puis l'affiche dans une boîte de dialogue.
     *
     * @note La conversion wstring → string est naïve (ASCII uniquement).
     *       Utiliser WideCharToMultiByte si les messages peuvent contenir de l'Unicode.
     *
     * @param sender  Interface WebView2 émettrice du message.
     * @param args    Arguments contenant le message brut.
     * @return        S_OK dans tous les cas (les erreurs sont silencieuses).
     */
    HRESULT onWebMessageReceived(ICoreWebView2* sender,
        ICoreWebView2WebMessageReceivedEventArgs* args);

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

    /** Handler des messages JS. Nullptr si non enregistré. */
    std::function<void(const std::string&)> m_onMessageReceived;
};