/**
 * @file Application.h
 * @brief Gestion du cycle de vie de l'application Win32.
 *
 * La classe @ref Application encapsule les trois étapes fondamentales du
 * démarrage d'une application Win32 :
 *  -# Enregistrement de la classe de fenêtre (@c RegisterClassExW).
 *  -# Création et affichage de la fenêtre principale (@ref MainWindow).
 *  -# Exécution de la boucle de messages (@c GetMessage / @c DispatchMessage).
 *
 * @note Une seule instance de cette classe doit exister par processus.
 */

#pragma once

#include "framework.h"
#include "Engine/HMI/MainWindow.h"

#include <memory>

#define MAX_LOADSTRING 100


/**
 * @class Application
 * @brief Représente le cycle de vie complet de l'application Win32.
 *
 * Responsabilités :
 *  - Enregistrer la classe de fenêtre Win32 via @c MyRegisterClass.
 *  - Créer la fenêtre principale via @ref MainWindow.
 *  - Exécuter la boucle de messages et retourner le code de sortie.
 *
 * Usage typique :
 * @code
 *   Application app(hInstance, nCmdShow);
 *   return app.run();
 * @endcode
 */
class Application
{
public:

    /**
     * @brief Construit l'application et enregistre la classe de fenêtre.
     *
     * @param hInstance Handle de l'instance Win32 du processus courant.
     * @param nCmdShow  Mode d'affichage initial de la fenêtre principale.
     *
     * @throws std::runtime_error Si l'enregistrement de la classe échoue.
     */
    Application(HINSTANCE hInstance, int nCmdShow);

    /**
     * @brief Initialise la fenêtre principale et démarre la boucle de messages.
     *
     * Cette méthode est bloquante : elle ne retourne qu'à la fermeture de
     * la fenêtre principale ou à la réception de @c WM_QUIT.
     *
     * @return Code de sortie du programme (issu de @c PostQuitMessage).
     *
     * @throws std::runtime_error Si la création de la fenêtre principale échoue.
     */
    int run();

private:

    /**
     * @brief Enregistre la classe de fenêtre Win32 (@c WNDCLASSEXW).
     *
     * Définit le curseur, l'icône, la procédure de fenêtre et les autres
     * attributs de la classe. Doit être appelé avant toute création de fenêtre.
     *
     * @throws std::runtime_error Si @c RegisterClassExW retourne 0.
     */
    void registerWindowClass();

    // -------------------------------------------------------------------------
    // Membres
    // -------------------------------------------------------------------------

    /** Handle de l'instance Win32 du processus. */
    HINSTANCE m_hInstance;

    /** Mode d'affichage passé à @c ShowWindow lors de la création. */
    int m_nCmdShow;

    /** Texte affiché dans la barre de titre. */
    WCHAR m_szTitle[MAX_LOADSTRING];

    /** Nom identifiant la classe de fenêtre Win32. */
    WCHAR m_szWindowClass[MAX_LOADSTRING];

    /** Fenêtre principale de l'application. */
    std::unique_ptr<MainWindow> m_mainWindow;
};
