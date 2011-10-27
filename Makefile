all: gkdtree_based

gkdtree_based: gkdtree_based.cpp gkdtree2.h Image.cpp Image.h
	g++ -g -O0 gkdtree_based.cpp Image.cpp -Wall -o gkdtree_based

clean:
	-rm gkdtree_based
