/**
 * @file PCCPanel.h
 * @brief Panneau PCC superposé togglable, affiché par-dessus la carte Leaflet.
 *
 * La classe @ref PCCPanel implémente un panneau enfant Win32 (WS_CHILD) qui
 * se superpose au @ref WebViewPanel sans l'affecter. Il est affiché ou masqué
 * via @ref toggle (typiquement branché sur F2 dans @ref MainWindow).
 *
 * Le rendu du schéma TCO est entièrement délégué à @ref TCORenderer, appelé
 * dans @c WM_PAINT. PCCPanel ne contient aucune logique de dessin.
 *
 * @par Cycle de vie
 *  -# @ref create — enregistre la classe Win32 et crée la fenêtre enfant.
 *  -# @ref toggle — alterne visibilité.
 *  -# @ref resize — appelé sur @c WM_SIZE du parent pour couvrir toute la zone cliente.
 *  -# @ref refresh — invalide le rectangle pour forcer un @c WM_PAINT.
 *
 * @par Patron de conception – WndProc statique
 * Identique à @ref MainWindow : @c this est stocké dans @c GWLP_USERDATA
 * lors de @c WM_NCCREATE, permettant à @ref windowProc de dispatcher vers
 * @ref handleMessage.
 *
 * @note Une seule instance est prévue, créée et possédée par @ref MainWindow.
 */
#pragma once
#include "framework.h"
#include "Engine/Core/Logger/Logger.h"
#include "Modules/PCC/PCCGraph.h"

class PCCPanel
{
public:
    /**
     * @brief Construit le PCCPanel avec un logger externe.
     *
     * @param logger  Référence au logger HMI fourni par @ref MainWindow.
     *                Doit rester valide pour toute la durée de vie du panneau.
     */
    explicit PCCPanel(Logger& logger);

    /**
     * @brief Enregistre la classe Win32 et crée la fenêtre enfant masquée.
     *
     * La fenêtre est positionnée à (0, 0) avec les dimensions de la zone
     * cliente de @p hParent. Elle est masquée (@c SW_HIDE) jusqu'au premier
     * appel à @ref toggle.
     *
     * L'enregistrement de la classe est idempotent : si @c RegisterClassExW
     * retourne @c ERROR_CLASS_ALREADY_EXISTS, l'erreur est ignorée.
     *
     * @param hParent   Handle de la fenêtre parente (@ref MainWindow).
     * @param hInstance Handle de l'instance Win32 de l'application.
     *
     * @throws std::runtime_error Si @c CreateWindowExW retourne @c nullptr.
     */
    void create(HWND hParent, HINSTANCE hInstance);

    /**
     * @brief Alterne la visibilité du panneau (masqué ↔ visible).
     *
     * Lors d'une transition masqué → visible, redimensionne d'abord le
     * panneau via @ref resize, puis invalide le rectangle pour déclencher
     * un @c WM_PAINT immédiat. Aucune action si @ref create n'a pas été
     * appelé.
     */
    void toggle();

    /**
     * @brief Redimensionne le panneau pour couvrir toute la zone cliente du parent.
     *
     * Interroge @c GetClientRect sur la fenêtre parente et applique les
     * dimensions via @c SetWindowPos. À appeler depuis le gestionnaire
     * @c WM_SIZE de @ref MainWindow.
     *
     * Aucune action si @ref create n'a pas été appelé.
     */
    void resize();

    /**
     * @brief Force un rafraîchissement du dessin TCO.
     *
     * Invalide l'intégralité du rectangle client via @c InvalidateRect,
     * ce qui provoque un @c WM_PAINT au prochain cycle de messages.
     * N'effectue rien si le panneau est masqué ou non créé.
     *
     * À appeler depuis @ref MainWindow::onParsingSuccess pour mettre à
     * jour le schéma après chargement d'un fichier GeoJSON.
     */
    void refresh();

    /**
     * @brief Indique si le panneau est actuellement visible.
     *
     * @return @c true si la fenêtre existe et est visible (@c IsWindowVisible),
     *         @c false sinon.
     */
    bool isVisible() const;

private:

    // =========================================================================
    // WndProc
    // =========================================================================

    /**
     * @brief Procédure de fenêtre statique, point d'entrée imposé par Win32.
     *
     * Lors du premier message (@c WM_NCCREATE), stocke le pointeur @c this
     * (passé via @c CREATESTRUCT::lpCreateParams) dans @c GWLP_USERDATA,
     * puis délègue chaque message à @ref handleMessage.
     *
     * @param hWnd    Handle de la fenêtre.
     * @param msg     Identifiant du message Win32.
     * @param wParam  Paramètre WPARAM.
     * @param lParam  Paramètre LPARAM.
     *
     * @return Résultat du traitement du message.
     */
    static LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Dispatcher principal des messages de la fenêtre.
     *
     * Route @c WM_PAINT vers @ref onPaint et @c WM_ERASEBKGND vers un
     * handler no-op (supprime le flickering). Tous les autres messages
     * sont transmis à @c DefWindowProcW.
     *
     * @param hWnd    Handle de la fenêtre.
     * @param msg     Identifiant du message Win32.
     * @param wParam  Paramètre WPARAM.
     * @param lParam  Paramètre LPARAM.
     *
     * @return Résultat du traitement du message.
     */
    LRESULT handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Gestionnaire de @c WM_PAINT — délègue le dessin à @ref TCORenderer.
     *
     * Ouvre un @c BeginPaint / @c EndPaint, interroge @c GetClientRect pour
     * obtenir les dimensions courantes, puis appelle @c TCORenderer::draw.
     * Aucune logique de dessin n'est présente ici.
     *
     * @param hWnd Handle de la fenêtre à peindre.
     */
    void onPaint(HWND hWnd);

    /**
    * @brief Reconstruit le graphe PCC depuis @ref TopologyRepository.
    *
    * Appelle successivement :
    *  -# @ref PCCGraphBuilder::build — crée nœuds et arêtes.
    *  -# @ref PCCLayout::compute — calcule les positions X/Y.
    *
    * No-op si @ref TopologyRepository est vide.
    */
    void rebuild();

    // =========================================================================
    // Membres
    // =========================================================================
    /** Handle Win32 de la fenêtre enfant (valide après @ref create). */
    HWND m_hWnd = nullptr;

    /** Handle de la fenêtre parente (@ref MainWindow). */
    HWND m_hParent = nullptr;

    /** Handle de l'instance Win32 de l'application. */
    HINSTANCE m_hInstance = nullptr;

    /**
     * Logger HMI partagé, fourni par @ref MainWindow.
     * Déclaré avant @ref m_graph — transmis au constructeur de PCCGraph.
     */
    Logger& m_logger;

    /**
     * Graphe PCC possédé par ce panneau.
     * Membre valeur — durée de vie identique à PCCPanel.
     * Reconstruit à chaque appel à @ref rebuild.
     * Déclaré après @ref m_logger — reçoit m_logger dans son constructeur.
     */
    PCCGraph m_graph;

    /** Nom de la classe Win32 enregistrée pour @ref PCCPanel. */
    static constexpr wchar_t CLASS_NAME[] = L"PCCPanelClass";
};