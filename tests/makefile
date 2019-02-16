tests: ServiceLocatorTests.cpp
	$(CXX) -std=c++11 -g -o tests ServiceLocatorTests.cpp -I../ -ICatch/include


testsxx: ServiceLocatorTests.cpp
	$(CXX) -std=c++0x -g -o tests ServiceLocatorTests.cpp -I../ -ICatch/include -I/home/matt/devel/src/mtdata/repos/4000/Image/trunk/ltib/rootfs/include

clean:
	rm -f tests core

