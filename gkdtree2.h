/*************************************************************************/
/* Depth upsampling from sparse data                                     */
/*                                                                       */
/* (If you use this code, please cite the papers below:)                 */
/*                                                                       */
/* For the paper, please see:                                            */
/*  Dolson, J., Baek, J., Plagmann, C. and Thrun, S.,                    */
/*   ``Upsampling Range Data in Dynamic Environments,'' In CVPR 2010.    */
/*                                                                       */
/* This is an adaptation of the CPU-based gaussian kd-tree filtering     */
/* algorithm by Adams, A., Gelfand, N., Dolson, J., and Levoy, M.        */
/*  ``Gaussian KD-Trees for Fast High-Dimensional Filtering''            */
/*   In SIGGRAPH 2009.                                                   */
/*                                                                       */
/*                                                                       */
/*                                                                       */
/*                                                                       */
/*                                                                       */
/* All rights reserved.                                                  */
/*************************************************************************/



#ifndef GKDTREE2_H 
#define GKDTREE2_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include <limits>
#ifndef INF
#define INF (std::numeric_limits<float>::infinity())
#endif

#define RAND_FLOAT ((double)(rand()) / RAND_MAX)
#include "Image.h"
#include "gaussian.h"

using namespace std;

void doNothing(...) {}

class GKDTree {
  public:

  //joint bilateral with 1D values:
  static Image filter_jointBilateral(Image im_slicer, float **points, float *points_depth, int num_channels, int num_points, float accuracy) {
		
		//before building, copy points array, because the act of building shifts the pointers to the points around
		float** points_fixed = new float *[num_points];
		for(int x = 0; x < num_points; x++){
			points_fixed[x] = points[x];		
		} 

		printf("Building...\n");

    //changing last parameter to 0 forces one leaf node per input data point 
		GKDTree tree(num_channels, points, num_points, 0.15); 

		tree.finalize();

		printf("Splatting...\n");

		int SPLAT_ACCURACY = 4 + (int)(accuracy*64);
		int BLUR_ACCURACY = 16 + (int)(accuracy*128);
		int SLICE_ACCURACY = 12 + (int)(accuracy*64);

    const float SPLAT_STD_DEV = 0.30156;
		const float BLUR_STD_DEV = 0.9045;
		const float SLICE_STD_DEV = 0.30156;

		int *indices = new int[BLUR_ACCURACY];
		float *weights = new float[BLUR_ACCURACY];
		float *values = new float[tree.getLeaves() * (2)];  
		float *tmpValues = new float[tree.getLeaves() * (2)];
		memset(values, 0, sizeof(float)*tree.getLeaves()*(2));
		memset(tmpValues, 0, sizeof(float)*tree.getLeaves()*(2));

		//iterate through the points instead
		//values come from depth points array	
		for(int p = 0; p < num_points; p++){ 
	    int results = tree.gaussianLookup(points_fixed[p], SPLAT_STD_DEV, indices, weights, SPLAT_ACCURACY); 
		    for (int i = 0; i < results; i++) {
					float w = weights[i];
					float *vPtr = values + indices[i]*(2); 
					vPtr[0] += points_depth[p]*w;
					vPtr[1] += w;
		   	}
	   }
		
    
		printf("Blurring...\n");
    tree.blur(BLUR_STD_DEV, values, tmpValues, 2, BLUR_ACCURACY);
		float *tmp = tmpValues;
		tmpValues = values;
		values = tmp; 
		
		printf("Slicing...\n");  
		Image out_depth(1, im_slicer.width, im_slicer.height, 1); //slice out a depth image
	
		float *outPtr = out_depth(0, 0, 0); //result is a 1D depth image
		float *refPtr = im_slicer(0, 0, 0); //ref is an rgbxytime image
	
	  for (int y = 0; y < out_depth.height; y++) {
			for (int x = 0; x < out_depth.width; x++) {
		 	  int results = tree.gaussianLookup(refPtr, SLICE_STD_DEV, indices, weights, SLICE_ACCURACY); //slice out by the image frame
				float totalWeight = 0;
		  	for (int i = 0; i < results; i++) {
					float w = weights[i];
					float *vPtr = values + indices[i]*(2);
					outPtr[0] += vPtr[0]*w;
					totalWeight += vPtr[1]*w;
		    }
				if(totalWeight != 0){
					outPtr[0] /= totalWeight;
		    }
				else{
						outPtr[0]= 0;					
				}
		    refPtr += im_slicer.channels;
		    outPtr ++;
			}
	   }
		
		delete[] values;
		delete[] tmpValues;
		delete[] indices;
		delete[] weights;	

		return out_depth;
	
  };


