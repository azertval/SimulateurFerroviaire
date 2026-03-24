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

#include <optional>
#include <string>

#include "framework.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Config/ParserConfigIni.h"
#include "Engine/HMI/ProgressBar.h"
#include "Engine/HMI/WebViewPanel/WebViewPanel.h"
#include "Engine/HMI/PCCPanel/PCCPanel.h"
#include "Modules/GeoParser/GeoParsingTask.h"

 /**
  * @class MainWindow
  * @brief Fenêtre principale de l'application SimulateurFerroviaire.
  *
  * Responsabilités :
  *  - Créer et afficher la fenêtre principale.
  *  - Gérer l'ensemble des messages Win32 entrants via @ref handleMessage.
  *  - Déclencher l'ouverture d'un fichier GeoJSON et le parsing asynchrone
  *    via @ref GeoParsingTask.
  *  - Mettre à jour la @ref ProgressBar (label + valeur + bouton Cancel)
  *    en réponse aux messages inter-threads.
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
     * @brief Crée et affiche la fenêtre Win32, initialise la ProgressBar
     *        et charge la @ref ParserConfig depuis le fichier .ini.
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
    static LRESULT CALLBACK windowProc(HWND hWnd, UINT message,
        WPARAM wParam, LPARAM lParam);

private:

    // =========================================================================
    // Gestionnaires de messages
    // =========================================================================

    /**
     * @brief Dispatcher principal des messages de la fenêtre.
     */
    LRESULT handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Gère les commandes menu et boutons (@c WM_COMMAND).
     *
     * Traite : IDM_FILE_OPEN, IDM_FILE_EXPORT, IDM_VIEW_PCC,
     *          IDM_PARSER_SETTINGS, IDM_ABOUT, IDM_EXIT,
     *          IDC_CANCEL_PARSING.
     *
     * @param hWnd      Handle de la fenêtre.
     * @param commandId Identifiant de la commande (issu de @c LOWORD(wParam)).
     */
    void onCommand(HWND hWnd, int commandId);

    /**
     * @brief Gère la mise à jour de la progression (@c WM_PROGRESS_UPDATE).
     *
     * Met à jour la barre et le label. Libère le @c std::wstring* transporté
     * dans @p lParam, alloué par @ref GeoParsingTask.
     *
     * @param progress  Valeur de progression (0–100), depuis @c wParam.
     * @param label     Pointeur vers le label de phase (propriété transférée).
     */
    void onProgressUpdate(int progress, std::wstring* label);

    /**
     * @brief Gère la fin réussie du parsing (@c WM_PARSING_SUCCESS).
     *
     * Injecte le rendu GeoJSON dans le WebView et réinitialise la ProgressBar.
     *
     * @param hWnd Handle de la fenêtre parente.
     */
    void onParsingSuccess(HWND hWnd);

    /**
     * @brief Gère un échec de parsing (@c WM_PARSING_ERROR).
     *
     * Affiche le message d'erreur transporté par @p lParam, libère la
     * mémoire allouée par le thread de parsing, puis réinitialise la ProgressBar.
     *
     * @param hWnd   Handle de la fenêtre parente.
     * @param lParam Pointeur vers un @c std::wstring alloué par GeoParsingTask.
     */
    void onParsingError(HWND hWnd, LPARAM lParam);

    /**
     * @brief Gère une annulation propre du parsing (@c WM_PARSING_CANCELLED).
     *
     * Réinitialise la ProgressBar sans afficher d'erreur.
     */
    void onParsingCancelled();

    /**
     * @brief Ouvre le sélecteur de fichier et lance le parsing asynchrone.
     *
     * @param hWnd Handle de la fenêtre principale.
     */
    void onFileOpen(HWND hWnd);

    /**
     * @brief Ouvre le dialogue d'export et déclenche l'export GeoJSON.
     *
     * @param hWnd Handle de la fenêtre principale.
     */
    void onFileExport(HWND hWnd);

    /**
     * @brief Demande l'annulation du parsing en cours via @ref GeoParsingTask.
     *
     * Appelé quand l'utilisateur clique sur le bouton "Annuler" (@c IDC_CANCEL_PARSING).
     */
    void onCancelButtonClick();

    /**
     * @brief Ouvre le dialogue des paramètres du parser (@c IDM_PARSER_SETTINGS).
     *
     * Si l'utilisateur valide, sauvegarde la config dans @c m_parserIniPath.
     * Le prochain parsing utilisera les nouveaux paramètres.
     */
    void onParserSettings();

    /**
     * @brief Appelé lors d'un redimensionnement — ajuste WebView et PCCPanel.
     */
    void onSizeUpdate();

    /**
     * @brief Nettoie les ressources avant la destruction (@c WM_DESTROY).
     */
    void onDestroy();

    /**
     * @brief Dispatcher des messages JSON reçus depuis Leaflet.
     *
     * @param jsonMessage  Contenu brut du postMessage (UTF-8).
     */
    void onWebMessage(const std::string& jsonMessage);

    /**
     * @brief Met à jour l'état d'un SwitchBlock après un clic Leaflet.
     *
     * @param switchId  Identifiant du switch (ex. "sw/0").
     */
    void onSwitchClick(const std::string& switchId);

    /**
     * @brief Bascule la visibilité du panneau PCC (F2 ou IDM_VIEW_PCC).
     */
    void onTogglePCC();


    // =========================================================================
    // Membres
    // =========================================================================

    /** Handle Win32 de la fenêtre physique (valide après @ref create). */
    HWND        m_hWnd = nullptr;
    HINSTANCE   m_hInstance;
    const WCHAR* m_className;
    const WCHAR* m_title;
    int          m_nCmdShow;

    /** Barre de progression affichée lors du parsing. */
    ProgressBar m_progressBar;

    /**
     * Configuration du pipeline GeoParser.
     * Chargée depuis @c m_parserIniPath au démarrage (@ref create),
     * potentiellement modifiée via @ref onParserSettings.
     */
    ParserConfig m_parserConfig;

    /**
     * Chemin du fichier .ini de configuration du parser.
     * Initialisé par @c ParserConfigIni::defaultPath() dans @ref create.
     */
    std::string m_parserIniPath;

    /**
     * Tâche asynchrone du pipeline GeoParser.
     * Instanciée dans @ref create une fois @c m_hWnd disponible.
     */
    std::optional<GeoParsingTask> m_parserTask;

    /** Logger HMI. */
    Logger m_logger{ "HMI" };

    /** Panneau WebView2 pour l'affichage de la carte ferroviaire. */
    WebViewPanel m_webViewPanel{ m_logger };

    /** Panneau PCC superposé — togglé via F2 ou IDM_VIEW_PCC. */
    PCCPanel m_pccPanel{ m_logger };
};