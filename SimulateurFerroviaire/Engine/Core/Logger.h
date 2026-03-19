#pragma once

/**
 * @file   Logger.h
 * @brief  Système de journalisation à 5 niveaux de trace, un fichier par moteur.
 *
 * Chaque instance de Logger est associée à un moteur nommé (ex. "GeoParser").
 * Les traces sont écrites dans "Logs/<NomDuMoteur>.log" et sur la sortie de
 * débogage Visual Studio.
 *
 * Niveaux disponibles (ordre de sévérité croissant) :
 *   INFO    — Événements nominaux (phases terminées, résultats attendus).
 *   DEBUG   — État interne, valeurs intermédiaires, traces fines.
 *   WARNING — Anomalie non-bloquante (données douteuses, repli sur valeur par défaut).
 *   ERROR   — Erreur bloquante récupérable (fichier absent, topologie invalide).
 *   FAILURE — Erreur fatale irrecupérable — journalise et plante l'application.
 *
 * Format de chaque ligne de trace :
 *   [HH:MM:SS] [NIVEAU] {NomDeClasse} [NomDeFonction] "Ligne : XX" : Message
 *
 * Utilisation via macros (injection automatique du contexte) :
 * @code
 *   Logger logger("GeoParser");
 *   LOG_INFO(logger,    "Fichier chargé : " + filePath);
 *   LOG_DEBUG(logger,   "Nœud créé à l'index " + std::to_string(nodeIndex));
 *   LOG_WARNING(logger, "Branche trop courte : " + branchId);
 *   LOG_ERROR(logger,   "Fichier introuvable : " + filePath);
 *   LOG_FAILURE(logger, "Topologie nulle — arrêt immédiat");
 * @endcode
 */

#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


// =============================================================================
// Niveau de trace
// =============================================================================

/**
 * @brief Niveaux de sévérité des traces, du moins critique au plus critique.
 */
enum class LogLevel : int
{    
    DEBUG   = 0,  ///< Trace de débogage interne.
    INFO = 1,  ///< Événement nominal.
    WARNING = 2,  ///< Anomalie non-bloquante.
    ERR   = 3,  ///< Erreur bloquante récupérable.
    FAILURE = 4,  ///< Erreur fatale — plante l'application après journalisation.
};


// =============================================================================
// Macros d'injection automatique du contexte (classe, fonction, ligne)
// =============================================================================

/**
 * Sur MSVC, __FUNCTION__ retourne "NomDeClasse::NomDeFonction".
 * Les macros transmettent __FUNCTION__ et __LINE__ au Logger,
 * qui en extrait le nom de classe et le nom de fonction.
 */
#define LOG_INFO(logger, message) \
    (logger).writeLog(LogLevel::INFO,    __FUNCTION__, __LINE__, (message))

#define LOG_DEBUG(logger, message) \
    (logger).writeLog(LogLevel::DEBUG,   __FUNCTION__, __LINE__, (message))

#define LOG_WARNING(logger, message) \
    (logger).writeLog(LogLevel::WARNING, __FUNCTION__, __LINE__, (message))

#define LOG_ERROR(logger, message) \
    (logger).writeLog(LogLevel::ERR,   __FUNCTION__, __LINE__, (message))

/** LOG_FAILURE journalise et appelle Logger::triggerFatalCrash() — ne retourne jamais. */
#define LOG_FAILURE(logger, message) \
    do { \
        (logger).writeLog(LogLevel::FAILURE, __FUNCTION__, __LINE__, (message)); \
        Logger::triggerFatalCrash((message)); \
    } while (false)


// =============================================================================
// Classe Logger
// =============================================================================

/**
 * @brief Journaliseur associé à un moteur nommé.
 *
 * Chaque instance ouvre (ou crée) un fichier "Logs/<nomDuMoteur>.log".
 * Les écritures sont protégées par un mutex pour la compatibilité multi-thread.
 * Le niveau minimum de filtre est configurable par setMinimumLogLevel().
 */
class Logger
{
public:

    // -------------------------------------------------------------------------
    // Construction / destruction
    // -------------------------------------------------------------------------

    /**
     * @brief Construit un Logger pour le moteur spécifié.
     *
     * Crée le répertoire "Logs/" si absent, puis ouvre (mode append)
     * le fichier "Logs/<motorName>.log".
     *
     * @param motorName  Nom du moteur (ex. "GeoParser", "Simulation").
     *                   Utilisé comme nom de fichier de log.
     */
    explicit Logger(const std::string& motorName);

    /** Destructeur — ferme proprement le fichier de log. */
    ~Logger();

    // Interdit la copie (un moteur = un fichier = une instance)
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    // -------------------------------------------------------------------------
    // API publique
    // -------------------------------------------------------------------------