  //joint bilateral where data is only sliced out in areas of high confidence
  static Image filter_jointBilateralConf(Image im_slicer, float **points, float *points_depth, int num_channels, int num_points, float accuracy, float conf_thresh) {
		
		//before building, copy points array, because the act of building shifts the pointers to the points around
		float** points_fixed = new float *[num_points];
		for(int x = 0; x < num_points; x++){
			points_fixed[x] = points[x];		
		} 

		printf("Building...\n");

		GKDTree tree(num_channels, points, num_points, 0.0); //changing to 0 forces one leaf node per input data point (was .15)

		tree.finalize();

		printf("Splatting...\n");

		int SPLAT_ACCURACY = 4 + (int)(accuracy*64);
		int BLUR_ACCURACY = 16 + (int)(accuracy*128);
		int SLICE_ACCURACY = 12 + (int)(accuracy*64);

    const float SPLAT_STD_DEV = 0.30156;
		const float BLUR_STD_DEV = 0.9045;
		const float SLICE_STD_DEV = 0.30156;

		int *indices = new int[BLUR_ACCURACY];
		float *weights = new float[BLUR_ACCURACY];
		float *values = new float[tree.getLeaves() * (2)];  
		float *tmpValues = new float[tree.getLeaves() * (2)];
		memset(values, 0, sizeof(float)*tree.getLeaves()*(2));
		memset(tmpValues, 0, sizeof(float)*tree.getLeaves()*(2));

		//iterate through the points instead of ref
		//values from depth points array instead of image		
		for(int p = 0; p < num_points; p++){ 
	    int results = tree.gaussianLookup(points_fixed[p], SPLAT_STD_DEV, indices, weights, SPLAT_ACCURACY); 
		    for (int i = 0; i < results; i++) {
					float w = weights[i];
		  		float *vPtr = values + indices[i]*(2); 
					vPtr[0] += points_depth[p]*w;
					vPtr[1] += w;
		   	}
	   }
		
    
		printf("Blurring...\n");
    tree.blur(BLUR_STD_DEV, values, tmpValues, 2, BLUR_ACCURACY);
		float *tmp = tmpValues;
		tmpValues = values;
		values = tmp; 

		printf("Slicing...\n");  
		Image out_depth(1, im_slicer.width, im_slicer.height, 1); //slice out a depth image
	
		float *outPtr = out_depth(0, 0, 0); //result is a 1D depth image
		float *refPtr = im_slicer(0, 0, 0); //ref is an rgbxytime image
	
	  for (int y = 0; y < out_depth.height; y++) {
			for (int x = 0; x < out_depth.width; x++) {
		 	  int results = tree.gaussianLookup(refPtr, SLICE_STD_DEV, indices, weights, SLICE_ACCURACY); //slice out by the image frame
				float totalWeight = 0;
		   	for (int i = 0; i < results; i++) {
					float w = weights[i];
					float *vPtr = values + indices[i]*(2);
					outPtr[0] += vPtr[0]*w;
					totalWeight += vPtr[1]*w;
		    }
				if(totalWeight > conf_thresh){
					outPtr[0] /= totalWeight;
		    }
				else{
						outPtr[0]= 0;					
				}
		    refPtr += im_slicer.channels;
		    outPtr ++;
			}
	   }
		
		delete[] values;
		delete[] tmpValues;
		delete[] indices;
		delete[] weights;	

		return out_depth;
	
  };

