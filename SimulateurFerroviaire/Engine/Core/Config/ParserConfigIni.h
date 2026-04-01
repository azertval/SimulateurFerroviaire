/**
 * @file  ParserConfigIni.h
 * @brief Persistence de @ref ParserConfig via fichier .ini (SimpleIni).
 *
 * Classe utilitaire statique. Seule classe du projet qui connaît le
 * format .ini et la bibliothèque SimpleIni.
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
     *
     * @param path  Chemin complet vers le fichier .ini.
     *
     * @return Configuration chargée, valeurs par défaut complétées.
     */
    static ParserConfig load(const std::string& path);

    /**
     * @brief Sauvegarde la configuration dans un fichier .ini.
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
     * Format : `<dossier_exe>/Config/parser_settings.ini`
     *
     * @return Chemin absolu vers le fichier .ini.
     */
    static std::string defaultPath();

    ParserConfigIni() = delete;

private:

    static double getDouble(const void* ini,
        const char* section,
        const char* key,
        double defaultVal);
};