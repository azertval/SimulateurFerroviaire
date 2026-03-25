#pragma once

/**
 * @file  CoordinateXY.h
 * @brief Représentation d'une coordonnée plane en mètres (système métrique UTM).
 *
 * Utilisé en interne par le pipeline pour tous les calculs métriques
 * (construction du graphe, accrochage sur grille, interpolation, angles).
 * Les coordonnées publiques (sorties du parseur) sont toujours en CoordinateLatLon WGS-84.
 *
 * Exemple :
 * @code
 *   CoordinateXY metricPoint{ 652345.0, 5862234.0 };  // UTM zone 31N
 *   double x = metricPoint.x;
 *   double y = metricPoint.y;
 * @endcode
 */

/**
 * @brief Coordonnée plane en mètres dans un système métrique (UTM ou similaire).
 */
class CoordinateXY
{
public:
    double x = 0.0;  ///< Abscisse en mètres (est pour UTM).
    double y = 0.0;  ///< Ordonnée en mètres (nord pour UTM).

    /** Constructeur par défaut — origine (0, 0). */
    CoordinateXY() = default;

    /**
     * @brief Construit une coordonnée avec les valeurs fournies.
     * @param xValue  Abscisse en mètres.
     * @param yValue  Ordonnée en mètres.
     */
    CoordinateXY(double xValue, double yValue)
        : x(xValue), y(yValue)
    {}

    bool operator==(const CoordinateXY& other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator!=(const CoordinateXY& other) const
    {
        return !(*this == other);
    }

    std::string toString() const
    {
        return "[" + std::to_string(x) + " ; " + std::to_string(y) + "]";
    }
};
