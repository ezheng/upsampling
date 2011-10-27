
#include "Image.h"

// The rest of the Image class is inlined.
// Inlining the destructor makes the compiler unhappy, so it goes here instead

Image::~Image() {    
    if (!refCount) {
        //printf("Deleting NULL image\n");
        return; // the image was a dummy
    }
    refCount[0]--;
    if (*refCount <= 0) {
        //printf("Deleting image ");
        //debug();
        delete refCount;
        //printf("refCount deleted");
        //debug();
        delete[] data;
        //printf("data deleted");
        //debug();
    }
}
    
