/**
 * @file  ParserConfigIni.h
 * @brief Persistence de @ref ParserConfig via fichier .ini (SimpleIni).
 *
 * Classe utilitaire statique. Seule classe du projet qui connaît le
 * format .ini et la bibliothèque SimpleIni.
 *
 * @par Chemin par défaut
 * Dossier de l'exécutable — mode portable, pas de dépendance AppData.
 *
 * @note Instanciation interdite — classe statique.
 */
#pragma once

#include "ParserConfig.h"
#include <string>

class ParserConfigIni
{
public:

    /**
     * @brief Charge la configuration depuis un fichier .ini.
     *
     * Si le fichier est absent ou si une clé est manquante, la valeur
     * par défaut de @ref ParserConfig est utilisée.
     * Ne lève pas d'exception — retourne toujours une config valide.
     *
     * @param path  Chemin complet vers le fichier .ini.
     *
     * @return Configuration chargée, éventuellement partielle (défauts complétés).
     */
    static ParserConfig load(const std::string& path);

    /**
     * @brief Sauvegarde la configuration dans un fichier .ini.
     *
     * Crée le fichier s'il n'existe pas. Écrase les valeurs existantes.
     *
     * @param path    Chemin complet vers le fichier .ini.
     * @param config  Configuration à sauvegarder.
     *
     * @throws std::runtime_error Si l'écriture échoue.
     */
    static void save(const std::string& path, const ParserConfig& config);

    /**
     * @brief Retourne le chemin par défaut du fichier .ini.
     *
     * Construit le chemin relatif à l'exécutable courant.
     * Format : `<dossier_exe>/Config/parser_settings.ini`
     *
     * @return Chemin absolu vers le fichier .ini.
     */
    static std::string defaultPath();

    ParserConfigIni() = delete;

private:

    /**
     * @brief Lit un double dans une section/clé avec valeur par défaut.
     *
     * @param ini         Instance SimpleIni déjà chargée.
     * @param section     Nom de la section .ini.
     * @param key         Nom de la clé.
     * @param defaultVal  Valeur retournée si la clé est absente ou invalide.
     *
     * @return Valeur lue ou @p defaultVal.
     */
    static double getDouble(const void* ini,   // CSimpleIniA — opaque dans le .h
        const char* section,
        const char* key,
        double defaultVal);
};