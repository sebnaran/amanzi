/*
  Tests the functionality of a simple DAG, executed on a device.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include "Epetra_MpiComm.h"
#include "Epetra_Vector.h"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_ParameterXMLFileReader.hpp"
#include "Teuchos_RCP.hpp"
#include "UnitTest++.h"

#include "AmanziComm.hh"
#include "Mesh.hh"
#include "MeshFactory.hh"
#include "State.hh"
#include "evaluator/EvaluatorSecondaryMonotype.hh"
#include "evaluator/EvaluatorPrimary.hh"

using namespace Amanzi;
using namespace Amanzi::AmanziMesh;

/*
  We will build the following dependencies tree:
    A -> {B, C, E, H}
    C -> {D, G}
    E -> {D, F}
    H -> F
    D -> G
    F -> G

  Primary fields are B=2 and G=3. The equations are
    A = 2*B + C*E*H = 6484
    C = 2*D + G     = 15
    E = D*F         = 36
    H = 2*F         = 12
    D = 2*G         = 6
    F = 2*G         = 6

  Derivatives are
    dA/dB = 2
    dA/dG = 8640

  WARNING: derivative of secondary field wrt to secondary field is
  not well defined. The code may throw an exception since
  intermediate derivatives are not saved.
*/

/* ******************************************************************
 * Equation A = 2*B + C*E*H
 ****************************************************************** */
// Device Type : GPUs or CPUs??

template<class DeviceType>
class AEvaluator : public EvaluatorSecondaryMonotype<CompositeVector, CompositeVectorSpace> {
public:
  AEvaluator(Teuchos::ParameterList &plist)
    : EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace>(plist) {
    dependencies_.emplace_back(std::make_pair(Key("fb"), Key()));
    dependencies_.emplace_back(std::make_pair(Key("fc"), Key()));
    dependencies_.emplace_back(std::make_pair(Key("fe"), Key()));
    dependencies_.emplace_back(std::make_pair(Key("fh"), Key()));
  }

  virtual Teuchos::RCP<Evaluator> Clone() const override {
    return Teuchos::rcp(new AEvaluator(*this));
  }

  virtual void Evaluate_(const State &S, const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto &fb_c = S.Get<CompositeVector>("fb").ViewComponent("cell",false);
    const auto &fc_c = S.Get<CompositeVector>("fc").ViewComponent("cell",false);
    const auto &fe_c = S.Get<CompositeVector>("fe").ViewComponent("cell",false);
    const auto &fh_c = S.Get<CompositeVector>("fh").ViewComponent("cell",false);
    Kokkos::parallel_for(1, KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = 2 * fb_c(i,0) + fc_c(i,0) * fe_c(i,0) * fh_c(i,0);
    });
  }
  
  virtual void EvaluatePartialDerivative_(const State &S, const Key &wrt_key,
                                          const Key &wrt_tag,
                                          const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto fc_c = S.Get<CompositeVector>("fc").ViewComponent("cell");
    const auto fe_c = S.Get<CompositeVector>("fe").ViewComponent("cell");
    const auto fh_c = S.Get<CompositeVector>("fh").ViewComponent("cell");

    if (wrt_key == "fb") {
      Kokkos::parallel_for(1, KOKKOS_LAMBDA(const int i) {
        result_c(i,0) = 2.0;
      });
    } else if (wrt_key == "fc") {
      Kokkos::parallel_for(1, KOKKOS_LAMBDA(const int i) {
        result_c(i,0) = fe_c(i,0) * fh_c(i,0);
      });
    } else if (wrt_key == "fe") {
    Kokkos::parallel_for(1, KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = fc_c(i,0) * fh_c(i,0);
    });
    } else if (wrt_key == "fh") {
    Kokkos::parallel_for(1, KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = fc_c(i,0) * fe_c(i,0);
    });
    }
  }
};

/* ******************************************************************
 * Equation C = 2*D + G
 ****************************************************************** */
