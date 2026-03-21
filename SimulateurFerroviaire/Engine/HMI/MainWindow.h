/**
 * @file MainWindow.h
 * @brief Déclaration de la fenêtre principale de l'application.
 *
 * La classe @ref MainWindow gère :
 *  - La création physique de la fenêtre Win32 (@c CreateWindowW).
 *  - Le routage des messages Windows (@c WM_COMMAND, @c WM_PAINT, …).
 *  - La coordination entre l'interface utilisateur et les modules métier
 *    (ouverture de fichier, lancement du parsing, retour de progression).
 *
 * @par Patron de conception – WndProc statique
 * Win32 impose une procédure de fenêtre statique (ou globale). Le pointeur
 * @c this est stocké dans @c GWLP_USERDATA lors de @c WM_NCCREATE, ce qui
 * permet à la méthode statique @ref windowProc de dispatcher vers la méthode
 * d'instance @ref handleMessage.
 */

#pragma once

#include "framework.h"
#include "Engine/HMI/ProgressBar.h"
#include "Engine/HMI/WebViewPanel/WebViewPanel.h"
#include "Engine/HMI/PCCPanel/PCCPanel.h"

/**
 * @class MainWindow
 * @brief Fenêtre principale de l'application SimulateurFerroviaire.
 *
 * Responsabilités :
 *  - Créer et afficher la fenêtre principale.
 *  - Gérer l'ensemble des messages Win32 entrants via @ref handleMessage.
 *  - Déclencher l'ouverture d'un fichier GeoJSON et le parsing asynchrone.
 *  - Mettre à jour la @ref ProgressBar en réponse aux messages inter-threads.
 */
class MainWindow
{
public:

    /**
     * @brief Construit la fenêtre principale (sans la créer physiquement).
     *
     * @param hInstance   Handle de l'instance Win32.
     * @param className   Nom de la classe enregistrée par @ref Application.
     * @param title       Titre affiché dans la barre de la fenêtre.
     * @param nCmdShow    Mode d'affichage (@c SW_SHOW, @c SW_HIDE, …).
     */
    MainWindow(HINSTANCE hInstance,
               const WCHAR* className,
               const WCHAR* title,
               int nCmdShow);

    /**
     * @brief Crée et affiche la fenêtre Win32, puis initialise la ProgressBar.
     *
     * @throws std::runtime_error Si @c CreateWindowW retourne @c nullptr.
     */
    void create();

    /**
     * @brief Procédure de fenêtre statique, point d'entrée imposé par Win32.
     *
     * Lors du premier message (@c WM_NCCREATE), stocke le pointeur @c this
     * dans @c GWLP_USERDATA, puis délègue chaque message à @ref handleMessage.
     *
     * @param hWnd    Handle de la fenêtre cible.
     * @param message Identifiant du message Win32.
     * @param wParam  Paramètre mot de message.
     * @param lParam  Paramètre long de message.
     *
     * @return Résultat du traitement du message.
     */
    static LRESULT CALLBACK windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:

    // =========================================================================
    // Gestionnaires de messages
    // =========================================================================

    /**
     * @brief Dispatcher principal des messages de la fenêtre.
     *
     * Reçoit chaque message depuis @ref windowProc et le route vers le
     * gestionnaire spécialisé approprié.
     *
     * @param hWnd    Handle de la fenêtre.
     * @param message Identifiant du message.
     * @param wParam  Paramètre WPARAM.
     * @param lParam  Paramètre LPARAM.
     *
     * @return Résultat du traitement.
     */
    LRESULT handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Gère les commandes menu (@c WM_COMMAND).
     *
     * Traite les identifiants @c IDM_FILE_OPEN, @c IDM_ABOUT et @c IDM_EXIT.
     *
     * @param hWnd      Handle de la fenêtre.
     * @param commandId Identifiant de la commande (issu de @c LOWORD(wParam)).
     */
    void onCommand(HWND hWnd, int commandId);

    /**
     * @brief Gère la mise à jour de la progression (@c WM_PROGRESS_UPDATE).
     *
     * Appelé depuis le thread principal en réponse à un @c PostMessage
     * émis par le thread de parsing.
     *
     * @param progressValue Valeur de progression (0–100).
     */
    void onProgressUpdate(int progressValue);

