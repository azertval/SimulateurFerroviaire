//{{NO_DEPENDENCIES}}
// fichier Include Microsoft Visual C++.
// Utilise par SimulateurFerroviaire.rc
//
#define IDC_MYICON                      2
#define IDD_SIMULATEURFERROVIAIRE_DIALOG 102
#define IDD_ABOUTBOX                    103
#define IDM_ABOUT                       104
#define IDM_EXIT                        105
#define IDS_APP_TITLE                   106
#define IDI_SIMULATEURFERROVIAIRE       107
#define IDI_SMALL                       108
#define IDC_SIMULATEURFERROVIAIRE       109
#define IDR_MAINFRAME                   128
#define IDM_FILE_OPEN                   32771
#define IDM_FILE_EXPORT                 32772
#define IDM_VIEW_PCC                    32773
#define IDM_PARSER_SETTINGS				32774
#define IDC_EDIT_SNAP_TOLERANCE			32775
#define IDC_EDIT_MAX_SEGMENT_LENGTH		32776
#define IDC_EDIT_MIN_SWITCH_ANGLE		32777
#define IDC_EDIT_JUNCTION_TRIM_MARGIN	32778
#define IDC_EDIT_INTERSECTION_EPSILON	32779
#define IDC_EDIT_DOUBLE_SWITCH_RADIUS	32780
#define IDC_EDIT_MIN_BRANCH_LENGTH		32781
#define IDC_BTN_RESET					32782
#define IDC_CANCEL_PARSING				32783
#define IDD_PARSER_SETTINGS             32784

#define IDC_STATIC                      -1

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NO_MFC                     1
#define _APS_NEXT_RESOURCE_VALUE        129
#define _APS_NEXT_COMMAND_VALUE         32774
#define _APS_NEXT_CONTROL_VALUE         1000
#define _APS_NEXT_SYMED_VALUE           110
#endif
#endif

// =============================================================================
// Messages inter-threads (GeoParsingTask → MainWindow)
// =============================================================================

/**
* Avancement du parsing.
* wParam = progression (int, 0-100).
* lParam = std::wstring* label de phase — alloué par GeoParsingTask,
*          libéré par MainWindow::onProgressUpdate.
*/
#define WM_PROGRESS_UPDATE   (WM_USER + 1)

/** Parsing terminé avec succès. wParam = lParam = 0. */
#define WM_PARSING_SUCCESS   (WM_USER + 2)

/**
* Parsing échoué.
* lParam = std::wstring* message d'erreur — alloué par GeoParsingTask,
*          libéré par MainWindow::onParsingError.
*/
#define WM_PARSING_ERROR     (WM_USER + 3)

/** Parsing annulé par l'utilisateur. wParam = lParam = 0. */
#define WM_PARSING_CANCELLED (WM_USER + 4)
