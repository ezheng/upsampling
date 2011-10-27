/*************************************************************************/
/* Depth upsampling from sparse data                                     */
/*                                                                       */
/* (If you use this code, please cite the papers below:)                 */
/*                                                                       */
/* For the paper, please see:                                            */
/*  Dolson, J., Baek, J., Plagmann, C. and Thrun, S.,                    */
/*   ``Upsampling Range Data in Dynamic Environments,'' In CVPR 2010.    */
/*                                                                       */
/* This is an adaptation of the gaussian kd-tree filtering               */
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

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <cmath>
#include <time.h>
#include "gkdtree2.h"

using namespace std;


Image Load(char* filename) {
  FILE *file = fopen(filename, "rb");
  if(!file){
    fprintf(stderr, "Could not open file %s\n", filename);  
  }
  // read the header
	int frames, width, height, channels;
	fread(&frames, sizeof(int), 1, file);
  fread(&width, sizeof(int), 1, file);
  fread(&height, sizeof(int), 1, file);
 // fread(&frames, sizeof(int), 1, file);
  fread(&channels, sizeof(int), 1, file);
  //--end header
	int size = width * height * channels * frames;
  //declare image and read in values
  //Image im(width, height, frames, channels);
	Image im(frames,width,height,channels);
  fread(im(0,0,0), sizeof(float), size, file);
	return im;
}


void Save(Image im, char* filename) {
  FILE *f = fopen(filename, "wb");
  if(!f){
    fprintf(stderr, "Could not write file %s\n", filename);  
  }
  //-- you can change the ordering/content of this header to be compatible ImageStack 
  //or other libraries or programs
  fwrite(&im.frames, sizeof(int), 1, f);
  fwrite(&im.width, sizeof(int), 1, f);
  fwrite(&im.height, sizeof(int), 1, f);
  fwrite(&im.channels, sizeof(int), 1, f);	
  //---end header
  int size = im.channels*im.width*im.height*im.frames;  
  //write the data
  fwrite(im(0, 0, 0), sizeof(float), size, f);
  fclose(f);
}



vector<Image> set_data(int numFrames, char **filenames, int *pt_count){

	vector<Image> images;

	//load all the rgbxytdepth .tmp data files that will be used to build the kd-tree
	for(int f = 1; f < numFrames; f++){ 
		images.push_back(Load(filenames[2+f]));
	}

	//count the points in this dataset
	int count = 0;
	for(int f= 0; f < (int)images.size(); f++){
		Image curr_im = images.at(f);
    printf("num %d width %d height %d frames %d channels %d \n", f, curr_im.width, curr_im.height, curr_im.frames, curr_im.channels);
		for(int y = 0; y < curr_im.height; y++){
			for(int x = 0; x < curr_im.width; x++){
				if(curr_im(x,y)[4] > 0.0){
					count++;				
				}
			}
		}	
	}

  printf("count %d\n", count);
  *pt_count = count;
  
	return images;
	
}


