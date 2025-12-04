/******************************************************************************
 * Top contributors (to current version):
 *   Andrew Reynolds, Mathias Preiner, Gereon Kremer
 *
 * This file is part of the cvc5 project.
 *
 * Copyright (c) 2009-2025 by the authors listed in the file AUTHORS
 * in the top-level source directory and their institutional affiliations.
 * All rights reserved.  See the file COPYING in the top-level source
 * directory for licensing information.
 * ****************************************************************************
 *
 * Abstract management of models for TheoryEngine.
 */

#include "theory/model_manager.h"

#include "options/smt_options.h"
#include "options/theory_options.h"
#include "prop/prop_engine.h"
#include "smt/env.h"
#include "theory/quantifiers/first_order_model.h"
#include "theory/quantifiers/fmf/model_builder.h"
#include "theory/quantifiers_engine.h"
#include "theory/theory_engine.h"

namespace cvc5::internal {
namespace theory {

ModelManager::ModelManager(Env& env, TheoryEngine& te, EqEngineManager& eem)
    : EnvObj(env),
      d_te(te),
      d_eem(eem),
      d_modelEqualityEngine(nullptr),
      d_model(new TheoryModel(
          env, "DefaultModel", options().theory.assignFunctionValues)),
      d_modelBuilder(nullptr),
      d_modelBuilt(false),
      d_modelBuiltSuccess(false)
{
}

ModelManager::~ModelManager()
{
  // pop the model context which we pushed on initialization
  d_modelEeContext.pop();
}

void ModelManager::finishInit()
{
  // construct the model
  // Initialize the model and model builder.
  if (logicInfo().isQuantified())
  {
    QuantifiersEngine* qe = d_te.getQuantifiersEngine();
    Assert(qe != nullptr);
    d_modelBuilder = qe->getModelBuilder();
  }

  // make the default builder, e.g. in the case that the quantifiers engine does
  // not have a model builder
  if (d_modelBuilder == nullptr)
  {
    d_alocModelBuilder.reset(new TheoryEngineModelBuilder(d_env));
    d_modelBuilder = d_alocModelBuilder.get();
  }

  // The model manager uses the central equality engine
  d_modelEqualityEngine = d_eem.getCentralEqualityEngine();

  // finish initializing the model
  d_model->finishInit(d_modelEqualityEngine);
  // We push a context during initialization since the model is cleared during
  // collectModelInfo using pop/push.
  d_modelEeContext.push();
}

void ModelManager::resetModel()
{
  d_modelBuilt = false;
  d_modelBuiltSuccess = false;
  // Reset basic information on the model object
  d_model->reset();
}

bool ModelManager::buildModel()
{
  if (d_modelBuilt)
  {
    // already computed
    return d_modelBuiltSuccess;
  }

  ResourceManager* rm = d_env.getResourceManager();

  // Disable resource manager limit while building the model. This ensures
  // that building the model is not interrupted (and shouldn't take too
  // long).
  rm->setEnabled(false);

  // reset the flags now
  d_modelBuilt = true;
  d_modelBuiltSuccess = false;

  // prepare the model, which is specific to the manager
  if (!prepareModel())
  {
    Trace("model-builder") << "ModelManager: fail prepare model" << std::endl;
  }
  else
  {
    // now, finish building the model
    d_modelBuiltSuccess = finishBuildModel();

    if (TraceIsOn("model-final"))
    {
      Trace("model-final") << "Final model:" << std::endl;
      Trace("model-final") << d_model->debugPrintModelEqc() << std::endl;
    }

    Trace("model-builder") << "ModelManager: model built success is "
                           << d_modelBuiltSuccess << std::endl;
  }

  // Enable resource management again.
  rm->setEnabled(true);

  return d_modelBuiltSuccess;
}

bool ModelManager::isModelBuilt() const { return d_modelBuilt; }

void ModelManager::postProcessModel(bool incomplete)
{
  if (!d_modelBuilt)
  {
    // model not built, nothing to do
    return;
  }
  Trace("model-builder") << "ModelManager: post-process model..." << std::endl;
  // model construction should always succeed unless lemmas were added
  AlwaysAssert(d_modelBuiltSuccess);
  if (!options().smt.produceModels)
  {
    return;
  }
  // Do post-processing of model from the theories (used for THEORY_SEP
  // to construct heap model)
  for (TheoryId theoryId = theory::THEORY_FIRST; theoryId < theory::THEORY_LAST;
       ++theoryId)
  {
    Theory* t = d_te.theoryOf(theoryId);
    if (t == nullptr)
    {
      // theory not active, skip
      continue;
    }
    Trace("model-builder-debug")
        << "  PostProcessModel on theory: " << theoryId << std::endl;
    t->postProcessModel(d_model.get());
  }
  // also call the model builder's post-process model
  d_modelBuilder->postProcessModel(incomplete, d_model.get());
}

theory::TheoryModel* ModelManager::getModel() { return d_model.get(); }

bool ModelManager::prepareModel()
{
  Trace("model-builder") << "ModelManager: reset model..." << std::endl;

  // push/pop to clear the equality engine of the model
  d_modelEeContext.pop();
  d_modelEeContext.push();

  // Collect model info from the theories
  Trace("model-builder") << "ModelManager: Collect model info..." << std::endl;
  // Consult each active theory to get all relevant information concerning the
  // model, which includes both dump their equality information and assigning
  // values. Notice the order of theories here is important and is the same
  // as the list in CVC5_FOR_EACH_THEORY in theory_engine.cpp.
  const LogicInfo& logicInfo = d_env.getLogicInfo();
  for (TheoryId theoryId = theory::THEORY_FIRST; theoryId < theory::THEORY_LAST;
       ++theoryId)
  {
    if (!logicInfo.isTheoryEnabled(theoryId))
    {
      // theory not active, skip
      continue;
    }
    Theory* t = d_te.theoryOf(theoryId);
    if (theoryId == TheoryId::THEORY_BOOL
        || theoryId == TheoryId::THEORY_BUILTIN)
    {
      Trace("model-builder")
          << "  Skipping theory " << theoryId
          << " as it does not contribute to the model anyway" << std::endl;
      continue;
    }
    Trace("model-builder") << "  CollectModelInfo on theory: " << theoryId
                           << std::endl;
    // collect the asserted terms
    std::set<Node> termSet;
    t->collectAssertedTermsForModel(termSet);
    // also get relevant terms
    t->computeRelevantTerms(termSet);
    if (!t->collectModelInfo(d_model.get(), termSet))
    {
      Trace("model-builder")
          << "ModelManager: fail collect model info" << std::endl;
      return false;
    }
  }

  if (!collectModelBooleanVariables())
  {
    Trace("model-builder") << "ModelManager: fail Boolean variables"
                           << std::endl;
    return false;
  }

  return true;
}

bool ModelManager::finishBuildModel() const
{
  // do not use relevant terms
  if (!d_modelBuilder->buildModel(d_model.get()))
  {
    Trace("model-builder") << "ModelManager: fail build model" << std::endl;
    return false;
  }
  return true;
}

bool ModelManager::collectModelBooleanVariables()
{
  Trace("model-builder") << "  CollectModelInfo boolean variables" << std::endl;
  // Get value of the Boolean variables
  prop::PropEngine* propEngine = d_te.getPropEngine();
  std::vector<TNode> boolVars;
  propEngine->getBooleanVariables(boolVars);
  std::vector<TNode>::iterator it, iend = boolVars.end();
  bool hasValue, value;
  for (it = boolVars.begin(); it != iend; ++it)
  {
    TNode var = *it;
    hasValue = propEngine->hasValue(var, value);
    // Should we assert that hasValue is true?
    if (!hasValue)
    {
      Trace("model-builder-assertions")
          << "    has no value : " << var << std::endl;
      value = false;
    }
    Trace("model-builder-assertions")
        << "(assert" << (value ? " " : " (not ") << var
        << (value ? ");" : "));") << std::endl;
    if (!d_model->assertPredicate(var, value))
    {
      return false;
    }
  }
  return true;
}

}  // namespace theory
}  // namespace cvc5::internal