    /**
     * @brief Gère la fin réussie du parsing (@c WM_PARSING_SUCCESS).
     *
     * Génère le script d'injection GeoJSON @ref TopologyRenderer 
     * et l'exécute dans le WebView pour afficher la carte. 
     *
     * @param hWnd Handle de la fenêtre parente pour la boîte de dialogue.
     */
    void onParsingSuccess(HWND hWnd);

    /**
     * @brief Gère un échec de parsing (@c WM_PARSING_ERROR).
     *
     * Affiche le message d'erreur transporté par @p lParam, libère la
     * mémoire allouée par le thread de parsing, puis masque la ProgressBar.
     *
     * @param hWnd   Handle de la fenêtre parente.
     * @param lParam Pointeur vers un @c std::string alloué par le thread de parsing.
     *               Ce pointeur est libéré ici après utilisation.
     */
    void onParsingError(HWND hWnd, LPARAM lParam);

    /**
     * @brief Ouvre le sélecteur de fichier et lance le parsing asynchrone.
     *
     * Délègue la sélection à @ref FileOpenDialog, puis déclenche
     * @ref GeoParsingTask si un fichier est sélectionné.
     * 
     * @param hWnd Handle de la fenêtre principale (propriétaire du dialogue).
     */
    void onFileOpen(HWND hWnd);

    /**
    * @brief Ouvre le dialogue d'exportation et déclenche l'export GeoJSON.
     *
     * Délègue la sélection à un dialogue d'exportation @ref FileOpenDialog, puis déclenche
     * puis déclenche l'export via un module dédié @ref TopologyRenderer
     *
     * @param hWnd Handle de la fenêtre principale (propriétaire du dialogue).
    */
    void onFileExport(HWND hWnd);

    /**
     * @brief Quand la fenêtre est redimensionnée, ajuste les éléments graphiques en conséquence.
     *
     * @param None
     */
    void onSizeUpdate();

    /**
    * @brief Nettoie les ressources avant la destruction de la fenêtre (WM_DESTROY)
    */
    void onDestroy();

    /**
    * @brief Dispatcher principal des messages JSON reçus depuis Leaflet.
    *
    * Branché sur WebViewPanel via setOnMessageReceived() dans create().
    * Parse le JSON, identifie le champ "type", et délègue au handler
    * spécialisé. Un JSON malformé est loggé et ignoré — jamais de crash.
    *
    * @param jsonMessage  Contenu brut du postMessage (UTF-8).
    */
    void onWebMessage(const std::string& jsonMessage);

    /**
    * @brief Met à jour l'état opérationnel d'un SwitchBlock après un clic.
    *
    * Localise le switch par ID dans TopologyRepository, convertit la chaîne
    * JS "normal"/"deviation" en ActiveBranch, et appelle setActiveBranch().
    *
    * Le visuel Leaflet est déjà à jour (mise à jour optimiste côté JS).
    * Si à l'avenir une validation C++ peut rejeter le changement, envoyer
    * ici un script de correction via executeScript().
    *
    * @param switchId  Identifiant du switch (ex. "sw/0").
    * @param active    "normal" ou "deviation".
    */
    void onSwitchClick(const std::string& switchId);

    /**
     * @brief Bascule la visibilité du panneau PCC.
     *
     * Délègue à @ref PCCPanel::toggle. Appelé depuis @ref onCommand
     * (IDM_VIEW_PCC) et depuis le gestionnaire @c WM_KEYDOWN (touche F2).
     */
    void onTogglePCC();


    // =========================================================================
    // Membres
    // =========================================================================

    /** Handle Win32 de la fenêtre physique (valide après @ref create). */
    HWND m_hWnd = nullptr;

    /** Handle de l'instance Win32 de l'application. */
    HINSTANCE m_hInstance;

    /** Nom de la classe Win32 enregistrée. */
    const WCHAR* m_className;

    /** Titre de la fenêtre. */
    const WCHAR* m_title;

    /** Mode d'affichage initial. */
    int m_nCmdShow;

    /** Barre de progression affichée lors du parsing. */
    ProgressBar m_progressBar;

    /** Logger dédié à la couche HMI, utilisé pour tracer les événements et erreurs liés à l'interface utilisateur. */
    Logger m_logger{"HMI"};

    /** Panneau WebView2 pour l'affichage de la carte ferroviaire. */
    WebViewPanel m_webViewPanel{m_logger};

    /**
     * @brief Panneau PCC superposé, togglé via F2 ou menu Vue → Panneau PCC.
     * Créé dans @ref create, masqué par défaut.
     */
    PCCPanel m_pccPanel{m_logger};
};
