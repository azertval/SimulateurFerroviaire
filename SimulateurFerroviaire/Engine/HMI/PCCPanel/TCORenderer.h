/**
 * @file  TCORenderer.h
 * @brief Renderer GDI du schéma TCO depuis le @ref PCCGraph.
 *
 * Classe utilitaire statique. Lit @ref PCCGraph au moment du dessin
 * et projette les positions logiques X/Y en pixels sur la zone cliente.
 *
 * @par Conventions de couleurs (style TCO SNCF)
 *  - Fond                    : noir   RGB(  0,  0,  0)
 *  - Voie libre   (FREE)     : blanc  RGB(220,220,220)
 *  - Voie occupée (OCCUPIED) : rouge  RGB(220, 50, 50)
 *  - Voie inactive (INACTIVE): gris   RGB( 80, 80, 80)
 *  - Branche normale active  : vert   RGB(  0,200, 80)
 *  - Branche déviation active: jaune  RGB(220,200,  0)
 *
 * @par Projection logique → pixels
 * pixelX = marginX + logicalX * cellWidth
 * pixelY = centerY - logicalY * cellHeight  (Y inversé)
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "framework.h"
#include "Modules/PCC/PCCGraph.h"
#include "Modules/PCC/PCCSwitchNode.h"
#include "Engine/Core/Logger/Logger.h"

class TCORenderer
{
public:

    /**
     * @brief Point d'entrée unique du rendu TCO.
     *
     * Dessine fond noir, puis arêtes (@ref drawEdges), puis nœuds
     * (@ref drawNodes). Si @p graph est vide, seul le fond est dessiné.
     *
     * @param hdc     Contexte de périphérique GDI (fourni par BeginPaint).
     * @param rect    Zone cliente dans laquelle dessiner.
     * @param graph   Graphe PCC à rendre — positions calculées par PCCLayout.
     * @param logger  Référence au logger HMI.
     */
    static void draw(HDC hdc, const RECT& rect,
        const PCCGraph& graph, Logger& logger);

    TCORenderer() = delete;

private:

    /**
     * @brief Paramètres de projection positions logiques → pixels.
     *
     * Calculés une fois par frame dans @ref computeProjection.
     */
    struct Projection
    {
        int minX = 0;   ///< X logique minimal observé.
        int maxX = 0;   ///< X logique maximal observé.
        int minY = 0;   ///< Y logique minimal observé.
        int maxY = 0;   ///< Y logique maximal observé.
        int width = 0;   ///< Largeur zone cliente en pixels.
        int height = 0;   ///< Hauteur zone cliente en pixels.
        int cellWidth = 1;   ///< Espacement horizontal entre nœuds en pixels.
        int cellHeight = 1;   ///< Espacement vertical entre rangs en pixels.
        int marginX = 0;   ///< Marge gauche en pixels.
        int centerY = 0;   ///< Y pixels correspondant au backbone (y=0).
    };

    /**
     * @brief Calcule la projection depuis les positions logiques du graphe.
     *
     * Détermine les bornes X/Y logiques, la taille des cellules et le
     * centre vertical (backbone y=0).
     *
     * @param rect    Zone cliente.
     * @param graph   Graphe dont les positions sont lues.
     * @param logger  Référence au logger HMI.
     *
     * @return Projection initialisée, prête à l'emploi.
     */
    static Projection computeProjection(const RECT& rect,
        const PCCGraph& graph,
        Logger& logger);

    /**
     * @brief Projette une position logique en coordonnées pixels.
     *
     * @param logicalX  Position X logique (profondeur BFS).
     * @param logicalY  Position Y logique (rang vertical).
     * @param proj      Projection précalculée.
     *
     * @return @c POINT en coordonnées pixels.
     */
    static POINT project(int logicalX, int logicalY, const Projection& proj);

    /**
     * @brief Dessine toutes les arêtes du graphe.
     *
     * Un segment est tracé entre la position du nœud source et celle du
     * nœud cible. La couleur dépend du rôle de l'arête et de la branche
     * active du switch source (voir @ref resolveEdgeColor).
     *
     * @param hdc     Contexte de périphérique GDI.
     * @param proj    Projection courante.
     * @param graph   Graphe dont les arêtes sont dessinées.
     * @param logger  Référence au logger HMI.
     */
    static void drawEdges(HDC hdc, const Projection& proj,
        const PCCGraph& graph, Logger& logger);

    /**
     * @brief Dessine un marqueur pour chaque nœud du graphe.
     *
     * @ref PCCSwitchNode : disque de rayon @c DOT_RADIUS_SWITCH.
     * @ref PCCStraightNode : disque de rayon @c DOT_RADIUS_STRAIGHT.
     *
     * @param hdc     Contexte de périphérique GDI.
     * @param proj    Projection courante.
     * @param graph   Graphe dont les nœuds sont dessinés.
     * @param logger  Référence au logger HMI.
     */
    static void drawNodes(HDC hdc, const Projection& proj,
        const PCCGraph& graph, Logger& logger);

    /**
     * @brief Résout la couleur d'une arête selon son rôle et l'état source.
     *
     * ROOT / STRAIGHT  → stateToColor(from.getState())
     * NORMAL           → COLORS.normal    si branche active == NORMAL
     * DEVIATION        → COLORS.deviation si branche active == DEVIATION
     *
     * @param edge  Arête dont la couleur est résolue.
     *
     * @return Couleur COLORREF à appliquer au segment GDI.
     */
    static COLORREF resolveEdgeColor(const PCCEdge* edge);

    /**
     * @brief Mappe un @ref ShuntingState vers une couleur GDI.
     *
     *  - FREE     → RGB(220,220,220)
     *  - OCCUPIED → RGB(220, 50, 50)
     *  - INACTIVE → RGB( 80, 80, 80)
     *
     * @param state  État opérationnel de l'élément.
     *
     * @return Couleur COLORREF correspondante.
     */
    static COLORREF stateToColor(ShuntingState state);

    /**
     * @brief Dessine un segment GDI entre deux points écran.
     *
     * @param hdc       Contexte de périphérique GDI.
     * @param from      Point de départ en pixels.
     * @param to        Point d'arrivée en pixels.
     * @param color     Couleur COLORREF du trait.
     * @param lineWidth Épaisseur du trait en pixels.
     */
    static void drawLine(HDC hdc, POINT from, POINT to,
        COLORREF color, int lineWidth = 2);

    /**
     * @brief Dessine un disque plein centré sur @p center.
     *
     * @param hdc    Contexte de périphérique GDI.
     * @param center Centre du disque en pixels.
     * @param radius Rayon en pixels.
     * @param color  Couleur de remplissage COLORREF.
     */
    static void drawDot(HDC hdc, POINT center, int radius, COLORREF color);
};