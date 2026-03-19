/**
 * @file  Logger.cpp
 * @brief Implémentation du Logger à 5 niveaux de trace.
 */

#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <filesystem>

// ============================================================================
// Fonctions utilitaires globales
// ============================================================================
 /**
  * @brief Get executable directory.
  *
  * @return Path of the folder containing the .exe
  */
std::filesystem::path getExecutableDirectory()
{
    char buffer[MAX_PATH];

    // Retrieve full path of the executable
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    // Convert to filesystem path
    std::filesystem::path exePath(buffer);

    // Return directory only
    return exePath.parent_path();
}


// =============================================================================
// Construction / destruction
// =============================================================================

Logger::Logger(const std::string& motorName)
    : m_motorName(motorName)
{
#ifdef NDEBUG
    // Release build → moins verbeux
    m_minimumLevel = LogLevel::INFO;
#else
    // Debug build → très verbeux
    m_minimumLevel = LogLevel::DEBUG;
#endif

    // Création du répertoire Logs/ si absent
    std::filesystem::path logsDirectory = getExecutableDirectory() / "Logs";
    if (!std::filesystem::exists(logsDirectory))
    {
        std::filesystem::create_directory(logsDirectory);
    }

    // Ouverture du fichier de log en mode ajout
    std::filesystem::path logFilePath = logsDirectory / (motorName + ".log");
    m_outputFile.open(logFilePath, std::ios::out | std::ios::app);

    if (!m_outputFile.is_open())
    {
        // Fallback : écriture sur la sortie de débogage si le fichier est inaccessible
        sendToDebugOutput("[Logger] ERREUR : impossible d'ouvrir le fichier : "
                          + logFilePath.string() + "\n");
    }
    else
    {
        // Ligne de séparation de session
        const std::string separator(70, '=');
        m_outputFile << "\n" << separator << "\n";
        m_outputFile << "  Session démarrée — moteur : " << motorName
                     << "  " << getCurrentTimestamp() << "\n";
        m_outputFile << separator << "\n";
        m_outputFile.flush();
    }
}

Logger::~Logger()
{
    if (m_outputFile.is_open())
    {
        m_outputFile.close();
    }
}


// =============================================================================
// API publique
// =============================================================================

void Logger::writeLog(LogLevel           level,
                      const char*        functionSignature,
                      int                lineNumber,
                      const std::string& message)
{
    // Filtrage par niveau minimum
    if (level < m_minimumLevel)
    {
        return;
    }

    const std::string signature(functionSignature ? functionSignature : "");
    const std::string className    = extractClassName(signature);
    const std::string functionName = extractFunctionName(signature);

    const std::string logLine = formatLogLine(level, className, functionName,
                                               lineNumber, message);

    std::lock_guard<std::mutex> lock(m_writeMutex);

    // Écriture dans le fichier
    if (m_outputFile.is_open())
    {
        m_outputFile << logLine << "\n";
        m_outputFile.flush();
    }

    // Envoi vers la sortie de débogage Visual Studio
    sendToDebugOutput(logLine + "\n");
}

void Logger::setMinimumLogLevel(LogLevel minimumLevel)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    m_minimumLevel = minimumLevel;
}

const std::string& Logger::getMotorName() const
{
    return m_motorName;
}


// =============================================================================
// Gestion des erreurs fatales
// =============================================================================

[[noreturn]] void Logger::triggerFatalCrash(const std::string& errorMessage)
{
#ifdef _WIN32
    // Affichage d'une boîte de dialogue bloquante sur les applications Win32
    const std::wstring wideMessage(errorMessage.begin(), errorMessage.end());
    const std::wstring title(L"Erreur fatale — SimulateurFerroviaire");
    MessageBoxW(nullptr, wideMessage.c_str(), title.c_str(),
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif

    // Terminaison immédiate de l'application
    std::terminate();
}


// =============================================================================
// Méthodes utilitaires statiques (privées)
// =============================================================================

std::string Logger::extractClassName(const std::string& functionSignature)
{
    // Sur MSVC : "NomDeClasse::NomDeFonction"
    // On recherche le dernier "::" et on retourne tout ce qui le précède.
    // Pour "Namespace::NomDeClasse::NomDeFonction", retourne "NomDeClasse"
    // (avant-dernier segment).

    const std::string delimiter("::");
    std::size_t lastPosition = functionSignature.rfind(delimiter);

    if (lastPosition == std::string::npos)
    {
        // Pas de "::" trouvé — pas de classe identifiable
        return "";
    }

    // Chercher le "::" précédent pour isoler uniquement le nom de classe
    const std::string beforeLastDelimiter = functionSignature.substr(0, lastPosition);
    std::size_t secondToLastPosition = beforeLastDelimiter.rfind(delimiter);

    if (secondToLastPosition == std::string::npos)
    {
        // Seul un "::" → tout ce qui précède est le nom de classe
        return beforeLastDelimiter;
    }

    // Plusieurs "::" → prendre le dernier segment avant le "::" final
    return beforeLastDelimiter.substr(secondToLastPosition + delimiter.size());
}

std::string Logger::extractFunctionName(const std::string& functionSignature)
{
    // Retourne tout ce qui suit le dernier "::"
    const std::string delimiter("::");
    std::size_t lastPosition = functionSignature.rfind(delimiter);

    if (lastPosition == std::string::npos)
    {
        return functionSignature;
    }

    return functionSignature.substr(lastPosition + delimiter.size());
}

std::string Logger::formatLogLine(LogLevel           level,
                                   const std::string& className,
                                   const std::string& functionName,
                                   int                lineNumber,
                                   const std::string& message)
{
    // Format : [HH:MM:SS] [NIVEAU  ] {NomDeClasse} [NomDeFonction] "Ligne : XX" : Message
    std::ostringstream stream;
    stream << getCurrentTimestamp()
           << " [" << levelToString(level) << "]";

    if (!className.empty())
    {
        stream << " {" << className << "}";
    }

    stream << " [" << functionName << "]"
           << " \"Ligne : " << lineNumber << "\""
           << " : " << message;

    return stream.str();
}

std::string Logger::levelToString(LogLevel level)
{
    // Largeur fixe de 7 caractères pour un alignement visuel dans le fichier log
    switch (level)
    {
    case LogLevel::INFO:    return "INFO";
    case LogLevel::DEBUG:   return "DEBUG";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::ERR:   return "ERROR";
    case LogLevel::FAILURE: return "FAILURE";
    default:                return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm     localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    char buffer[12];
    std::strftime(buffer, sizeof(buffer), "[%H:%M:%S]", &localTime);
    return std::string(buffer);
}

void Logger::sendToDebugOutput(const std::string& text)
{
#ifdef _WIN32
    OutputDebugStringA(text.c_str());
#endif
}
