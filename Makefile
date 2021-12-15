INCLUDE_OPENCV = `pkg-config --cflags --libs opencv`
all: sender receiver agent
sender: sender.cpp
	g++ sender.cpp -o sender $(INCLUDE_OPENCV)
receiver: receiver.cpp
	g++ receiver.cpp -o receiver $(INCLUDE_OPENCV)
agent: agent.cpp
	g++ agent.cpp -o agent $(INCLUDE_OPENCV)
clean:
	rm -f sender receiver agent