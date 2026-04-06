/**
 * @file  SwitchCrossBlock.cpp
 * @brief Implémentation du croisement TJD (Traversée-Jonction Double).
 *
 * @see SwitchCrossBlock
 */
#include "SwitchCrossBlock.h"


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

    if (isPath1Active() || isPath2Active())
        return ShuntingState::FREE;

    return ShuntingState::INACTIVE;
}