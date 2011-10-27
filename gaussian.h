#ifndef GAUSSIAN_H
#define GAUSSIAN_H

#include <math.h>

#include <limits>
#ifndef INF
#define INF (std::numeric_limits<float>::infinity())
#endif

#define DIVISIONS 256

class Gaussian {
  public:
    Gaussian(float sigma_) : sigma(sigma_) {
	alpha = 0.5f/sigma;
    }
	
    // sample the gaussian using a cubic bezier approximation
    inline float sample(float x) const {
	x *= alpha;
	if (x < -2) {
	    return 0;
	}
	if (x < -1) {
	    x += 2;
	    return x * x * x;
	}
	if (x < 0) {
	    return (4 - 3*x*x*(2 + x));
	}
	if (x < 1) {
	    return (4 - 3*x*x*(2 - x));
	}
	if (x < 2) {
	    x = 2-x;
	    return x * x * x;
	}
	return 0;
    }

    // sample the gaussian using x^2 as the argument instead of x
    // uses a different 
    inline float sampleSquared(float x2) {
	x2 *= alpha * alpha;
	return expf(-2*x2);
    }

    // sample the integral of a gaussian, computed by analytically
    // integrating the cubic bezier curve used to sample the gaussian
    inline float sampleCDF(float x) const {
	x *= alpha;
	if (x < -2) {
	    return 0;
	}
	if (x < -1) {
	    x += 2;
	    x *= x;
	    x *= x;
	    return x;
	}
	if (x < 0) {
	    return 12 + x*(16 - x*x*(8 + 3*x));
	}
	if (x < 1) {
	    return 12 + x*(16 - x*x*(8 - 3*x));
	}
	if (x < 2) {
	    x = x-2;
	    x *= x;
	    x *= x;
	    return -x + 24;	    
	}
	return 24;
    }

    inline float getSigma() const {
	return sigma;
    }

  private:

    float alpha, sigma;
};

#endif
