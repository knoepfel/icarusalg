/**
 * @file   SampledFunction_test.cc
 * @brief  Unit test for `util::SampledFunction`.
 * @date   February 14, 2020
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @see    `icarusalg/Utilities/SampledFunction.h
 */

// Boost libraries
#define BOOST_TEST_MODULE SampledFunction
#include <boost/test/unit_test.hpp>
namespace tt = boost::test_tools;

// ICARUS libraries
#include "icarusalg/Utilities/SampledFunction.h"

// LArSoft libraries
#include "larcorealg/CoreUtils/counter.h"


//------------------------------------------------------------------------------
template <typename T, typename U>
constexpr T constexpr_floor(U num) {
  
  // goes to closest smaller positive integer or larger negative integer
  T const inum = static_cast<T>(num);
  if (num == static_cast<float>(inum)) return inum;
  
  return (num > U{ 0 })? inum: inum - T{ 1 };
} // constexpr_floor()

static_assert(constexpr_floor<int>(0) == 0);

static_assert(constexpr_floor<int>(-2.00) == -2);
static_assert(constexpr_floor<int>(-1.75) == -2);
static_assert(constexpr_floor<int>(-1.50) == -2);
static_assert(constexpr_floor<int>(-1.25) == -2);
static_assert(constexpr_floor<int>(-1.00) == -1);
static_assert(constexpr_floor<int>(-0.75) == -1);
static_assert(constexpr_floor<int>(-0.50) == -1);
static_assert(constexpr_floor<int>(-0.00) == +0);
static_assert(constexpr_floor<int>(+0.00) == +0);
static_assert(constexpr_floor<int>(+0.50) == +0);
static_assert(constexpr_floor<int>(+0.75) == +0);
static_assert(constexpr_floor<int>(+1.00) == +1);
static_assert(constexpr_floor<int>(+1.25) == +1);
static_assert(constexpr_floor<int>(+1.50) == +1);
static_assert(constexpr_floor<int>(+1.75) == +1);
static_assert(constexpr_floor<int>(+2.00) == +2);

//------------------------------------------------------------------------------
void IdentityTest() {

  auto identity = [](double x){ return x; };

  // [ -2.0 , 6.0 ] with step size 0.5 and 4 subsamples (0.125 substep size)
  constexpr gsl::index nSamples = 16;
  constexpr gsl::index nSubsamples = 4;
  constexpr double min = -2.0;
  constexpr double max = min + static_cast<double>(nSamples / 2);
  constexpr double range = max - min;
  constexpr double step = range / nSamples;
  constexpr double substep = step / nSubsamples;


  BOOST_TEST_MESSAGE("Test settings:"
    << "\nRange:      " << min << " -- " << max << " (range: " << range << ")"
    << "\nSamples:    " << nSamples << " (size: " << step << ")"
    << "\nSubsamples: " << nSubsamples << " (size: " << substep << ")"
    );

  // function with 10 samples, sampled 4 times
  util::SampledFunction<> sampled { identity, min, max, nSamples, nSubsamples };

  //
  // Query
  //
  auto const close = tt::tolerance(1.e-6);
  BOOST_TEST(sampled.size() == nSamples);
  BOOST_TEST(sampled.nSubsamples() == nSubsamples);
  BOOST_TEST(sampled.lower() == min);
  BOOST_TEST(sampled.upper() == max);
  BOOST_TEST(sampled.rangeSize() == max - min, close);
  BOOST_TEST(sampled.stepSize() == step, close);
  BOOST_TEST(sampled.substepSize() == substep, close);

  for (auto const iSub: util::counter(nSubsamples)) BOOST_TEST_CONTEXT("Subsample: " << iSub)
  {

    double const subsampleStart = min + iSub * substep;

    auto const& subSample = sampled.subsample(iSub);
    BOOST_TEST_MESSAGE
      ("Subsample #" << iSub << ": " << subSample.size() << " samples");
    auto itSample = subSample.begin();

    for (auto const iSample: util::counter(-nSamples, 2*nSamples+1)) BOOST_TEST_CONTEXT("Sample: " << iSample)
    {
      bool const bInRange = (iSample >= 0) && (iSample < nSamples);

      double const expected_x
        = static_cast<double>(subsampleStart + iSample * step);
      double const expected_value = identity(expected_x); // I wonder how much

      if (bInRange) {
        BOOST_TEST(sampled.value(iSample, iSub) == expected_value);
        BOOST_TEST(*itSample == expected_value);
        BOOST_TEST_MESSAGE("[" << iSample << "] " << *itSample);
        ++itSample;
      }

      // check lookup from within the substep
      for (double const shift: { 0.0, 0.25, 0.50, 0.75 }) BOOST_TEST_CONTEXT("Shift: " << shift) {
        double const expected_x_in_the_middle = expected_x + shift * substep;

        gsl::index const stepIndex
          = sampled.stepIndex(expected_x_in_the_middle, iSub);

        BOOST_TEST(sampled.isValidStepIndex(stepIndex) == bInRange);
        BOOST_TEST(stepIndex == iSample);

        BOOST_TEST
          (sampled.closestSubsampleIndex(expected_x_in_the_middle) == iSub);

      } // for shift

    } // for all samples in the subsample

    BOOST_TEST((itSample == subSample.end()), "itSample != subSample.end()");

    BOOST_TEST(!sampled.isValidStepIndex
      (sampled.stepIndex(subsampleStart + max - min, iSub))
      );

  } // for all subsamples

} // void IdentityTest()