  //visualization of confidence (sum of weights at each query point) 
  static Image filter_weightVis(Image im_slicer, float **points, float *points_depth, int num_channels, int num_points, float accuracy) {
		
		//before building, copy points array, because the act of building shifts the pointers to the points around
		float** points_fixed = new float *[num_points];
		for(int x = 0; x < num_points; x++){
			points_fixed[x] = points[x];		
		} 

		printf("Building...\n");

		GKDTree tree(num_channels, points, num_points, 0.0); //changing to 0 forces one leaf node per input data point (was .15)

		tree.finalize();

		printf("Splatting...\n");

		int SPLAT_ACCURACY = 4 + (int)(accuracy*64);
		int BLUR_ACCURACY = 16 + (int)(accuracy*128);
		int SLICE_ACCURACY = 12 + (int)(accuracy*64);

    const float SPLAT_STD_DEV = 0.30156;
		const float BLUR_STD_DEV = 0.9045;
		const float SLICE_STD_DEV = 0.30156;

		int *indices = new int[BLUR_ACCURACY];
		float *weights = new float[BLUR_ACCURACY];
		float *values = new float[tree.getLeaves() * (2)];  
		float *tmpValues = new float[tree.getLeaves() * (2)];
		memset(values, 0, sizeof(float)*tree.getLeaves()*(2));
		memset(tmpValues, 0, sizeof(float)*tree.getLeaves()*(2));

		//iterate through the points instead of ref
		//values from depth points array instead of image		
		for(int p = 0; p < num_points; p++){ 
	    int results = tree.gaussianLookup(points_fixed[p], SPLAT_STD_DEV, indices, weights, SPLAT_ACCURACY); 
		    for (int i = 0; i < results; i++) {
					float w = weights[i];
					//printf("indices[i] %d i %d\n", indices[i], i);
					float *vPtr = values + indices[i]*(2); 
					vPtr[0] += points_depth[p]*w;
					vPtr[1] += w;
		   	}
	   }
		
    
		printf("Blurring...\n");
    tree.blur(BLUR_STD_DEV, values, tmpValues, 2, BLUR_ACCURACY);
		float *tmp = tmpValues;
		tmpValues = values;
		values = tmp; 

		printf("Slicing...\n");  
		Image out_depth(1, im_slicer.width, im_slicer.height, 1); //slice out a depth image
	
		float *outPtr = out_depth(0, 0, 0); //result is a 1D depth image
		float *refPtr = im_slicer(0, 0, 0); //ref is an rgbxytime image
	
	  for (int y = 0; y < out_depth.height; y++) {
			for (int x = 0; x < out_depth.width; x++) {
		 	  int results = tree.gaussianLookup(refPtr, SLICE_STD_DEV, indices, weights, SLICE_ACCURACY); //slice out by the image frame
				for (int i = 0; i < results; i++) {
					float w = weights[i];
					outPtr[0] += w;
		    }
		    refPtr += im_slicer.channels;
		    outPtr ++;
			}
	   }
		
		delete[] values;
		delete[] tmpValues;
		delete[] indices;
		delete[] weights;	

		return out_depth;
	
  };




  // Build a gkdtree using the supplied array of points to control
  // the sampling.

  // sizeBound specifies the maximum allowable side length of a
  // kdtree leaf.

  // At least one point from data lies in any given leaf.

  GKDTree(int dims, float **data, int nData, float sBound) : 
	dimensions(dims), sizeBound(sBound), leaves(0) {

		// Allocate space to store a data bounding box while we build
		// the tree 
		dataMins = new float[dims];
		dataMaxs = new float[dims];

		root = build(data, nData);	

		gaussian = NULL;

		delete dataMins;
		delete dataMaxs;
	}       	
	
  ~GKDTree() {
		delete root;
   }

