/**
 * @file  SwitchCrossBlock.cpp
 * @brief Implémentation du croisement TJD (Traversée-Jonction Double).
 *
 * @see SwitchCrossBlock
 */
#include "SwitchCrossBlock.h"

namespace
{
    /**
     * @brief Test générique de praticabilité d'une voie entre deux corners.
     *
     * Une voie est praticable si :
     *  - les deux corners ont leur branche active sur @p expected (NORMAL ou DEVIATION),
     *  - leur partenaire (m_doubleOnNormal ou m_doubleOnDeviation selon le côté)
     *    est bien le corner opposé.
     *
     * Le second critère est une sécurité topologique post-absorption Phase 7 :
     * les pointeurs sont mis à jour par SwitchBlock::absorbTJD pour pointer
     * directement vers le SwitchBlock partenaire — la voie est cohérente si
     * la chaîne est bidirectionnelle.
     */
    bool testTjdPath(const SwitchBlock* a, const SwitchBlock* b, ActiveBranch expected)
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

bool SwitchCrossBlock::isPath1Active() const
{
    if (!m_branchA || !m_branchC) return false;

    SwitchBlock* swEntry = getLine1Entry();  // branche A
    SwitchBlock* swExit = getLine1Exit();   // branche C

    // La branche active du switch pointe-t-elle vers ce crossing ?
    // On récupère le bloc pointé par la branche active (normal ou deviation)
    // et on compare son adresse à this.
    auto pointsToCrossing = [this](const SwitchBlock* sw) -> bool
        {
            if (!sw) return false;
            const ShuntingElement* activeDest =
                (sw->getActiveBranch() == ActiveBranch::NORMAL)
                ? sw->getNormalBlock()
                : sw->getDeviationBlock();
            return activeDest == this;
        };

    return pointsToCrossing(swEntry) && pointsToCrossing(swExit);
}

bool SwitchCrossBlock::isPath2Active() const
{
    if (!m_branchB || !m_branchD) return false;

    SwitchBlock* swEntry = getLine2Entry();  // branche B
    SwitchBlock* swExit = getLine2Exit();   // branche D

    auto pointsToCrossing = [this](const SwitchBlock* sw) -> bool
        {
            if (!sw) return false;
            const ShuntingElement* activeDest =
                (sw->getActiveBranch() == ActiveBranch::NORMAL)
                ? sw->getNormalBlock()
                : sw->getDeviationBlock();
            return activeDest == this;
        };

    return pointsToCrossing(swEntry) && pointsToCrossing(swExit);
}

// =============================================================================
// Voies TJD post-absorption (4 voies)
// =============================================================================

bool SwitchCrossBlock::isPathACActive() const
{
    return testTjdPath(getLine1Entry(), getLine1Exit(), ActiveBranch::NORMAL);
}

bool SwitchCrossBlock::isPathBDActive() const
{
    return testTjdPath(getLine2Entry(), getLine2Exit(), ActiveBranch::NORMAL);
}

bool SwitchCrossBlock::isPathADActive() const
{
    return testTjdPath(getLine1Entry(), getLine2Exit(), ActiveBranch::DEVIATION);
}

bool SwitchCrossBlock::isPathBCActive() const
{
    return testTjdPath(getLine2Entry(), getLine1Exit(), ActiveBranch::DEVIATION);
}

ShuntingState SwitchCrossBlock::getState() const
{
    // Parcours des 4 branches — accesseurs typés, pas de cast supplémentaire
    const SwitchBlock* lines[4] = {
        getLine1Entry(), getLine1Exit(),
        getLine2Entry(), getLine2Exit()
    };
    for (const auto* sw : lines)
        if (sw && sw->getState() == ShuntingState::OCCUPIED)
            return ShuntingState::OCCUPIED;

    if (isPathACActive() || isPathBDActive()
        || isPathADActive() || isPathBCActive())
        return ShuntingState::FREE;

    return ShuntingState::INACTIVE;
}