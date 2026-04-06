#pragma once
#include "CrossBlock.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"

/**
 * @file  SwitchCrossBlock.h
 * @brief TJD — croisement encadré de quatre aiguillages.
 *
 * Les quatre branches sont garanties être des SwitchBlock* (vérification
 * effectuée par Phase6_BlockExtractor lors de la création). Les casts
 * static_cast<SwitchBlock*> dans les accesseurs typés sont donc sûrs,
 * sans nécessité de dynamic_cast ni de vérification à l'exécution.
 *
 * @par Modèle de données
 * Un TJD est structurellement deux lignes traversantes, chacune encadrée
 * de deux SwitchBlock :
 *   Ligne 1 : getLine1Entry() ——[cr]—— getLine1Exit()
 *   Ligne 2 : getLine2Entry() ——[cr]—— getLine2Exit()
 *
 * @par Chemins actifs
 * Un chemin est actif si les deux SwitchBlock de sa ligne pointent leur
 * branche active vers ce CrossBlock (SwitchBlock::getActiveBranch() == this).
 */
class SwitchCrossBlock final : public CrossBlock
{
public:
    SwitchCrossBlock() = default;

    [[nodiscard]] bool isTJD() const override { return true; }

    // =========================================================================
    // Accesseurs typés par ligne
    // Sûrs car Phase6_BlockExtractor garantit SwitchBlock* sur toutes les branches.
    // =========================================================================

    /** @brief Switch d'entrée de la ligne 1 (branche A). */
    [[nodiscard]] SwitchBlock* getLine1Entry() const
    {
        return static_cast<SwitchBlock*>(m_branchA);
    }

    /** @brief Switch de sortie de la ligne 1 (branche C). */
    [[nodiscard]] SwitchBlock* getLine1Exit() const
    {
        return static_cast<SwitchBlock*>(m_branchC);
    }

    /** @brief Switch d'entrée de la ligne 2 (branche B). */
    [[nodiscard]] SwitchBlock* getLine2Entry() const
    {
        return static_cast<SwitchBlock*>(m_branchB);
    }

    /** @brief Switch de sortie de la ligne 2 (branche D). */
    [[nodiscard]] SwitchBlock* getLine2Exit() const
    {
        return static_cast<SwitchBlock*>(m_branchD);
    }

    // =========================================================================
    // Chemins actifs
    // =========================================================================

    /**
     * @brief Retourne true si la ligne 1 (A↔C) est praticable.
     *
     * Condition : les deux switches de la ligne pointent leur branche
     * active vers ce CrossBlock.
     *
     * @return false si un pointeur de branche est nullptr.
     */
    [[nodiscard]] bool isPath1Active() const;
    /**
     * @brief Retourne true si la ligne 2 (B↔D) est praticable.
     */
    [[nodiscard]] bool isPath2Active() const;

    // =========================================================================
    // État agrégé
    // =========================================================================

    /**
     * @brief Agrège l'état des quatre switches adjacents.
     *
     * Priorité :
     *   1. OCCUPIED si au moins un switch est occupé.
     *   2. FREE si au moins un chemin est actif.
     *   3. INACTIVE si aucun chemin n'est ouvert.
     */
    [[nodiscard]] ShuntingState getState() const override;
};