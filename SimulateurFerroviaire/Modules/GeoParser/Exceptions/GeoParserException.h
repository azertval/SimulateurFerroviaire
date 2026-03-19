#pragma once

/**
 * @file  GeoParserException.h
 * @brief Hiérarchie d'exceptions du pipeline GeoJSON ferroviaire.
 *
 * Hiérarchie :
 *   std::runtime_error
 *   └── RailwayParserException              Base de toutes les erreurs du parseur
 *       ├── InvalidGeoJsonFormatException   Fichier GeoJSON malformé ou vide
 *       ├── InvalidTopologyException        Topologie du graphe incohérente
 *       └── InvalidSwitchConfigException    Aiguillage non-orientable / hors contraintes CDC
 *
 * Usage recommandé :
 * @code
 *   throw InvalidGeoJsonFormatException("Fichier sans champ 'features'");
 *
 *   try { parser.parse(); }
 *   catch (const RailwayParserException& exception)
 *   {
 *       LOG_ERROR(logger, exception.what());
 *   }
 * @endcode
 */

#include <stdexcept>
#include <string>


// =============================================================================
// Classe de base
// =============================================================================

/**
 * @brief Classe de base pour toutes les erreurs du parseur ferroviaire.
 *
 * Attraper cette classe intercepte n'importe quelle erreur du pipeline.
 * Pour une gestion plus fine, attraper les sous-classes spécialisées.
 */
class RailwayParserException : public std::runtime_error
{
public:
    /**
     * @brief Construit une exception avec le message fourni.
     * @param errorMessage  Description de l'erreur.
     */
    explicit RailwayParserException(const std::string& errorMessage)
        : std::runtime_error(errorMessage)
    {}
};


// =============================================================================
// Sous-classes spécialisées
// =============================================================================

/**
 * @brief Levée quand le fichier GeoJSON est malformé ou ne contient
 *        aucune géométrie LineString exploitable.
 *
 * Exemples déclencheurs :
 *   - Fichier sans champ "features" ou sans champ "geometry".
 *   - Fichier ne contenant aucun LineString après normalisation.
 *   - Erreur de lecture (format inconnu, encodage invalide, fichier absent).
 */
class InvalidGeoJsonFormatException final : public RailwayParserException
{
public:
    explicit InvalidGeoJsonFormatException(const std::string& errorMessage)
        : RailwayParserException(errorMessage)
    {}
};


/**
 * @brief Levée quand la topologie du graphe parsé est structurellement incohérente.
 *
 * Exemples déclencheurs :
 *   - Aucune arête construite depuis le GeoJSON (réseau vide après normalisation).
 *   - Pointeur de parseur nul passé à une fonction de conversion.
 */
class InvalidTopologyException final : public RailwayParserException
{
public:
    explicit InvalidTopologyException(const std::string& errorMessage)
        : RailwayParserException(errorMessage)
    {}
};


/**
 * @brief Levée quand un aiguillage viole une contrainte géométrique du CDC.
 *
 * Exemples déclencheurs :
 *   - Branche d'aiguillage inférieure à min_branch_length_m.
 *   - Aiguillage de degré différent de 3 impossible à orienter.
 *   - Tip CDC impossible à calculer (branche dégénérée).
 */
class InvalidSwitchConfigException final : public RailwayParserException
{
public:
    explicit InvalidSwitchConfigException(const std::string& errorMessage)
        : RailwayParserException(errorMessage)
    {}
};
