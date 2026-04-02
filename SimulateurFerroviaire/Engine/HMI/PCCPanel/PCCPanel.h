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
 * @par Cache de projection (optimisation v2)
 * La projection logique → écran (@ref TCORenderer::Projection) est mise en
 * cache dans @c m_cachedProj. Elle est recalculée uniquement si la fenêtre
 * a été redimensionnée (@c m_lastRect changé) ou si le graphe a été
 * reconstruit (@c m_projDirty = true dans @ref rebuild).
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
#include "TCORenderer.h"
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
     * @param hParent   Handle de la fenêtre parente (@ref MainWindow).
     * @param hInstance Handle de l'instance Win32 de l'application.
     * @throws std::runtime_error Si @c CreateWindowExW retourne @c nullptr.
     */
    void create(HWND hParent, HINSTANCE hInstance);

    /**
     * @brief Alterne la visibilité du panneau (masqué ↔ visible).
     *
     * Lors d'une transition masqué → visible, redimensionne d'abord le
     * panneau puis invalide le rectangle pour un @c WM_PAINT immédiat.
     */
    void toggle();

    /**
     * @brief Redimensionne le panneau pour couvrir toute la zone cliente du parent.
     *
     * À appeler depuis le gestionnaire @c WM_SIZE de @ref MainWindow.
     */
    void resize();

    /**
     * @brief Force un rafraîchissement du dessin TCO.
     *
     * Reconstruit le graphe puis invalide la fenêtre si visible.
     * À appeler depuis @ref MainWindow::onParsingSuccess.
     */
    void refresh();

    /**
     * @brief Indique si le panneau est actuellement visible.
     * @return @c true si la fenêtre existe et est visible.
     */
    bool isVisible() const;

private:

    // =========================================================================
    // WndProc
    // =========================================================================

    static LRESULT CALLBACK windowProc(HWND hWnd, UINT msg,
        WPARAM wParam, LPARAM lParam);

    LRESULT handleMessage(HWND hWnd, UINT msg,
        WPARAM wParam, LPARAM lParam);

    /**
     * @brief Gestionnaire de @c WM_PAINT — délègue le dessin à @ref TCORenderer.
     *
     * Utilise le cache de projection (@c m_cachedProj) :
     * recalcule via @c TCORenderer::computeProjection uniquement si
     * @c m_projDirty est vrai ou si la taille de la fenêtre a changé.
     *
     * @param hWnd Handle de la fenêtre à peindre.
     */
    void onPaint(HWND hWnd);

    /**
     * @brief Reconstruit le graphe PCC depuis @ref TopologyRepository.
     *
     * Appelle successivement @ref PCCGraphBuilder::build et @ref PCCLayout::compute.
     * Invalide le cache de projection (@c m_projDirty = true).
     * No-op si @ref TopologyRepository est vide.
     */
    void rebuild();

    // =========================================================================
    // Membres — fenêtre Win32
    // =========================================================================

    HWND      m_hWnd = nullptr;
    HWND      m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;

    // =========================================================================
    // Membres — données
    // =========================================================================

    /**
     * Logger HMI partagé, fourni par @ref MainWindow.
     * Déclaré avant @ref m_graph — transmis au constructeur de PCCGraph.
     */
    Logger& m_logger;

    /**
     * Graphe PCC possédé par ce panneau.
     * Reconstruit à chaque appel à @ref rebuild.
     * Déclaré après @ref m_logger — reçoit m_logger dans son constructeur.
     */
    PCCGraph m_graph;

    // =========================================================================
    // Membres — cache de projection (Famille F)
    // =========================================================================

    /**
     * Dernière RECT passée à computeProjection.
     * Comparée à la RECT courante dans onPaint pour détecter un resize.
     */
    RECT m_lastRect = {};

    /**
     * Projection logique → écran mise en cache.
     * Valide tant que m_projDirty est false et que la RECT n'a pas changé.
     */
    TCORenderer::Projection m_cachedProj = {};

    /**
     * Indicateur d'invalidation du cache.
     * Mis à true par rebuild() (nouveau parsing) et lors du premier paint.
     * Remis à false après recalcul dans onPaint().
     */
    bool m_projDirty = true;

    // =========================================================================
    // Constantes
    // =========================================================================

    static constexpr wchar_t CLASS_NAME[] = L"PCCPanelClass";
};