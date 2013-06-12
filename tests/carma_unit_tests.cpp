//
//  carma_unit_tests.cpp
//  carma_pack
//
//  Created by Brandon C. Kelly on 6/10/13.
//  Copyright (c) 2013 Brandon Kelly. All rights reserved.
//

#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include "carmcmc.hpp"
#include "carpack.hpp"
#include <armadillo>
#include <boost/math/distributions/normal.hpp>

// Files containing simulated CAR(1) and CAR(5) time series, used for testing
std::string car1file("data/car1_test.dat");
std::string car5_file("data/car5_test.dat");

/*******************************************************************
                        TESTS FOR CAR1 CLASS
 *******************************************************************/

TEST_CASE("CAR1/constructor", "Make sure constructor sorts the time vector and removes duplicates.") {
    int ny = 100;
    arma::vec time0 = arma::linspace<arma::vec>(0.0, 100.0, ny);
    arma::vec y0 = arma::randn<arma::vec>(ny);
    arma::vec ysig = arma::zeros<arma::vec>(ny);

    // swap two elements so that time is out of order
    arma::vec time = time0;
    arma::vec y = y0;
    time(43) = time0(12);
    y(43) = y0(12);
    time(12) = time0(43);
    y(12) = y0(43);

    CAR1 car1_unordered(true, "CAR(1) - 1", time, y, ysig);
    
    // make sure CAR1 constructor sorted the time values
    time = car1_unordered.GetTime();
    REQUIRE(time(43) == time0(43));
    REQUIRE(time(12) == time0(12));
    arma::vec ycent = car1_unordered.GetTimeSeries();
    double ymean = arma::mean(y0);
    double frac_diff = std::abs(ycent(43) + ymean - y0(43)) / std::abs(y0(43));
    REQUIRE(frac_diff < 1e-8);
    frac_diff = std::abs(ycent(12) + ymean - y0(12)) / std::abs(y0(12));
    REQUIRE(frac_diff < 1e-8);
    
    // duplicate one of the elements of time
    time(43) = time(42);
    
    CAR1 car1_duplicate(true, "CAR(1) - 2", time, y, ysig);
    
    // make sure CAR1 constructor removed the duplicate value
    time = car1_duplicate.GetTime();
    REQUIRE(time.size() == (ny-1));
    REQUIRE(time(43) == time0(44)); // removed 43rd element from time vector
    ycent = car1_duplicate.GetTimeSeries();
    frac_diff = std::abs(ycent(43) + ymean - y0(44)) / std::abs(y0(44));
    REQUIRE(frac_diff < 1e-8);
}

TEST_CASE("CAR1/logpost_test", "Make sure the that CAR1.logpost_ == Car1.GetLogPost(theta) after running MCMC sampler") {
    int ny = 100;
    arma::vec time = arma::linspace<arma::vec>(0.0, 100.0, ny);
    arma::vec y = arma::randn<arma::vec>(ny);
    arma::vec ysig = 0.01 * arma::ones(ny);
    
    CAR1 car1_test(true, "CAR(1)", time, y, ysig);
    double max_stdev = 10.0 * arma::stddev(y); // For prior: maximum standard-deviation of CAR(1) process
    car1_test.SetPrior(max_stdev);
    
    // setup Robust Adaptive Metropolis step object
    StudentProposal tUnit(8.0, 1.0);
    arma::mat prop_covar(3,3);
    prop_covar.eye();
    int niter = 1000;
    double target_rate = 0.4;
    AdaptiveMetro RAM(car1_test, tUnit, prop_covar, target_rate, niter+1);
    RAM.Start();
    
    // perform a bunch of steps, which will update the car1_test.value_ and car1_test.log_posterior_ values.
    int logpost_neq_count = 0;
    for (int i=0; i<niter; i++) {
        RAM.DoStep();
        double logdens_stored = car1_test.GetLogDensity(); // stored value of log-posterior for current theta
        double logdens_computed = car1_test.LogDensity(car1_test.Value()); // explicitly calculate log-posterior for current theta
        if (std::abs(logdens_computed - logdens_stored) > 1e-10) {
            logpost_neq_count++; // count the number of time the two log-posterior values do not agree
        }
    }
    // make sure that saved logdensity is always equal to LogDensity(theta) for current thera value
    REQUIRE(logpost_neq_count == 0);
}