class CEvaluator : public EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace> {
public:
  CEvaluator(Teuchos::ParameterList &plist)
    : EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace>(plist) {
    dependencies_.emplace_back(std::make_pair(Key("fd"), Key()));
    dependencies_.emplace_back(std::make_pair(Key("fg"), Key()));
  }

  virtual Teuchos::RCP<Evaluator> Clone() const override {
    return Teuchos::rcp(new CEvaluator(*this));
  }

  virtual void Evaluate_(const State &S, const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto fd_c = S.Get<CompositeVector>("fd").ViewComponent("cell",false);
    const auto fg_c = S.Get<CompositeVector>("fg").ViewComponent("cell",false);
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
    result_c(i,0) = 2 * fd_c(i,0) + fg_c(i,0);
    });
  }

  virtual void EvaluatePartialDerivative_(const State &S, const Key &wrt_key,
                                          const Key &wrt_tag,
                                          const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    if (wrt_key == "fd") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = 2.;
    });
    } else if (wrt_key == "fg") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = 1.;
    });
    }
  }
};

/* ******************************************************************
 * Equation D = 2*G
 ****************************************************************** */
class DEvaluator : public EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace> {
public:
  DEvaluator(Teuchos::ParameterList &plist)
    : EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace>(plist) {
    dependencies_.emplace_back(std::make_pair(Key("fg"), Key()));
  }

  virtual Teuchos::RCP<Evaluator> Clone() const override {
    return Teuchos::rcp(new DEvaluator(*this));
  }

  virtual void Evaluate_(const State &S, const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto fg_c = S.Get<CompositeVector>("fg").ViewComponent("cell",false);
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
    result_c(i,0) = 2 * fg_c(i,0);
    });
  }

  virtual void EvaluatePartialDerivative_(const State &S, const Key &wrt_key,
                                          const Key &wrt_tag,
                                          const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    if (wrt_key == "fg") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = 2.;
    });
    }
  }
};

/* ******************************************************************
 * Equation E = D*F
 ****************************************************************** */
class EEvaluator : public EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace> {
public:
  EEvaluator(Teuchos::ParameterList &plist)
    : EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace>(plist) {
    dependencies_.emplace_back(std::make_pair(Key("fd"), Key()));
    dependencies_.emplace_back(std::make_pair(Key("ff"), Key()));
  }

  virtual Teuchos::RCP<Evaluator> Clone() const override {
    return Teuchos::rcp(new EEvaluator(*this));
  }

  virtual void Evaluate_(const State &S, const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto fd_c = S.Get<CompositeVector>("fd").ViewComponent("cell",false);
    const auto ff_c = S.Get<CompositeVector>("ff").ViewComponent("cell",false);
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
    result_c(i,0) = fd_c(i,0) * ff_c(i,0);
    });
  }

  virtual void EvaluatePartialDerivative_(const State &S, const Key &wrt_key,
                                          const Key &wrt_tag,
                                          const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto fd_c = S.Get<CompositeVector>("fd").ViewComponent("cell",false);
    const auto ff_c = S.Get<CompositeVector>("ff").ViewComponent("cell",false);

    if (wrt_key == "fd") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = ff_c(i,0);
    });
    } else if (wrt_key == "ff") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = fd_c(i,0);
    });
    }
  }
};

/* ******************************************************************
 * Equation F = 2*G
 ****************************************************************** */
class FEvaluator : public EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace> {
public:
  FEvaluator(Teuchos::ParameterList &plist)
    : EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace>(plist) {
    dependencies_.emplace_back(std::make_pair(Key("fg"), Key()));
  }

  virtual Teuchos::RCP<Evaluator> Clone() const override {
    return Teuchos::rcp(new FEvaluator(*this));
  }

  virtual void Evaluate_(const State &S, const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto fg_c = S.Get<CompositeVector>("fg").ViewComponent("cell",false);
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
    result_c(i,0) = 2 * fg_c(i,0);
    });
  }

  virtual void EvaluatePartialDerivative_(const State &S, const Key &wrt_key,
                                          const Key &wrt_tag,
                                          const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    if (wrt_key == "fg") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = 2.;
    });
    }
  }
};