//------------------------------------------------------------------------------
void ExtendedRangeTest() {

  auto identity = [](double x){ return x; };

  // the following checks work for a monotonic function;
  // the value at x = stopAt should not be included in the function range.

  constexpr gsl::index nSubsamples = 4;
  constexpr double min = -2.0;
  constexpr double atLeast = +1.0;
  constexpr double stopBefore = +8.2; // deliberately avoid border effects
  constexpr double step = 0.5;
  constexpr double substep = step / nSubsamples;

  // this stop function does *not* include the stop value in the range
  // when that value would be the right limit of the range; for that to
  // be included, (y > stopValue) should be used.
  constexpr double stopValue = identity(stopBefore);
  auto const stopIf
    = [](double, double y){ return (y < 0) || (y >= stopValue); };

  // function with 10 samples, sampled 4 times
  util::SampledFunction sampled
    { identity, min, step, stopIf, nSubsamples, atLeast };

  constexpr gsl::index expected_nSamples
    = constexpr_floor<gsl::index>((stopBefore - min) / step);
  constexpr gsl::index expected_max = min + step * expected_nSamples;
  constexpr gsl::index expected_range = expected_max - min;

  //
  // Query
  //
  BOOST_TEST(sampled.nSubsamples() == nSubsamples);
  BOOST_TEST(sampled.lower() == min);
  BOOST_TEST(sampled.stepSize() == step, close);
  BOOST_TEST(sampled.substepSize() == substep, close);

  BOOST_TEST(sampled.upper() == expected_max, close);
  BOOST_TEST_REQUIRE(sampled.size() == expected_nSamples);
  BOOST_TEST(sampled.rangeSize() == expected_range, close);


  auto const nSamples = sampled.size();

  for (auto const iSub: util::counter(nSubsamples)) BOOST_TEST_CONTEXT("Subsample: " << iSub)
  {

    double const subsampleStart = min + iSub * substep;

    auto const& subSample = sampled.subsample(iSub);
    BOOST_TEST_MESSAGE
      ("Subsample #" << iSub << ": " << subSample.size() << " samples");
    auto itSample = subSample.begin();

    for (auto const iSample: util::counter(-nSamples, 2*nSamples+1)) BOOST_TEST_CONTEXT("Sample: " << iSample)
    {
      bool const bInRange = (iSample >= 0) && (iSample < nSamples);

      double const expected_x
        = static_cast<double>(subsampleStart + iSample * step);
      double const expected_value = identity(expected_x); // I wonder how much

      if (bInRange) {
        BOOST_TEST(sampled.value(iSample, iSub) == expected_value);
        BOOST_TEST(*itSample == expected_value);
        BOOST_TEST_MESSAGE("[" << iSample << "] " << *itSample);
        ++itSample;
      }

    } // for all samples in the subsample

    BOOST_TEST((itSample == subSample.end()), "itSample != subSample.end()");

    BOOST_TEST(!sampled.isValidStepIndex
      (sampled.stepIndex(subsampleStart + expected_max - min, iSub))
      );

  } // for all subsamples

} // void ExtendedRangeTest()


//------------------------------------------------------------------------------
//---  The tests
//---
BOOST_AUTO_TEST_CASE( TestCase ) {

  IdentityTest();
  ExtendedRangeTest();

} // BOOST_AUTO_TEST_CASE( TestCase )

