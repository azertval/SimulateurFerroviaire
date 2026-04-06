#pragma once
#include "Modules/Elements/ShuntingElements/ShuntingElement.h"
#include "Engine/Core/Coordinates/CoordinateLatLon.h"
#include "Engine/Core/Coordinates/CoordinateXY.h"

/**
 * @file  CrossBlock.h
 * @brief Classe de base abstraite pour les croisements ferroviaires.
 *
 * Représente un nœud topologique de degré 4 (NodeClass::CROSSING).
 * Deux sous-classes concrètes : StraightCrossBlock et SwitchCrossBlock.
 *
 * @par Paires traversantes
 * Après résolution par Phase6_BlockExtractor::extractCrossings(), les quatre
 * branches sont groupées en deux voies indépendantes :
 *   - Voie 1 : branchA ↔ branchC
 *   - Voie 2 : branchB ↔ branchD
 */
class CrossBlock : public ShuntingElement
{
public:

    CrossBlock() = default;
    virtual ~CrossBlock() = default;
    CrossBlock(CrossBlock&&) = default;
    CrossBlock& operator=(CrossBlock&&) = default;

    // =========================================================================
    // Interface ShuntingElement
    // =========================================================================

    [[nodiscard]] std::string getId()   const override { return m_id; }
    [[nodiscard]] ElementType getType() const override { return ElementType::CROSSING; }
    // getState() reste virtuelle pure — comportement différent selon la sous-classe

    // =========================================================================
    // Interface CrossBlock
    // =========================================================================

    /**
     * @brief Retourne true si ce crossing est un TJD.
     * Implémenté par StraightCrossBlock (false) et SwitchCrossBlock (true).
     */
    [[nodiscard]] virtual bool isTJD() const = 0;

    // =========================================================================
    // Requêtes — géométrie
    // =========================================================================

    [[nodiscard]] const CoordinateXY& getJunctionUTM()   const { return m_junctionUTM; }
    [[nodiscard]] const CoordinateLatLon& getJunctionWGS84() const { return m_junctionWGS84; }

    // =========================================================================
    // Requêtes — branches (accès par label)
    // =========================================================================

    [[nodiscard]] ShuntingElement* getBranchA() const { return m_branchA; }
    [[nodiscard]] ShuntingElement* getBranchB() const { return m_branchB; }
    [[nodiscard]] ShuntingElement* getBranchC() const { return m_branchC; }
    [[nodiscard]] ShuntingElement* getBranchD() const { return m_branchD; }

    // =========================================================================
    // Requêtes — branches (accès sémantique par voie)
    // Voie 1 : A → C  |  Voie 2 : B → D
    // =========================================================================

    [[nodiscard]] ShuntingElement* getPath1Entry() const { return m_branchA; }
    [[nodiscard]] ShuntingElement* getPath1Exit()  const { return m_branchC; }
    [[nodiscard]] ShuntingElement* getPath2Entry() const { return m_branchB; }
    [[nodiscard]] ShuntingElement* getPath2Exit()  const { return m_branchD; }

    // =========================================================================
    // Mutations — identifiant
    // =========================================================================

    void setId(std::string id) { m_id = std::move(id); }

    // =========================================================================
    // Mutations — géométrie
    // =========================================================================

    void setJunctionUTM(CoordinateXY pos) { m_junctionUTM = pos; }
    void setJunctionWGS84(CoordinateLatLon pos) { m_junctionWGS84 = pos; }

    // =========================================================================
    // Mutations — pointeurs résolus (Phase8_RepositoryTransfer)
    // =========================================================================

    void setBranchAPointer(ShuntingElement* e) { m_branchA = e; }
    void setBranchBPointer(ShuntingElement* e) { m_branchB = e; }
    void setBranchCPointer(ShuntingElement* e) { m_branchC = e; }
    void setBranchDPointer(ShuntingElement* e) { m_branchD = e; }

    /**
     * @brief Représentation textuelle pour le débogage.
     */
    [[nodiscard]] std::string toString() const;

protected:

    CoordinateXY     m_junctionUTM;
    CoordinateLatLon m_junctionWGS84;

    // Pointeurs non-propriétaires — possédés par TopologyRepository
    ShuntingElement* m_branchA = nullptr;  ///< Voie 1, entrée.
    ShuntingElement* m_branchB = nullptr;  ///< Voie 2, entrée.
    ShuntingElement* m_branchC = nullptr;  ///< Voie 1, sortie.
    ShuntingElement* m_branchD = nullptr;  ///< Voie 2, sortie.

    ShuntingState m_state = ShuntingState::FREE;
};