/* ******************************************************************
 * Equation H = 2*F
 ****************************************************************** */
class HEvaluator : public EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace> {
public:
  HEvaluator(Teuchos::ParameterList &plist)
    : EvaluatorSecondaryMonotype<CompositeVector,CompositeVectorSpace>(plist) {
    dependencies_.emplace_back(std::make_pair(Key("ff"), Key()));
  }

  virtual Teuchos::RCP<Evaluator> Clone() const override {
    return Teuchos::rcp(new HEvaluator(*this));
  }

  virtual void Evaluate_(const State &S, const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    const auto ff_c = S.Get<CompositeVector>("ff").ViewComponent("cell",false);
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
    result_c(i,0) = 2. * ff_c(i,0);
    });
  }

  virtual void EvaluatePartialDerivative_(const State &S, const Key &wrt_key,
                                          const Key &wrt_tag,
                                          const std::vector<CompositeVector*> &results) override {
    auto result_c = results[0]->ViewComponent("cell",false);
    if (wrt_key == "ff") {
    Kokkos::parallel_for(result_c.extent(0), KOKKOS_LAMBDA(const int i) {
      result_c(i,0) = 2.;
    });
    }
  }
};

class make_state {
public:
  make_state() {
    Teuchos::ParameterList es_list, ep_list;
    es_list.sublist("verbose object")
        .set<std::string>("verbosity level", "extreme");
    ep_list.sublist("verbose object")
        .set<std::string>("verbosity level", "extreme");


    // create a mesh
    auto comm = Amanzi::getDefaultComm();
    MeshFactory meshfac(comm);
    auto mesh = meshfac.create(0.0, 0.0, 0.0, 4.0, 4.0, 4.0, 2, 2, 2);

    // create a state
    //State S;
    S.RegisterDomainMesh(mesh);

    // Secondary fields
    // --  A and its evaluator
    es_list.setName("fa");
    es_list.set("tag", "");
    S.Require<CompositeVector,CompositeVectorSpace>("fa", "", "fa")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    S.RequireDerivative<CompositeVector,CompositeVectorSpace>("fa", "", "fb", "");
    S.RequireDerivative<CompositeVector,CompositeVectorSpace>("fa", "", "fg", "");
    fa_eval = Teuchos::rcp(new AEvaluator<AmanziDefaultDevice>(es_list));
    S.SetEvaluator("fa", fa_eval);

    // --  C and its evaluator
    es_list.setName("fc");
    S.Require<CompositeVector,CompositeVectorSpace>("fc", "", "fc")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    fc_eval = Teuchos::rcp(new CEvaluator(es_list));
    S.SetEvaluator("fc", fc_eval);

    // --  D and its evaluator
    es_list.setName("fd");
    S.Require<CompositeVector,CompositeVectorSpace>("fd", "", "fd")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    fd_eval = Teuchos::rcp(new DEvaluator(es_list));
    S.SetEvaluator("fd", fd_eval);

    // --  E and its evaluator
    es_list.setName("fe");
    S.Require<CompositeVector,CompositeVectorSpace>("fe", "", "fe")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    S.RequireDerivative<CompositeVector,CompositeVectorSpace>("fe", "", "fg", "");
    fe_eval = Teuchos::rcp(new EEvaluator(es_list));
    S.SetEvaluator("fe", fe_eval);

    // --  F and its evaluator
    es_list.setName("ff");
    S.Require<CompositeVector,CompositeVectorSpace>("ff", "", "ff")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    ff_eval = Teuchos::rcp(new FEvaluator(es_list));
    S.SetEvaluator("ff", ff_eval);

    // --  H and its evaluator
    es_list.setName("fh");
    S.Require<CompositeVector,CompositeVectorSpace>("fh", "", "fh")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    fh_eval = Teuchos::rcp(new HEvaluator(es_list));
    S.SetEvaluator("fh", fh_eval);

    // Primary fields
    ep_list.setName("fb");
    // -- field B and its evaluator
    S.Require<CompositeVector,CompositeVectorSpace>("fb", "", "fb")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    fb_eval = Teuchos::rcp(new EvaluatorPrimary<CompositeVector,CompositeVectorSpace>(ep_list));
    S.SetEvaluator("fb", fb_eval);

    // -- field G and its evaluator
    ep_list.setName("fg");
    S.Require<CompositeVector,CompositeVectorSpace>("fg", "", "fg")
      .SetMesh(mesh)
      ->SetGhosted(true)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
    fg_eval = Teuchos::rcp(new EvaluatorPrimary<CompositeVector,CompositeVectorSpace>(ep_list));
    S.SetEvaluator("fg", fg_eval);

    // Setup fields initialize
    S.Setup();
    S.GetW<CompositeVector>("fb", "fb").putScalar(2.0);
    S.GetRecordW("fb", "fb").set_initialized();
    S.GetW<CompositeVector>("fg", "fg").putScalar(3.0);
    S.GetRecordW("fg", "fg").set_initialized();
    S.Initialize();

  }

public:
  State S;
  Teuchos::RCP<AEvaluator<AmanziDefaultDevice>> fa_eval;
  Teuchos::RCP<CEvaluator> fc_eval;
  Teuchos::RCP<DEvaluator> fd_eval;
  Teuchos::RCP<EEvaluator> fe_eval;
  Teuchos::RCP<FEvaluator> ff_eval;
  Teuchos::RCP<HEvaluator> fh_eval;
  Teuchos::RCP<EvaluatorPrimary<CompositeVector,CompositeVectorSpace>> fb_eval, fg_eval;
};

