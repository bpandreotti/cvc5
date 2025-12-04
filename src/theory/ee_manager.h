/******************************************************************************
 * Top contributors (to current version):
 *   Andrew Reynolds, Aina Niemetz
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * Utilities for management of equality engines.
 */

#include "cvc5_private.h"

#ifndef CVC5__THEORY__EE_MANAGER__H
#define CVC5__THEORY__EE_MANAGER__H

#include <map>
#include <memory>

#include "smt/env_obj.h"
#include "theory/ee_setup_info.h"
#include "theory/quantifiers/master_eq_notify.h"
#include "theory/theory.h"
#include "theory/uf/equality_engine.h"

namespace cvc5::internal {

class TheoryEngine;

namespace theory {

class SharedSolver;

/**
 * This is (theory-agnostic) information associated with the management of
 * an equality engine for a single theory. This information is maintained
 * by the manager class below.
 *
 * Currently, this simply is the equality engine itself, for memory
 * management purposes.
 */
struct EeTheoryInfo
{
  EeTheoryInfo() : d_usedEe(nullptr) {}
  /** Equality engine that is used (if it exists) */
  eq::EqualityEngine* d_usedEe;
  /** Equality engine allocated specifically for this theory (if it exists) */
  std::unique_ptr<eq::EqualityEngine> d_allocEe;
};

/** Virtual base class for equality engine managers */
class EqEngineManager : protected EnvObj
{
 public:
   /**
   * @param te Reference to the theory engine
   * @param sharedSolver The shared solver that is being used in combination
   * with this equality engine manager
    */
  EqEngineManager(Env& env, TheoryEngine& te, SharedSolver& shs);
  ~EqEngineManager();
  /**
   * Initialize theories, called during TheoryEngine::finishInit after theory
   * objects have been created but prior to their final initialization. This
   * sets up equality engines for all theories.
   *
   * This method is context-independent, and is applied once during
   * the lifetime of TheoryEngine (during finishInit).
   */
  void initializeTheories();
  /**
   * Get the equality engine theory information for theory with the given id.
   */
  const EeTheoryInfo* getEeTheoryInfo(TheoryId tid) const;

  /** Allocate equality engine that is context-dependent on c with info esi */
  eq::EqualityEngine* allocateEqualityEngine(EeSetupInfo& esi,
                                             context::Context* c);

  /**
   * Return true if the theory with the given id uses central equality engine
   * with the given options.
   */
  static bool usesCentralEqualityEngine(const Options& opts, TheoryId id);

 protected:
  /** Reference to the theory engine */
  TheoryEngine& d_te;
  /** Reference to the shared solver */
  SharedSolver& d_sharedSolver;
  /** Information related to the equality engine, per theory. */
  std::map<TheoryId, EeTheoryInfo> d_einfo;

 private:
  /**
   * Notify class for central equality engine. This class dispatches
   * notifications from the central equality engine to the appropriate
   * theory(s).
   */
  class CentralNotifyClass : public theory::eq::EqualityEngineNotify
  {
   public:
    CentralNotifyClass(EqEngineManager& eem);
    bool eqNotifyTriggerPredicate(TNode predicate, bool value) override;
    bool eqNotifyTriggerTermEquality(TheoryId tag,
                                     TNode t1,
                                     TNode t2,
                                     bool value) override;
    void eqNotifyConstantTermMerge(TNode t1, TNode t2) override;
    void eqNotifyNewClass(TNode t) override;
    void eqNotifyMerge(TNode t1, TNode t2) override;
    void eqNotifyDisequal(TNode t1, TNode t2, TNode reason) override;
    /** Parent */
    EqEngineManager& d_eem;
    /** List of notify classes that need new class notification */
    std::vector<eq::EqualityEngineNotify*> d_newClassNotify;
    /** List of notify classes that need merge notification */
    std::vector<eq::EqualityEngineNotify*> d_mergeNotify;
    /** List of notify classes that need disequality notification */
    std::vector<eq::EqualityEngineNotify*> d_disequalNotify;
    /** The model notify class */
    eq::EqualityEngineNotify* d_mNotify;
    /** The quantifiers engine */
    QuantifiersEngine* d_quantEngine;
  };
  /** Notification when predicate gets value in central equality engine */
  bool eqNotifyTriggerPredicate(TNode predicate, bool value);
  bool eqNotifyTriggerTermEquality(TheoryId tag,
                                   TNode t1,
                                   TNode t2,
                                   bool value);
  /** Notification when constants are merged in central equality engine */
  void eqNotifyConstantTermMerge(TNode t1, TNode t2);
  /** The master equality engine notify class */
  std::unique_ptr<quantifiers::MasterNotifyClass> d_masterEENotify;
  /** The master equality engine. */
  eq::EqualityEngine* d_masterEqualityEngine;
  /** The master equality engine, if we allocated it */
  std::unique_ptr<eq::EqualityEngine> d_masterEqualityEngineAlloc;
  /** The central equality engine notify class */
  CentralNotifyClass d_centralEENotify;
  /** The central equality engine. */
  eq::EqualityEngine d_centralEqualityEngine;
  /** The proof equality engine for the central equality engine */
  std::unique_ptr<eq::ProofEqEngine> d_centralPfee;
  /**
   * A table of from theory IDs to notify classes.
   */
  eq::EqualityEngineNotify* d_theoryNotify[theory::THEORY_LAST];
};

}  // namespace theory
}  // namespace cvc5::internal

#endif /* CVC5__THEORY__EE_MANAGER__H */
