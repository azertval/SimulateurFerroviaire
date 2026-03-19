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

#include "./Models/StraightBlock.h"
#include "./Models/SwitchBlock.h"
#include "./Enums/GeoParserEnums.h"
#include "./../../Engine/Core/Logger.h"

class GeoParser
{
public:

    /** Résultat final : voies droites */
    std::vector<StraightBlock> straights;

    /** Résultat final : aiguillages */
    std::vector<SwitchBlock> switches;

    /**
     * @brief Constructeur.
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
     * @brief Dump debug.
     */
    void dumpDebugOutput() const;

    /**
     * @brief Définit le callback de progression.
     */
    void setProgressCallback(std::function<void(int)> callback)
    {
        m_progressCallback = callback;
    }

private:

    Logger& m_logger;
    std::string m_geoJsonFilePath;

    double m_snapGridMeters;
    double m_endpointSnapMeters;
    double m_maxStraightLengthMeters;
    double m_minBranchLengthMeters;
    double m_doubleLinkMaxMeters;
    double m_branchTipDistanceMeters;

    std::function<void(int)> m_progressCallback;

    /**
     * @brief Notifie la progression.
     */
    void reportProgress(int value) const
    {
        if (m_progressCallback)
            m_progressCallback(value);
    }
};