TEST_CASE("CAR1/prior_bounds", "Make sure CAR1::LogDensity returns -infinty when prior bounds are violated") {
    int ny = 100;
    arma::vec time = arma::linspace<arma::vec>(0.0, 100.0, ny);
    arma::vec y = arma::randn<arma::vec>(ny);
    arma::vec ysig = 0.01 * arma::ones(ny);
    
    CAR1 car1_test(true, "CAR(1)", time, y, ysig);
    double max_stdev = 10.0 * arma::stddev(y); // For prior: maximum standard-deviation of CAR(1) process
    car1_test.SetPrior(max_stdev);

    // prior bounds on omega
    double max_freq = 10.0;
	double min_freq = 1.0 / (10.0 * time.max());

    arma::vec bad_theta(3); // parameter value will violated the prior bounds
    double sigma = max_stdev / 10.0;
    double measerr_scale = 1.0;
    double omega = 2.0 * max_freq;
    bad_theta << sigma << measerr_scale << log(omega);
    
    double logpost = car1_test.LogDensity(bad_theta);
    REQUIRE(logpost == -1.0 * arma::datum::inf);
    
    omega = min_freq / 2.0;
    bad_theta(2) = log(omega);
    logpost = car1_test.LogDensity(bad_theta);
    REQUIRE(logpost == -1.0 * arma::datum::inf);
    
    omega = 1.0;
    sigma = -1.0;
    bad_theta(0) = sigma;
    bad_theta(2) = log(omega);
    logpost = car1_test.LogDensity(bad_theta);
    REQUIRE(logpost == -1.0 * arma::datum::inf);

    sigma = 100.0 * max_stdev;
    bad_theta(0) = sigma;
    logpost = car1_test.LogDensity(bad_theta);
    REQUIRE(logpost == -1.0 * arma::datum::inf);

    sigma = 1.0;
    bad_theta(0) = sigma;
    measerr_scale = 0.1;
    bad_theta(1) = measerr_scale;
    logpost = car1_test.LogDensity(bad_theta);
    REQUIRE(logpost == -1.0 * arma::datum::inf);

    measerr_scale = 4.0;
    bad_theta(1) = measerr_scale;
    logpost = car1_test.LogDensity(bad_theta);
    REQUIRE(logpost == -1.0 * arma::datum::inf);
}

TEST_CASE("CAR1/kalman_filter", "Test the Kalman Filter") {
    // first grab the simulated Gaussian CAR(1) data set
    arma::mat car1_data;
    car1_data.load(car1file, arma::raw_ascii);
    
    arma::vec time = car1_data.col(0);
    arma::vec y = car1_data.col(1);
    arma::vec yerr = car1_data.col(2);
    
    // CAR(1) process parameters
    double tau = 100.0;
    double omega = 1.0 / tau;
    double sigmay = 2.3;
    double sigma = sigmay * sqrt(2.0 / tau);
    double measerr_scale = 1.0;
    arma::vec theta(3);
    theta << sigma << measerr_scale << log(omega);
    
    CAR1 car1_process(true, "CAR(1)", time, y, yerr);
    
    // Compute and grab the kalman filter
    car1_process.KalmanFilter(theta);
    arma::vec kmean = car1_process.GetKalmanMean();
    arma::vec kvar = car1_process.GetKalmanVariance();
    
    // Compute the standardized residuals of the time series
    arma::vec sresid = (kmean - y) / arma::sqrt(kvar + yerr % yerr);
    
    // First do simple test on mean and variance of standardized residuals
    REQUIRE(std::abs(arma::mean(sresid)) < 3.0 / sqrt(sresid.n_elem));
    REQUIRE(std::abs(arma::var(sresid) - 1.0) < 3.0 * sqrt(2.0 * sresid.n_elem) / sresid.n_elem);
    
    // Test that the standardized residuals are consistent with having a standard normal distribution using
    // the Anderson-Darling test statistic
    arma::vec sorted_sresid = arma::sort(sresid);
    boost::math::normal snorm;
    arma::vec snorm_cdf(sresid.n_elem);
    for (int i=0; i<snorm_cdf.n_elem; i++) {
        // compute the standard normal CDF of the standardized residuals
        snorm_cdf(i) = boost::math::cdf(snorm, sorted_sresid(i));
    }
    double AD_stat = -snorm_cdf.n_elem;
    for (int i=0; i<snorm_cdf.n_elem; i++) {
        // compute the Anderson-Darling statistic
        AD_stat -= 1.0 / snorm_cdf.n_elem * ((2.0 * (i+1) - 1) * log(snorm_cdf(i)) +
                                             (2.0 * (snorm_cdf.n_elem - (i+1)) + 1.0) * log(1.0 - snorm_cdf(i)));
    }
    REQUIRE(AD_stat < 3.857); // critical value for 1% significance level
    
    // Now test that the autocorrelation function of the standardized residuals is consistent with a white noise process
    arma::vec acorr_sresid = arma::cor(sresid);
    std::cout << "Acorr size: " << acorr_sresid.n_elem << std::endl;
    acorr_sresid.print();
    
    // Finally, test that the autocorrelation function of the square of the residuals is consistent with a white noise process
}