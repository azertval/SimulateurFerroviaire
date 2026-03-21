/**
 * @file  TCORenderer.h
 * @brief Renderer GDI du schéma TCO (Tableau de Contrôle Optique) ferroviaire.
 *
 * Classe utilitaire statique sans état. Lit @ref TopologyRepository au moment
 * du dessin et projette les coordonnées GPS (@ref LatLon) en pixels via une
 * normalisation min/max sur la zone cliente fournie.
 *
 * @par Conventions de couleurs (style TCO SNCF)
 *  - Fond                    : noir  (@c RGB(  0,  0,  0))
 *  - Voie libre   (FREE)     : blanc (@c RGB(220,220,220))
 *  - Voie occupée (OCCUPIED) : rouge (@c RGB(220, 50, 50))
 *  - Voie inactive (INACTIVE): gris  (@c RGB( 80, 80, 80))
 *  - Branche normale active  : vert  (@c RGB(  0,200, 80))
 *  - Branche déviation active: jaune (@c RGB(220,200,  0))
 *
 * @par Projection GPS → pixels
 * Longitudes et latitudes min/max calculées en un seul parcours sur l'ensemble
 * des éléments. Marge de 5 % appliquée de chaque côté. Axe Y inversé
 * (latitude croissante → Y décroissant).
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "framework.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/Topology/TopologyRepository.h"

class TCORenderer
{
public:

    /**
     * @brief Point d'entrée unique du rendu TCO.
     *
     * Lit @ref TopologyRepository, calcule la projection GPS → pixels,
     * remplit le fond noir, puis délègue aux méthodes privées de rendu
     * des StraightBlocks et SwitchBlocks. Aucun état n'est conservé
     * entre deux appels.
     *
     * Si @ref TopologyRepository est vide (parsing non encore effectué),
     * seul le fond noir est dessiné — aucune exception n'est levée.
     *
     * @param hdc    Contexte de périphérique GDI valide, fourni par @c BeginPaint.
     * @param rect   Zone cliente dans laquelle dessiner (origine = (0,0)).
     * @param logger Référence au logger HMI, fourni par @ref PCCPanel.
     */
    static void draw(HDC hdc, const RECT& rect, Logger& logger);

private:

    /**
     * @brief Paramètres de projection GPS → pixels, calculés une fois par frame.
     *
     * Encapsule les bornes GPS observées et les dimensions de la zone cliente
     * pour projeter n'importe quel @ref LatLon en @c POINT écran sans
     * recalculer les bornes à chaque appel.
     */
    struct Projection
    {
        /** Latitude minimale observée sur l'ensemble des éléments. */
        double minLat = 0.0;

        /** Latitude maximale observée. */
        double maxLat = 0.0;

        /** Longitude minimale observée. */
        double minLon = 0.0;

        /** Longitude maximale observée. */
        double maxLon = 0.0;

        /** Largeur de la zone cliente en pixels. */
        int width = 0;

        /** Hauteur de la zone cliente en pixels. */
        int height = 0;

        /** Marge relative appliquée de chaque côté (5 %). */
        static constexpr double MARGIN = 0.05;
    };

    /**
     * @brief Calcule la @ref Projection à partir du contenu de @ref TopologyRepository.
     *
     * Parcourt toutes les coordonnées des StraightBlocks (polyligne complète)
     * et des SwitchBlocks (jonction + trois tips optionnels) pour déterminer
     * les bornes GPS min/max.
     *
     * @param rect   Zone cliente fournie par @ref draw.
     * @param logger Référence au logger HMI pour tracer les anomalies.
     * @return       Projection initialisée, prête à l'emploi.
     *               Retourne une Projection par défaut si le repository est vide.
     */
    static Projection computeProjection(const RECT& rect, Logger& logger);

    /**
     * @brief Projette un point GPS en coordonnées pixels.
     *
     * Applique une normalisation linéaire avec marge sur [0, width] × [0, height].
     * L'axe Y est inversé : latitude maximale → Y = 0.
     *
     * @param lat  Latitude WGS-84 en degrés décimaux.
     * @param lon  Longitude WGS-84 en degrés décimaux.
     * @param proj Projection précalculée par @ref computeProjection.
     * @return     @c POINT en coordonnées pixels dans la zone cliente.
     */
    static POINT project(double lat, double lon, const Projection& proj);

    /**
     * @brief Dessine tous les StraightBlocks sous forme de polyligne GDI.
     *
     * Itère sur @ref TopologyData::straights. La couleur du trait est déterminée
     * par @ref stateToColor appliqué à @c StraightBlock::getState().
     * Les straights dont la polyligne contient moins de 2 points sont ignorés.
     *
     * @param hdc    Contexte de périphérique GDI.
     * @param proj   Projection GPS → pixels courante.
     * @param logger Référence au logger HMI.
     */
    static void drawStraights(HDC hdc, const Projection& proj, Logger& logger);

    /**
     * @brief Dessine tous les SwitchBlocks (jonction + 3 branches).
     *
     * Pour chaque switch orienté (@c isOriented() == true), dessine :
     *  - tip root      → jonction (couleur état switch)
     *  - jonction      → tip normal    (vert si branche active, sinon couleur état)
     *  - jonction      → tip déviation (jaune si branche active, sinon couleur état)
     *  - disque de 4 px à la jonction
     *
     * Les switches non orientés (parsing incomplet) sont ignorés silencieusement.
     *
     * @param hdc    Contexte de périphérique GDI.
     * @param proj   Projection GPS → pixels courante.
     * @param logger Référence au logger HMI.
     */
    static void drawSwitches(HDC hdc, const Projection& proj, Logger& logger);

    /**
     * @brief Mappe un @ref ShuntingState vers une couleur GDI.
     *
     *  - FREE     → @c RGB(220, 220, 220) blanc cassé
     *  - OCCUPIED → @c RGB(220,  50,  50) rouge
     *  - INACTIVE → @c RGB( 80,  80,  80) gris foncé
     *
     * @param state État opérationnel de l'élément.
     * @return      Couleur @c COLORREF correspondante.
     */
    static COLORREF stateToColor(ShuntingState state);

    /**
     * @brief Dessine un segment GDI entre deux points écran.
     *
     * Crée un @c HPEN de largeur @p lineWidth, trace le segment, puis
     * restaure et supprime le pen. Le pen est désélectionné avant suppression
     * pour éviter une fuite de ressource GDI.
     *
     * @param hdc       Contexte de périphérique GDI.
     * @param from      Point de départ en pixels.
     * @param to        Point d'arrivée en pixels.
     * @param color     Couleur @c COLORREF du trait.
     * @param lineWidth Épaisseur du trait en pixels (défaut : 2).
     */
    static void drawLine(HDC hdc, POINT from, POINT to, COLORREF color, int lineWidth = 2);

    /**
     * @brief Dessine un disque plein centré sur @p center.
     *
     * Utilisé pour marquer visuellement les jonctions de SwitchBlocks.
     * Crée et supprime un brush et un pen temporaires.
     *
     * @param hdc    Contexte de périphérique GDI.
     * @param center Centre du disque en pixels.
     * @param radius Rayon en pixels.
     * @param color  Couleur de remplissage @c COLORREF.
     */
    static void drawDot(HDC hdc, POINT center, int radius, COLORREF color);

    /** @brief Instanciation interdite — classe utilitaire statique. */
    TCORenderer() = delete;
};