#pragma once

/**
 * @file GeoParser.h
 * @brief Orchestrateur principal du pipeline GeoJSON ferroviaire.
 *
 * Pipeline exécuté :
 *   Phase 1-2 : GraphBuilder
 *   Phase 3-5 : TopologyExtractor
 *   Phase 6   : SwitchOrientator
 *   Phase 7-8 : DoubleSwitchDetector
 *
 * Fournit également :
 *   - résultats (switches / straights)
 *   - callback de progression UI
 */

#include <string>
#include <vector>
#include <functional>

#include "Modules/Models/StraightBlock.h"
#include "Modules/Models/SwitchBlock.h"
#include "./Enums/GeoParserEnums.h"
#include "Engine/Core/Logger/Logger.h"

class GeoParser
{
public:
    /**
     * @brief Construit le parseur avec ses paramètres de pipeline.
     *
     * @param logger                   Logger associé au moteur GeoParser.
     * @param geoJsonFilePath          Chemin du fichier GeoJSON source.
     * @param snapGridMeters           Pas de la grille d'accrochage (mètres).
     * @param endpointSnapMeters       Tolérance de fusion des extrémités (mètres).
     * @param maxStraightLengthMeters  Longueur maximale d'un Straight avant découpe (mètres).
     * @param minBranchLengthMeters    Longueur minimale d'une branche d'aiguillage (mètres).
     * @param doubleLinkMaxMeters      Distance maximale du lien interne d'un double aiguille (mètres).
     * @param branchTipDistanceMeters  Distance d'interpolation des points tip CDC (mètres).
     */
    GeoParser(Logger& logger,
        const std::string& geoJsonFilePath,
        double snapGridMeters,
        double endpointSnapMeters,
        double maxStraightLengthMeters,
        double minBranchLengthMeters,
        double doubleLinkMaxMeters,
        double branchTipDistanceMeters);

    /**
     * @brief Lance le pipeline complet.
     */
    void parse(bool enableDebugDump = ParserDefaultValues::ENABLE_FULL_DEBUG_MODE);

    /**
     * @brief Dump détaillé des résultats dans les traces DEBUG.
     */
    void dumpDebugOutput() const;

    /**
     * @brief Enregistre le callback de progression UI.
     *
     * @param callback  Fonction appelée avec une valeur [0–100] à chaque étape.
     */
    void setProgressCallback(std::function<void(int)> callback)
    {
        m_progressCallback = callback;
    }

private:
    // Membres privés :
    Logger& m_logger;                    ///< Logger du moteur GeoParser.
    std::string m_geoJsonFilePath;           ///< Chemin du fichier GeoJSON source.
    double      m_snapGridMeters;            ///< Pas de la grille d'accrochage (mètres).
    double      m_endpointSnapMeters;        ///< Tolérance de fusion des extrémités (mètres).
    double      m_maxStraightLengthMeters;   ///< Longueur maximale avant découpe (mètres).
    double      m_minBranchLengthMeters;     ///< Longueur minimale de branche (mètres).
    double      m_doubleLinkMaxMeters;       ///< Distance max du lien double aiguille (mètres).
    double      m_branchTipDistanceMeters;   ///< Distance d'interpolation des tips CDC (mètres).
    std::function<void(int)> m_progressCallback; ///< Callback de progression UI (optionnel).

    /**
     * @brief Notifie la progression via le callback UI si défini.
     * @param value  Valeur de progression (0–100).
     */
    void reportProgress(int value) const
    {
        if (m_progressCallback)
            m_progressCallback(value);
    }
};