  void include(float *value) {
		root = root->include(value, sizeBound, &leaves);
  }

  void finalize() {
		float *kdtreeMins = new float[dimensions];
		float *kdtreeMaxs = new float[dimensions];

		for (int i = 0; i < dimensions; i++) {
	    kdtreeMins[i] = -INF;
	    kdtreeMaxs[i] = +INF;
		}
	
		root->computeBounds(kdtreeMins, kdtreeMaxs);
	
		delete kdtreeMins;
		delete kdtreeMaxs;		

		printf("Constructed a gkdtree with %i leaves\n", leaves);	
  }

  int getLeaves() {
		return leaves;
  }

  int getDimensions() {
		return dimensions;
  }

  // Compute a gaussian spread of kdtree leaves around the given
  // point. This is the general case sampling strategy.
  int gaussianLookup(float *value, float sigma, int *ids, float *weights, int nSamples) {
		return root->gaussianLookup(value, makeGaussian(sigma), &ids, &weights, nSamples, 1);
  }

  // compute which kdtree cell a given point lies within. Equivalent
  // to a gaussian lookup of sigma zero
  int nearestLookup(float *value) {
		return root->nearestLookup(value);
  }

  // assign some data to each leaf node and do a gaussian blur
  void blur(float sigma, float *oldValues, float *newValues, int dataDimensions, int nSamples) {
		int *ids = new int[nSamples];
		float *weights = new float[nSamples];	

		memset(newValues, 0, sizeof(float)*leaves*dataDimensions);
		root->blur(this, sigma, oldValues, newValues, dataDimensions, ids, weights, nSamples);

		delete ids;
		delete weights;
  }
	
	void blur_threshold(float sigma, float *oldValues, float *newValues, int dataDimensions, int nSamples, float blurThresh) {
		int *ids = new int[nSamples];
		float *weights = new float[nSamples];	

		memset(newValues, 0, sizeof(float)*leaves*dataDimensions);
		root->blur_threshold(this, sigma, oldValues, newValues, dataDimensions, ids, weights, nSamples, blurThresh);

		delete ids;
		delete weights;
  }
	


  private:

  	Gaussian *makeGaussian(float sigma) {
			if (gaussian) {
	    	if (gaussian->getSigma() != sigma) {
					delete gaussian;
					gaussian = new Gaussian(sigma);
	    	}
			} else {
	    	gaussian = new Gaussian(sigma);
			}
			return gaussian;
    }

    class Node {	
      public:
				virtual ~Node() {}

				// Returns a list of samples from the kdtree distributed
				// around value with std-dev sigma in all dimensions. Some
				// samples may be repeated. Returns how many entries in the
				// ids and weights arrays were used.
				virtual int gaussianLookup(float *value, Gaussian *gaussian, int **ids, float **weights, int nSamples, float p) = 0;

				// special case optimization of the above where nsamples = 1
				virtual int singleGaussianLookup(float *value, Gaussian *gaussian, int **ids, float **weights, float p) = 0;

				// Same as the above if the gaussian has std dev 0
				virtual int nearestLookup(float *value) = 0;

				virtual void blur(GKDTree *tree, float sigma, float *oldValues, float *newValues, int dataDimensions, int *ids, float *weights, int nSamples) = 0;

				virtual void blur_threshold(GKDTree *tree, float sigma, float *oldValues, 
																		float *newValues, int dataDimensions, int *ids, float *weights, int nSamples, float blurThresh) = 0;

				virtual Node *include(float *value, float sizeBound, int *leaves) = 0;

				virtual void computeBounds(float *mins, float *maxs) = 0;

   };
    
    class Split : public Node {
      public:
				virtual ~Split() {
	    		delete left;
	    		delete right;
				}


