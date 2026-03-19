/**
 * @file  TopologyEdge.cpp
 * @brief Implémentation de l'arête du graphe planaire.
 */

#include "TopologyEdge.h"

#include <cmath>
#include <iomanip>


TopologyEdge::TopologyEdge(std::string               edgeId,
                             int                       startIndex,
                             int                       endIndex,
                             std::vector<CoordinateXY> edgeGeometry)
    : id(std::move(edgeId))
    , startNodeIndex(startIndex)
    , endNodeIndex(endIndex)
    , geometry(std::move(edgeGeometry))
    , lengthMeters(0.0)
{
    lengthMeters = computePlanarLength();
}

void TopologyEdge::recomputeLength()
{
    lengthMeters = computePlanarLength();
}

std::string TopologyEdge::toString() const
{
    std::ostringstream stream;
    stream << "TopologyEdge(id=" << id
           << ", " << startNodeIndex << "<->" << endNodeIndex
           << std::fixed;
    stream.precision(1);
    stream << ", len=" << lengthMeters << "m)";
    return stream.str();
}

double TopologyEdge::computePlanarLength() const
{
    if (geometry.size() < 2)
    {
        return 0.0;
    }

    double totalLength = 0.0;
    for (std::size_t index = 1; index < geometry.size(); ++index)
    {
        const double deltaX = geometry[index].x - geometry[index - 1].x;
        const double deltaY = geometry[index].y - geometry[index - 1].y;
        totalLength += std::hypot(deltaX, deltaY);
    }
    return totalLength;
}
