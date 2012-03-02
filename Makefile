
moodpd:		src/main.cpp src/*.h oscpkt/*
		g++ -Ioscpkt -O2 -ggdb -o moodpd src/main.cpp
