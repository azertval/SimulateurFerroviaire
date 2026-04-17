#pragma once
#include "CrossBlock.h"
#include "Modules/Elements/ShuntingElements/StraightBlock.h"

/**
 * @file  StraightCrossBlock.h
 * @brief Croisement plat — deux StraightBlock traversent le nœud CROSSING.
 *
 * Les quatre branches sont garanties être des StraightBlock* (vérification
 * effectuée par Phase6_BlockExtractor lors de la création). Les casts
 * static_cast<StraightBlock*> dans les accesseurs typés sont donc sûrs.
 *
 * getState() retourne toujours ShuntingState::FREE — un croisement plat
 * n'a aucun mécanisme de blocage.
 */
class StraightCrossBlock final : public CrossBlock
{
public:
    StraightCrossBlock();

    /**
     * @brief Retourne toujours ShuntingState::FREE.
     */
    [[nodiscard]] ShuntingState getState() const override
    {
        return ShuntingState::FREE;
    }

    [[nodiscard]] bool isTJD() const override { return false; }

    // =========================================================================
    // Accesseurs typés par ligne — évite dynamic_cast chez les consommateurs
    // Sûrs car Phase6_BlockExtractor garantit StraightBlock* sur toutes les branches.
    // =========================================================================

    /** @brief Entrée de la ligne 1 (branche A). */
    [[nodiscard]] StraightBlock* getLine1Entry() const
    {
        return static_cast<StraightBlock*>(m_branchA);
    }

    /** @brief Sortie de la ligne 1 (branche C). */
    [[nodiscard]] StraightBlock* getLine1Exit() const
    {
        return static_cast<StraightBlock*>(m_branchC);
    }

    /** @brief Entrée de la ligne 2 (branche B). */
    [[nodiscard]] StraightBlock* getLine2Entry() const
    {
        return static_cast<StraightBlock*>(m_branchB);
    }

    /** @brief Sortie de la ligne 2 (branche D). */
    [[nodiscard]] StraightBlock* getLine2Exit() const
    {
        return static_cast<StraightBlock*>(m_branchD);
    }
};