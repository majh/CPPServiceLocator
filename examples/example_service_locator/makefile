example: main.cpp
	$(CXX) -std=c++11 -g -o example main.cpp -I../../

examplexx: main.cpp
	$(CXX) -std=c++0x -o example main.cpp -I../../ -I/home/matt/devel/src/mtdata/repos/4000/Image/trunk/ltib/rootfs/include

check: check.cpp
	$(CXX) -std=c++0x -o check check.cpp -I../../ -I/home/matt/devel/src/mtdata/repos/4000/Image/trunk/ltib/rootfs/include


clean:
	rm -f example core