				// for a given gaussian and a given value, the probability of splitting left at this node
				inline float pLeft(Gaussian *gaussian, float value) {	    
						// Coarsely approximate the cumulative normal distribution 
						float val = gaussian->sampleCDF(cut_val - value);
						float minBound = gaussian->sampleCDF(min_val - value);
						float maxBound = gaussian->sampleCDF(max_val - value);	    
						return (val - minBound) / (maxBound - minBound);	    
				}

				int gaussianLookup(float *value, Gaussian *gaussian, int **ids, float **weights, int nSamples, float p) {
					// Calculate how much of a gaussian ball of radius sigma,
					// that has been trimmed by all the cuts so far, lies on
					// each side of the split

					// compute the probability of a sample splitting left
					float val = pLeft(gaussian, value[cut_dim]);

					// Send some samples to the left of the split
					int leftSamples = (int)(val*nSamples);

					// Send some samples to the right of the split
					int rightSamples = (int)((1-val)*nSamples);
					
					// There's probably one sample left over by the rounding
					if (leftSamples + rightSamples != nSamples) {
						float fval = val*nSamples - rightSamples;
						// if val is high we send it right, if val is low we send it left
						if (RAND_FLOAT < fval) {
							leftSamples++;
						} else {
							rightSamples++;
						}
					}	    

			  	int samplesFound = 0;
			  	// Get the left samples
			  	if (leftSamples > 0) {
						if (leftSamples > 1) {
				  		samplesFound += left->gaussianLookup(value, gaussian, ids, weights, leftSamples, p*val);
						} else {
				  		samplesFound += left->singleGaussianLookup(value, gaussian, ids, weights, p*val);
						}
			  	}

			  // Get the right samples
	    	if (rightSamples > 0) {
					if (rightSamples > 1) {
		    		samplesFound += right->gaussianLookup(value, gaussian, ids, weights, rightSamples, p*(1-val));
					} else {
		    		samplesFound += right->singleGaussianLookup(value, gaussian, ids, weights, p*(1-val));
					}
	    	}	    	    

	    	return samplesFound;
			}
	
			// a special case optimization of the above for when nSamples is 1
			int singleGaussianLookup(float *value, Gaussian *gaussian, int **ids, float **weights, float p) {
				float val = pLeft(gaussian, value[cut_dim]);
				if (RAND_FLOAT < val) {
					return left->singleGaussianLookup(value, gaussian, ids, weights, p*val);
				} else {
					return right->singleGaussianLookup(value, gaussian, ids, weights, p*(1-val));
				}
			}

		// which leaf does a given value lie within? Equivalent to a
		// gaussian lookup with std dev = 0
		int nearestLookup(float *value) {
			  if (value[cut_dim] < cut_val) return left->nearestLookup(value);
			  else return right->nearestLookup(value);
		}
	
		void blur(GKDTree *tree, float sigma, float *oldValues, float *newValues, int dataDimensions, int *ids, float *weights, int nSamples) {
	    left->blur(tree, sigma, oldValues, newValues, dataDimensions, ids, weights, nSamples);
	    right->blur(tree, sigma, oldValues, newValues, dataDimensions, ids, weights, nSamples);	    
		}

		void blur_threshold(GKDTree *tree, float sigma, float *oldValues, float *newValues, int dataDimensions, 
												int *ids, float *weights, int nSamples, float blurThresh) {
	    left->blur_threshold(tree, sigma, oldValues, newValues, dataDimensions, ids, weights, nSamples, blurThresh);
	    right->blur_threshold(tree, sigma, oldValues, newValues, dataDimensions, ids, weights, nSamples, blurThresh);	    
		}

		void computeBounds(float *mins, float *maxs) {
	    min_val = mins[cut_dim];
	    max_val = maxs[cut_dim];

	    maxs[cut_dim] = cut_val;
	    left->computeBounds(mins, maxs);
	    maxs[cut_dim] = max_val;

	    mins[cut_dim] = cut_val;		
	    right->computeBounds(mins, maxs);
	    mins[cut_dim] = min_val;
		}

