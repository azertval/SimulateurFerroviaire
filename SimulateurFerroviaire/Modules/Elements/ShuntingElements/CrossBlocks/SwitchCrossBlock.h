#pragma once
#include "CrossBlock.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"

/**
 * @file  SwitchCrossBlock.h
 * @brief TJD — croisement encadré de quatre aiguillages.
 *
 * Les quatre branches sont garanties être des SwitchBlock* (vérification
 * effectuée par Phase6_BlockExtractor / Phase7_SwitchProcessor lors de la
 * création). Les casts static_cast<SwitchBlock*> dans les accesseurs typés
 * sont donc sûrs, sans nécessité de dynamic_cast ni de vérification à
 * l'exécution.
 *
 * @par Modèle de données
 * Un TJD relie quatre corners aiguillages selon le mapping (Q3=A — diagonale
 * via NORMAL) établi en Phase 7 :
 *   - A.normal = C, A.deviation = D
 *   - B.normal = D, B.deviation = C
 *   - C.normal = A, C.deviation = B
 *   - D.normal = B, D.deviation = A
 *
 * @par Topologie des 4 voies praticables
 * @code
 *   A ─────────────── C   (voie A↔C, traversée NORMAL)
 *      ╲           ╱
 *       ╲         ╱
 *        ╲       ╱       Voies diagonales :
 *         ╲     ╱         A↔D et B↔C, traversée DEVIATION
 *          ╲   ╱
 *           ╲ ╱
 *           cr/✕
 *           ╱ ╲
 *          ╱   ╲
 *         ╱     ╲
 *        ╱       ╲
 *       ╱         ╲
 *      ╱           ╲
 *   B ─────────────── D   (voie B↔D, traversée NORMAL)
 * @endcode
 *
 * @par Chemins actifs
 * Une voie est praticable si ses deux corners aux extrémités ont leur
 * branche active orientée correctement :
 *  - A↔C : A.active=NORMAL  ET C.active=NORMAL
 *  - B↔D : B.active=NORMAL  ET D.active=NORMAL
 *  - A↔D : A.active=DEVIATION ET D.active=DEVIATION
 *  - B↔C : B.active=DEVIATION ET C.active=DEVIATION
 *
 * @par États cohérents (sémantique TJD ferroviaire)
 * Un TJD physique se commute en bloc — soit toutes les voies traversantes
 * (NORMAL partout) sont actives, soit toutes les diagonales (DEVIATION
 * partout). Les autres combinaisons sont topologiquement incohérentes.
 */
class SwitchCrossBlock final : public CrossBlock
{
public:
    SwitchCrossBlock();

    [[nodiscard]] bool isTJD() const override { return true; }

    // =========================================================================
    // Accesseurs typés par corner
    // Sûrs car Phase 7 garantit SwitchBlock* sur toutes les branches.
    // =========================================================================

    /** @brief Corner aiguille A (entrée voie 1). */
    [[nodiscard]] SwitchBlock* getCornerA() const
    {
        return static_cast<SwitchBlock*>(m_branchA);
    }

    /** @brief Corner aiguille B (entrée voie 2). */
    [[nodiscard]] SwitchBlock* getCornerB() const
    {
        return static_cast<SwitchBlock*>(m_branchB);
    }

    /** @brief Corner aiguille C (sortie voie 1). */
    [[nodiscard]] SwitchBlock* getCornerC() const
    {
        return static_cast<SwitchBlock*>(m_branchC);
    }

    /** @brief Corner aiguille D (sortie voie 2). */
    [[nodiscard]] SwitchBlock* getCornerD() const
    {
        return static_cast<SwitchBlock*>(m_branchD);
    }

    // =========================================================================
    // Chemins actifs — quatre voies indépendantes
    // =========================================================================

    /**
     * @brief Voie traversante A↔C praticable.
     *
     * Condition : A et C ont tous deux leur branche active sur NORMAL,
     * et leur partenaire NORMAL est bien le corner opposé (sécurité).
     *
     * @return false si un corner est nullptr.
     */
    [[nodiscard]] bool isPathACActive() const;

    /**
     * @brief Voie traversante B↔D praticable.
     *
     * Condition : B et D ont tous deux leur branche active sur NORMAL.
     */
    [[nodiscard]] bool isPathBDActive() const;

    /**
     * @brief Voie diagonale A↔D praticable.
     *
     * Condition : A et D ont tous deux leur branche active sur DEVIATION.
     */
    [[nodiscard]] bool isPathADActive() const;

    /**
     * @brief Voie diagonale B↔C praticable.
     *
     * Condition : B et C ont tous deux leur branche active sur DEVIATION.
     */
    [[nodiscard]] bool isPathBCActive() const;

    // =========================================================================
    // État agrégé
    // =========================================================================

    /**
     * @brief Agrège l'état des quatre corners.
     *
     * Priorité :
     *   1. OCCUPIED si au moins un corner est occupé.
     *   2. FREE si au moins une voie est active.
     *   3. INACTIVE si aucune voie n'est praticable.
     */
    [[nodiscard]] ShuntingState getState() const override;
};