    /**
     * @brief Écrit une ligne de trace dans le fichier et la sortie de débogage.
     *
     * Méthode appelée directement par les macros LOG_*. Ne pas appeler
     * manuellement — préférer les macros pour bénéficier du contexte automatique.
     *
     * @param level              Niveau de sévérité de la trace.
     * @param functionSignature  Signature de la fonction appelante (__FUNCTION__).
     * @param lineNumber         Numéro de ligne source (__LINE__).
     * @param message            Texte du message à journaliser.
     */
    void writeLog(LogLevel           level,
                  const char*        functionSignature,
                  int                lineNumber,
                  const std::string& message);

    /**
     * @brief Définit le niveau minimum en dessous duquel les traces sont ignorées.
     *
     * Par défaut : LogLevel::DEBUG (toutes les traces sont écrites).
     *
     * @param minimumLevel  Niveau minimum (inclus) à journaliser.
     */
    void setMinimumLogLevel(LogLevel minimumLevel);

    /**
     * @brief Retourne le nom du moteur associé à cette instance.
     * @return Nom du moteur passé au constructeur.
     */
    const std::string& getMotorName() const;

    // -------------------------------------------------------------------------
    // Gestion des erreurs fatales
    // -------------------------------------------------------------------------

    /**
     * @brief Provoque l'arrêt immédiat de l'application après affichage d'une boîte de dialogue.
     *
     * Appelé automatiquement par la macro LOG_FAILURE. Ne retourne jamais.
     *
     * @param errorMessage  Message d'erreur affiché dans la boîte de dialogue.
     */
    [[noreturn]] static void triggerFatalCrash(const std::string& errorMessage);

private:

    // -------------------------------------------------------------------------
    // Membres privés
    // -------------------------------------------------------------------------

    std::string   m_motorName;       ///< Nom du moteur (identifiant du fichier log).
    std::ofstream m_outputFile;      ///< Flux d'écriture vers le fichier de log.
    std::mutex    m_writeMutex;      ///< Protection mutex pour les accès concurrents.
    LogLevel      m_minimumLevel;    ///< Niveau minimum de filtrage.

    // -------------------------------------------------------------------------
    // Méthodes utilitaires statiques (privées)
    // -------------------------------------------------------------------------

    /**
     * @brief Extrait le nom de classe depuis la signature MSVC "__FUNCTION__".
     *
     * Sur MSVC, "__FUNCTION__" retourne "NomDeClasse::NomDeFonction".
     * Cette méthode retourne "NomDeClasse", ou une chaîne vide si aucun "::" n'est trouvé.
     *
     * @param functionSignature  Valeur de __FUNCTION__ fournie par le préprocesseur.
     * @return Nom de la classe extraite, ou "" si non disponible.
     */
    static std::string extractClassName(const std::string& functionSignature);

    /**
     * @brief Extrait le nom de la fonction depuis la signature MSVC "__FUNCTION__".
     *
     * Retourne la partie après le dernier "::". Si aucun "::" n'est présent,
     * retourne la chaîne entière.
     *
     * @param functionSignature  Valeur de __FUNCTION__ fournie par le préprocesseur.
     * @return Nom de la fonction extraite.
     */
    static std::string extractFunctionName(const std::string& functionSignature);

    /**
     * @brief Formate une ligne de trace complète.
     *
     * Format : [HH:MM:SS] [NIVEAU] {NomDeClasse} [NomDeFonction] "Ligne : XX" : Message
     *
     * @param level         Niveau de sévérité.
     * @param className     Nom de la classe (extrait de __FUNCTION__).
     * @param functionName  Nom de la fonction (extrait de __FUNCTION__).
     * @param lineNumber    Numéro de ligne source.
     * @param message       Message utilisateur.
     * @return Chaîne formatée prête à écrire.
     */
    static std::string formatLogLine(LogLevel           level,
                                     const std::string& className,
                                     const std::string& functionName,
                                     int                lineNumber,
                                     const std::string& message);

    /**
     * @brief Convertit un LogLevel en chaîne courte pour l'affichage.
     * @param level  Niveau de trace.
     * @return Chaîne de 7 caractères max (ex. "INFO   ", "WARNING").
     */
    static std::string levelToString(LogLevel level);

    /**
     * @brief Retourne l'horodatage courant au format [HH:MM:SS].
     * @return Chaîne horodatage entre crochets.
     */
    static std::string getCurrentTimestamp();

    /**
     * @brief Envoie une chaîne vers la sortie de débogage Visual Studio.
     *
     * Utilise OutputDebugStringA sur Windows. Sans effet sur les autres plateformes.
     *
     * @param text  Texte à envoyer à la sortie de débogage.
     */
    static void sendToDebugOutput(const std::string& text);
};