		Node *include(float *value, float sizeBound, int *leaves) {
	    /*
	    if (value[cut_dim] < min_val || value[cut_dim] > max_val) {
		printf("A node for inclusion was sent down the wrong path!\n");
		printf("%f %f %f\n", min_val, value[cut_dim], max_val);
	    }
	    */

	    if (value[cut_dim] < cut_val) left = left->include(value, sizeBound, leaves);
	    else right = right->include(value, sizeBound, leaves);
	    return this;
		}

		int cut_dim;
		float cut_val, min_val, max_val;
		Node *left, *right;
 	};
    
  class Leaf : public Node {
     public:
			Leaf(int id_, float **data, int nData, int dimensions_) 
	    : id(id_), dimensions(dimensions_) {
			  position = new float[dimensions];
			  for (int i = 0; i < dimensions; i++) {
					position[i] = 0;
						for (int j = 0; j < nData; j++) {
				  		position[i] += data[j][i];		    
						}
					position[i] /= nData;
			  }
			}
	    
		~Leaf() {
	    delete position;
		}
	
		int gaussianLookup(float *query, Gaussian *g, int **ids, float **weights, int nSamples, float p) {
	    // p is the probability with which one sample arrived here
	    // calculate the correct probability, q
	    
	    // TODO: sse?
	    float q = 0;
	    for (int i = 0; i < dimensions; i++) {
				float diff = query[i] - position[i];
				diff *= diff;
				q += diff;
	    }

	    q = g->sampleSquared(q);

	    *(*ids)++ = id;
	    *(*weights)++ = nSamples * q / p;
	    
	    return 1;
		}
	
		int singleGaussianLookup(float *query, Gaussian *g, int **ids, float **weights, float p) {
	    return gaussianLookup(query, g, ids, weights, 1, p);
		}
	
		int nearestLookup(float *) {
	    return id;
		}
	
		void blur(GKDTree *tree, float sigma, float *oldValues, float *newValues, int dataDimensions, int *ids, float *weights, int nSamples) {
	    // do a gaussian gather	    
	    int results = tree->gaussianLookup(position, sigma, ids, weights, nSamples);
	    int newIdx = id*dataDimensions;
	    for (int i = 0; i < results; i++) {
				int oldIdx = ids[i]*dataDimensions;
				for (int d = 0; d < dataDimensions; d++) {
		  	  newValues[newIdx+d] += weights[i] * oldValues[oldIdx+d];
				}
	    }
	    
		}

		void blur_threshold(GKDTree *tree, float sigma, float *oldValues, float *newValues, 
												int dataDimensions, int *ids, float *weights, int nSamples, float blurThresh) {
	    // do a gaussian gather	    
	    int results = tree->gaussianLookup(position, sigma, ids, weights, nSamples);
	    int newIdx = id*dataDimensions;
	    for (int i = 0; i < results; i++) {
				int oldIdx = ids[i]*dataDimensions;
				for (int d = 0; d < dataDimensions; d++) {
					float oldValue = oldValues[oldIdx+d];
					if(abs((int)(oldValue - oldValues[newIdx+d])) < blurThresh){
		  	  	newValues[newIdx+d] += weights[i] * oldValue;
					}
				}
	    }
	    
		}