SUITE(DAG) {
  TEST_FIXTURE(make_state, DAG_TWO_FIELDS) {
    // check initialized properly
    CHECK_CLOSE(2.0, (S.Get<CompositeVector>("fb").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK_CLOSE(3.0, (S.Get<CompositeVector>("fg").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    
    // calculate field A
    std::cout << "Calculate field A:" << std::endl;
    bool changed = fa_eval->Update(S, "main");
    CHECK_CLOSE(6484.0, (S.Get<CompositeVector>("fa").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK(changed);

    
    // check intermediate steps got updated too
    CHECK_CLOSE(6.0, (S.Get<CompositeVector>("fd").ViewComponent<AmanziDefaultHost>("cell", false))(0,0), 1e-12);
    
    // calculate dA/dB
    std::cout << "Calculate derivative of field A wrt field B:" << std::endl;
    changed = fa_eval->UpdateDerivative(S, "fa", "fb", "");
    CHECK_CLOSE(2.0, (S.GetDerivative<CompositeVector>("fa", "", "fb", "").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK(changed);
    
    // calculate dA/dG
    std::cout << "Calculate derivative of field A wrt field G:" << std::endl;
    changed = fa_eval->UpdateDerivative(S, "fa", "fg", "");
    CHECK_CLOSE(8640.0, (S.GetDerivative<CompositeVector>("fa", "", "fg", "").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK(changed);

    // calculate dE/dG:
    std::cout << "Calculate derivative of field E wrt field G:" << std::endl;
    changed = fe_eval->UpdateDerivative(S, "fe", "fg", "");
    CHECK_CLOSE(24.0, (S.GetDerivative<CompositeVector>("fe", "", "fg", "").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK(changed);

    // Now we repeat some calculations. Since no primary fields changed,
    // the result should be the same
    // calculate dA/dG
    std::cout << "Calculate derivative of field A wrt field G:" << std::endl;
    changed = fa_eval->UpdateDerivative(S, "fa", "fg", "");
    CHECK_CLOSE(8640.0, (S.GetDerivative<CompositeVector>("fa", "", "fg", "").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK(!changed);

    std::cout << "Calculate derivative of field A wrt field G:" << std::endl;
    fb_eval->SetChanged();
    changed = fa_eval->UpdateDerivative(S, "fa", "fg", "");
    CHECK_CLOSE(8640.0, (S.GetDerivative<CompositeVector>("fa", "", "fg", "").ViewComponent<AmanziDefaultHost>("cell",false))(0,0), 1e-12);
    CHECK(changed);
  }
}