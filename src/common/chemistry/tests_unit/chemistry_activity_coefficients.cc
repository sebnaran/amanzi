#include <cstdlib>
#include <cmath>
#include <iostream>
#include <typeinfo>
#include <vector>

#include "UnitTest++.h"

#include "species.hh"
#include "aqueous_equilibrium_complex.hh"
#include "activity_model_factory.hh"
#include "activity_model_unit.hh"
#include "activity_model_debye_huckel.hh"
#include "activity_model.hh"
#include "chemistry_exception.hh"

/*****************************************************************************
*  Common testing code
*****************************************************************************/
namespace ac = Amanzi::AmanziChemistry;

class ActivityModelTest {
 public:
  ActivityModelTest();
  ~ActivityModelTest();

  void RunTest(const std::string name, double* gamma);

  void set_activity_model_name(const std::string name) {
    activity_model_name_ = name;
  };
  std::string activity_model_name(void) const {
    return activity_model_name_;
  };
  double ionic_strength(void) {
    return activity_model_->ionic_strength();
  };
  double tolerance(void) {
    return tolerance_;
  };

 protected:
  ac::ActivityModelFactory amf_;

 private:
  double tolerance_;
  ac::ActivityModel* activity_model_;
  std::string activity_model_name_;
  ac::SpeciesArray species_;
  ac::Species H_p;
  ac::Species OH_m;
  ac::Species Ca_pp;
  ac::Species SO4_mm;
  ac::Species Al_ppp;
  ac::Species PO4_mmm;
  std::vector<ac::AqueousEquilibriumComplex> aqueous_complexes_;
};  // end class SpeciationTest

ActivityModelTest::ActivityModelTest()
    : amf_(),
      tolerance_(1.0e-5),
      activity_model_name_(""),
      H_p(0, "H+", 1.0, 1.0079, 9.0),
      OH_m(1, "OH-", -1.0, 17.0073, 3.5),
      Ca_pp(2, "Ca++", 2.0, 40.0780, 6.0),
      SO4_mm(3, "SO4--", -2.0, 96.0636, 4.0),
      Al_ppp(4, "Al+++", 3.0, 26.9815, 9.0),
      PO4_mmm(5, "PO4---", -3.0, 94.9714, 4.0) {
  // set concentrations to get ionic strength of 0.025
  H_p.update(0.0005);
  OH_m.update(0.0015);
  Ca_pp.update(0.001);
  SO4_mm.update(0.002);
  Al_ppp.update(0.003);
  PO4_mmm.update(0.001);
  species_.push_back(H_p);
  species_.push_back(OH_m);
  species_.push_back(Ca_pp);
  species_.push_back(SO4_mm);
  species_.push_back(Al_ppp);
  species_.push_back(PO4_mmm);
  // TODO(bandre): should add some aqueous complexes to test ionic strength....
  aqueous_complexes_.clear();
}

ActivityModelTest::~ActivityModelTest() {
  delete activity_model_;
}

void ActivityModelTest::RunTest(const std::string name, double * gamma) {
  int index = -1;
  for (std::vector<ac::Species>::iterator primary = species_.begin();
       primary != species_.end(); primary++) {
    if (primary->name() == name) {
      index = primary->identifier();
    }
  }
  *gamma = -1.0;  // final value should always be > 0

  ac::ActivityModel::ActivityModelParameters parameters;
  parameters.database_filename = "";
  parameters.pitzer_jfunction = "";

  activity_model_ = amf_.Create(activity_model_name(), parameters, 
                                species_, aqueous_complexes_);
  activity_model_->CalculateIonicStrength(species_, aqueous_complexes_);
  *gamma = activity_model_->Evaluate(species_.at(index));
}  // end ActivityModelTest::RunTest()


/*!
  @namespace Amanzi::AmanziChemistry::unit_tests::ActivityModel

  @details Unit tests for the activity model base class, Amanzi::AmanziChemistry::ActivityModel

  @test ActivityModel
*/