		Node *include(float *value, float sizeBound, int *leaves) {
	    float d = 0;
	    for (int i = 0; i < dimensions; i++) {
				float diff = value[i] - position[i];
				diff *= diff;
				d += diff;
	    }
	    if (d > sizeBound * sizeBound) {
			/*
			printf("This value:\n");
			for (int i = 0; i < dimensions; i++) {
				  printf("%f ", value[i]);
			}
			printf("\nIs too far from this node:\n");
			for (int i = 0; i < dimensions; i++) {
				  printf("%f ", position[i]);
			}		
			printf("\n");
			*/
			// this value isn't well represented by this tree -
			// split up this node
			int longest = 0;
			d = (value[0] - position[0])*(value[0] - position[0]);
			for (int i = 0; i < dimensions; i++) {
				  float diff = value[i] - position[i];
				  diff *= diff;
				  if (diff > d) {
				longest = i;
				d = diff;
			}
		}

		Split *s = new Split;
		s->cut_dim = longest;
		s->cut_val = (position[longest] + value[longest])/2;
		//printf("So I'll make a new split on dimension %i at position %f\n", s->cut_dim, s->cut_val);
		s->min_val = -INF;
		s->max_val = INF;
		Leaf *l = new Leaf((*leaves)++, &value, 1, dimensions);
		if (position[longest] < s->cut_val) {
		    //printf("I go on the left, the new guy goes on the right\n");
		    s->left = this;
		    s->right = l;
		} else {
		    //printf("I go on the right, the new guy goes on the left\n");
		    s->left = l;
		    s->right = this;
		}
		return s;
	    } else {
		// this new value is already well represented in the tree
		return this;
	    }
	}

	void computeBounds(float *mins, float *maxs) {
	    /*
	    bool ok = true;
	    for (int i = 0; i < dimensions && ok; i++) {
		ok = (position[i] >= mins[i]) && (position[i] <= maxs[i]);
	    }
	    if (!ok) {
		printf("Corrupt kdtree:\n");
		printf("mins: ");
		for (int i = 0; i < dimensions; i++)
		    printf("%3.3f ", mins[i]);
		printf("\n");

		printf("position: ");
		for (int i = 0; i < dimensions; i++)
		    printf("%3.3f ", position[i]);
		printf("\n");

		printf("maxs: ");
		for (int i = 0; i < dimensions; i++)
		    printf("%3.3f ", maxs[i]);
		printf("\n");
	    }
	    */
	}
	
      private:
	int id, dimensions;
	float *position;
    };
    
    Node *root;
    int dimensions;
    float sizeBound;
    int leaves;

    float *dataMins, *dataMaxs;
    Gaussian *gaussian;

    Node *build(float **data, int nData) {
	if (nData == 1) {
	    return new Leaf(leaves++, data, nData, dimensions);
	} else {
	    // calculate the data bounds in every dimension
	    for (int i = 0; i < dimensions; i++) {
		dataMins[i] = dataMaxs[i] = data[0][i];
	    }
	    for (int j = 1; j < nData; j++) {
		for (int i = 0; i < dimensions; i++) {
		    if (data[j][i] < dataMins[i]) dataMins[i] = data[j][i];
		    if (data[j][i] > dataMaxs[i]) dataMaxs[i] = data[j][i];
		}
	    }

	    // find the longest dimension
	    int longest = 0;
	    float diagonal = 0;
	    for (int i = 1; i < dimensions; i++) {
		float delta = dataMaxs[i] - dataMins[i];
		diagonal += delta*delta;
		if (delta > dataMaxs[longest] - dataMins[longest]) 
		    longest = i;
	    }

	    // if it's large enough, cut in that dimension
	    if (diagonal > 4*sizeBound*sizeBound) {
		Split *n = new Split;
		n->cut_dim = longest;
		n->cut_val = (dataMaxs[longest] + dataMins[longest])/2;

		// these get computed later		
		n->min_val = -INF; 
		n->max_val = INF; 
		
		// resort the input over the split
		int pivot = 0;
		for (int i = 0; i < nData; i++) {
		    // The next value is larger than the pivot
		    if (data[i][longest] >= n->cut_val) continue;
		    
		    // We haven't seen anything larger than the pivot yet
		    if (i == pivot) {
			pivot++;
			continue;
		    }
		
		    // The current value is smaller than the pivot
		    float *tmp = data[i];
		    data[i] = data[pivot];
		    data[pivot] = tmp;
		    pivot++;
		}

		// Build the two subtrees
		n->left = build(data, pivot);
		n->right = build(data+pivot, nData-pivot);

		return n;
	    } else { 
		return new Leaf(leaves++, data, nData, dimensions);
	    }
	}
    };


};


#endif
