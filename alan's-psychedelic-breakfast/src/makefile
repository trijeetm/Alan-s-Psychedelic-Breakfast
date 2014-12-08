
CXX=g++
INCLUDES=
FLAGS=-D__MACOSX_CORE__ -O3 -c -w
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ -lm

OBJS=   RtAudio.o visualizer.o chuck_fft.o

visualizer: $(OBJS)
	$(CXX) -o visualizer $(OBJS) $(LIBS)

visualizer.o: visualizer.cpp RtAudio.h
	$(CXX) $(FLAGS) visualizer.cpp

RtAudio.o: RtAudio.h RtAudio.cpp RtError.h
	$(CXX) $(FLAGS) RtAudio.cpp

chuck_fft.o: chuck_fft.h chuck_fft.c
	$(CXX) $(FLAGS) chuck_fft.c

clean:
	rm -f *~ *# *.o visualizer
