/**
 * @file  SwitchCrossBlock.cpp
 * @brief Implémentation du croisement TJD (Traversée Jonction Double).
 *
 * @see SwitchCrossBlock
 */
#include "SwitchCrossBlock.h"

namespace
{
    /**
     * @brief Test générique d'activation d'une voie entre deux corners.
     *
     * Vérifie que les deux corners pointent leur branche active sur
     * @p expected, et que le partenaire correspondant est bien le corner
     * opposé (sécurité topologique).
     *
     * @param a         Premier corner.
     * @param b         Second corner.
     * @param expected  Branche attendue (NORMAL = traversante, DEVIATION = diagonale).
     */
    bool testPath(const SwitchBlock* a, const SwitchBlock* b, ActiveBranch expected)
    {
        if (!a || !b) return false;
        if (a->getActiveBranch() != expected) return false;
        if (b->getActiveBranch() != expected) return false;

        const ShuntingElement* aDest = (expected == ActiveBranch::NORMAL)
            ? a->getNormalBlock()
            : a->getDeviationBlock();
        const ShuntingElement* bDest = (expected == ActiveBranch::NORMAL)
            ? b->getNormalBlock()
            : b->getDeviationBlock();

        return aDest == b && bDest == a;
    }
}

SwitchCrossBlock::SwitchCrossBlock()
{
    LOG_INFO(m_logger, "SwitchCross created");
}

bool SwitchCrossBlock::isPathACActive() const
{
    return testPath(getCornerA(), getCornerC(), ActiveBranch::NORMAL);
}

bool SwitchCrossBlock::isPathBDActive() const
{
    return testPath(getCornerB(), getCornerD(), ActiveBranch::NORMAL);
}

bool SwitchCrossBlock::isPathADActive() const
{
    return testPath(getCornerA(), getCornerD(), ActiveBranch::DEVIATION);
}

bool SwitchCrossBlock::isPathBCActive() const
{
    return testPath(getCornerB(), getCornerC(), ActiveBranch::DEVIATION);
}

ShuntingState SwitchCrossBlock::getState() const
{
    const SwitchBlock* corners[4] = {
        getCornerA(), getCornerB(), getCornerC(), getCornerD()
    };

    for (const auto* sw : corners)
        if (sw && sw->getState() == ShuntingState::OCCUPIED)
            return ShuntingState::OCCUPIED;

    if (isPathACActive() || isPathBDActive()
        || isPathADActive() || isPathBCActive())
        return ShuntingState::FREE;

    return ShuntingState::INACTIVE;
}