SUITE(amanzi_chemistry_unit_tests_ActivityModel) {
  /*!
    @class Amanzi::AmanziChemistry::unit_tests::ActivityModel::ActivityModel_IonicStrength

    @brief ActivityModel_IonicStrength

    @details Test that a the ionic strength is calculated
    correctly for an arbitrary choice of 6 ions with charge of +-1,
    +-2, +-3. Adjust concentrations to yield an ionic strength of
    0.025.

    @test ActivityModel::CalculateIonicStrength()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModel_IonicStrength) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("H+", &gamma);
    // std::cout << "ionic strength: " << ionic_strength() << std::endl;
    CHECK_CLOSE(0.025, ionic_strength(), tolerance());
  }
}  // end SUITE(amanzi_chemistry_unit_tests_ActivityModel)


/*!
  @namespace Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit

  @details Unit tests for class ActivityModelUnit.

  - Unit activity coefficients are always 1.0 regardless of species
  @f[ \gamma_i = 1.0 @f]

  @test ActivityModelUnit
*/

SUITE(amanzi_chemistry_unit_tests_ActivityModelUnit) {
  /*!
    @brief ActivityModelUnit_H

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit::ActivityModelUnit_H

    @details Test calculation of unit activity coefficient for @f$ H^{+} @f$

    @test ActivityModelUnit::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelUnit_H) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("H+", &gamma);
    CHECK_EQUAL(1.0, gamma);
  }

  /*!
    @brief ActivityModelUnit_OH

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit::ActivityModelUnit_OH

    @details Test calculation of unit activity coefficient for @f$ OH^{+} @f$

    @test ActivityModelUnit::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelUnit_OH) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("OH-", &gamma);
    CHECK_EQUAL(1.0, gamma);
  }

  /*!
    @brief ActivityModelUnit_Ca

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit::ActivityModelUnit_Ca

    @details Test calculation of unit activity coefficient for @f$ Ca^{+2} @f$

    @test ActivityModelUnit::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelUnit_Ca) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("Ca++", &gamma);
    CHECK_EQUAL(1.0, gamma);
  }

  /*!
    @brief ActivityModelUnit_SO4

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit::ActivityModelUnit_SO4

    @details Test calculation of unit activity coefficient for @f$ SO4^{-2} @f$

    @test ActivityModelUnit::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelUnit_SO4) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("SO4--", &gamma);
    CHECK_EQUAL(1.0, gamma);
  }

  /*!
    @brief ActivityModelUnit_Al

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit::ActivityModelUnit_Al

    @details Test calculation of unit activity coefficient for @f$ Al^{+3} @f$

    @test ActivityModelUnit::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelUnit_Al) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("Al+++", &gamma);
    CHECK_EQUAL(1.0, gamma);
  }

  /*!
    @brief ActivityModelUnit_PO4

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelUnit::ActivityModelUnit_PO4

    @details Test calculation of unit activity coefficient for @f$ PO4^{-3} @f$

    @test ActivityModelUnit::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelUnit_PO4) {
    set_activity_model_name(ac::ActivityModelFactory::unit);
    double gamma;
    RunTest("PO4---", &gamma);
    CHECK_EQUAL(1.0, gamma);
  }
}  // end SUITE(amanzi_chemistry_unit_tests_ActivityModelUnit)


/*!
  @namespace Amanzi::AmanziChemistry::unit_tests::ActivityModelDebyeHuckel

  @details Test the calculation of the Debye-Huckel B-dot activity
  coefficients is correct.

  @f[
  \log \gamma _{i} =
  - \frac{A_{\gamma} z_{i}^{2} \sqrt{ \bar{I} }}
  {1+ \mathring{a_i} B_{\gamma } \sqrt{\bar{I}}}
  + \dot{B} \bar{I}
  @f]

  - Debye-Huckel: check first two digits of activity coefficients
  at 25C with Langmuir, 1997, Aqueous Environmental Geochemistry,
  Table 4.1 and 4.2, pg 130-131. Note, code uses slightly different
  debyeA and debyeB parameters.

  - source for higher number of sig figs?

  - For temperature dependance, run 5-10 temperature values, then
  store results in a vector and use CHECK_ARRAY_CLOSE(). Source
  for temperature dependance?

  @test ActivityModelDebyHuckel
*/

SUITE(amanzi_chemistry_unit_tests_ActivityModelDebyeHuckel) {
  /*!
    @brief ActivityModelDebyeHuckel_H

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelDebyeHuckel::ActivityModelDebyeHuckel_H

    @details Test calculation of Debye-Huckel activity coefficient for @f$ H^{+} @f$

    @test ActivityModelDebyeHuckel::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelDebyeHuckel_H) {
    set_activity_model_name(ac::ActivityModelFactory::debye_huckel);
    double gamma;
    RunTest("H+", &gamma);
    CHECK_CLOSE(0.88, gamma, 1.0e-2);
  }

  /*!
    @brief ActivityModelDebyeHuckel_OH

    @class Amanzi::AmanziChemistry::unit_tests::ActivityModelDebyeHuckel::ActivityModelDebyeHuckel_OH

    @details Test calculation of Debye-Huckel activity coefficient for @f$ OH^{-} @f$

    @test ActivityModelDebyeHuckel::Evaluate()
  */
  TEST_FIXTURE(ActivityModelTest, ActivityModelDebyeHuckel_OH) {
    set_activity_model_name(ac::ActivityModelFactory::debye_huckel);
    double gamma;
    RunTest("OH-", &gamma);
    CHECK_CLOSE(0.855, gamma, 1.0e-2);
  }

  TEST_FIXTURE(ActivityModelTest, ActivityModelDebyeHuckel_Ca) {
    set_activity_model_name(ac::ActivityModelFactory::debye_huckel);
    double gamma;
    RunTest("Ca++", &gamma);
    CHECK_CLOSE(0.57, gamma, 1.0e-2);
  }

  TEST_FIXTURE(ActivityModelTest, ActivityModelDebyeHuckel_SO4) {
    set_activity_model_name(ac::ActivityModelFactory::debye_huckel);
    double gamma;
    RunTest("SO4--", &gamma);
    CHECK_CLOSE(0.545, gamma, 1.0e-2);
  }

  TEST_FIXTURE(ActivityModelTest, ActivityModelDebyeHuckel_Al) {
    set_activity_model_name(ac::ActivityModelFactory::debye_huckel);
    double gamma;
    RunTest("Al+++", &gamma);
    CHECK_CLOSE(0.325, gamma, 1.0e-2);
  }

  TEST_FIXTURE(ActivityModelTest, ActivityModelDebyeHuckel_PO4) {
    set_activity_model_name(ac::ActivityModelFactory::debye_huckel);
    double gamma;
    RunTest("PO4---", &gamma);
    CHECK_CLOSE(0.25, gamma, 1.0e-2);
  }
}  // end SUITE(amanzi_chemistry_unit_tests_ActivityModelDebyeHuckel)
