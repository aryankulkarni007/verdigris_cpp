<-*
  * SOON THIS README WILL BE INFORMATIVE
  * maybe after parser
*->

claude context for new convo

I am writing a general purpose scripting language called verdigris. i want professional grade tooling and compiler even though I am a solo developer. I have 3 past iterations of this. The first I wrote in C and I realised that my lexer and parser where both lossy and context-blind and couldn't do error handling. Secondly, i started properly again in C, rebranding to verdigris, although that is probably still a codename. This time i approached it meticulously and there is nothing wrong with the implementation, but i reached 5000 lines of code before even writing the fucking cst parser but everything else was pretty good. Now I am undertaking a rewrite in C++. this i intend to take to the finish line. i prefer to write c style c++, while taking advantage of the features so i don't have to take addy to code (hyperbole). i have implemented an arena class and a token class and I am starting on the Lexer class. I intended to implement a lossless lexer and overall an eventually fully features programming language. I will provide the current source code. Here are you instructions

* I want professional, senior developer advice that teaches me C++ and also guides me with compiler design
* I want to write the code. guide me and provide scaffolding
* Participate in design discussion when required keeping an open mind and being able to backtrack and re-evaluate echo chamber issues and potential technical debt
/caveman

BUILD INSTRUCTIONS

docker container

- ( ) do this once
docker build -t verdigris-dev .
- then
docker run -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -v "$(pwd):/app" verdigris-dev

___________________________

LINUX

cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux/
valgrind --leak-check=full ./build-linux/bin/verdigris
./build-linux/bin/verdigris

MAC

cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mac/
./build-mac/bin/verdigris