Image do_filter_rgb(int numFrames, char ** filenames, Image color_time_frame, float stdevX, float stdevY, float stdevColor, float stdevTime){ 

	//create the color + timestamp frame - to slice out with
	printf("creating slice frame (ref)\n");
	Image color_time_ref(1, color_time_frame.width, color_time_frame.height, 6);	//r,g,b,x,y,t
	
	float invStdevX = 1.0f/stdevX;
	float invStdevY = 1.0f/stdevY;
  float invColorStdev = 1.0f/stdevColor;
	float invTimeStdev = 1.0f/stdevTime;

	printf("building ref\n");
	for(int y = 0; y < color_time_frame.height; y++){
		for(int x = 0; x < color_time_frame.width; x++){
			color_time_ref(x,y)[0] = color_time_frame(x,y)[0] * invColorStdev; //r
			color_time_ref(x,y)[1] = color_time_frame(x,y)[1] * invColorStdev; //g
			color_time_ref(x,y)[2] = color_time_frame(x,y)[2] * invColorStdev; //b
			color_time_ref(x,y)[3] = (float)x * invStdevX; //x
			color_time_ref(x,y)[4] = (float)y * invStdevY; //y
			color_time_ref(x,y)[5] = color_time_frame(x,y)[3] * invTimeStdev; //t
			
		}
	}

	int pt_count = -1;
	vector<Image> rgbxytd_frames = set_data(numFrames, filenames, &pt_count);

	float** points = new float *[pt_count]; //create pointers to rgbxyt points
	float* 	depth_vals = new float[pt_count]; //create a corresponding array of depth values	
	
	//load points and adjust point values to reflect standard deviations:
	int count = 0;
	for(int f = 0; f < (int)rgbxytd_frames.size(); f++){
		Image curr_image = rgbxytd_frames.at(f);
		for(int y = 0; y < curr_image.height; y++){
			for(int x = 0; x < curr_image.width; x++){
				if(curr_image(x,y)[4] > 0.0){
					float *temp_pt = new float[6];
					temp_pt[0] = curr_image(x,y)[0] * invColorStdev; //r
					temp_pt[1] = curr_image(x,y)[1] * invColorStdev; //g
					temp_pt[2] = curr_image(x,y)[2] * invColorStdev; //b
					temp_pt[3] = (float)x *invStdevX; //x
					temp_pt[4] = (float)y *invStdevY; //y
					temp_pt[5] = curr_image(x,y)[3] * invTimeStdev; //t			
					points[count] = temp_pt;
					depth_vals[count] = curr_image(x,y)[4]; //set depth value
					count++;
				}			
			}
		}
	}

	if(count != pt_count){
		printf("WRONG count %d pt_count %d\n", count, pt_count);
		exit(1);
	}
	printf("number of points: %d\n", pt_count);
	Image im_depth = GKDTree::filter_jointBilateral(color_time_ref, points, depth_vals, 6, pt_count, 1);
	
	/*	
	printf("calculating confidence");	
	Image weight_vis = GKDTree::filter_weightVis(color_time_ref, points, depth_vals, 6, pt_count, 1);	
	Save::apply(weight_vis, "weight_vis.tmp");
	*/

	delete[] depth_vals;
	for(int x = 0; x < pt_count; x++){
		delete[] points[x];	
	}
	delete[] points;
	
	return im_depth;

}



void filter(int type, int numFrames, char **filenames, float stdevX, float stdevY, float stdevColor, float stdevTime){
	
	//load query image
	Image slice_image = Load(filenames[2]);

	//type of filter
	if(type == 0){ //rgb descriptor
		Image depth = do_filter_rgb(numFrames, filenames, slice_image, stdevX, stdevY, stdevColor, stdevTime);
		Save(depth, filenames[1]);
	}
  //else if(type == 1) //use different descriptor...

}


/*
 * Command-line interface for running the algorithm.
 *
 * It is assumed that the input files are in a particular format (.tmp)
 * To be precise, the file begins with a header describing the image size (see the struct header), followed by a floating-point
 * array of size [frame*width*height*channels]. The floats are ordered by frame first, then row, then column, then channel.
 * The input files should have uniform width and height, have a single frame containing 5 channels (r,g,b,t,depth),
 * where depth of 0 indicates no data.
 * The output will be 1 frame of the same image size (based on the input query image) 
 * -- uncomment the appropriate lines above to also save a confidence map
 * The standard deviations of the blurs are hard-coded. Change these to blur more or less in arbitrary dimensions.
 */

int main(int argc, char **argv)
{
  // Check input parameters.
  if (argc < 3) {
    printf("Usage: ./%s output.tmp slice_image.tmp input1.tmp input2.tmp .... inputn.tmp\n", argv[0]);
    printf("  The sampling percentage specifies how many of the input depth points should be kept.\n");
    printf("  Set this to 100 for normal use. If you have a synthetic dataset with dense depthmap,\n");
    printf("  Lower this appropriately to avoid exceeding space in memory.\n");
    return 1;
  }

  //number of data files input
  int nFrames = argc - 3;

  //change these values to change standard deviations. 
  //(also note you can modify the code above if you want a different standard deviation for each color channel)
  double stdevX = 10.0;
  double stdevY = 10.0;
  double stdevColor = 0.25; 
  double stdevTime = 0.15;

  filter(0, nFrames, argv, stdevX, stdevY, stdevColor, stdevTime);

  return 0;
}
