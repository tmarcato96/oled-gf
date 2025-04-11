### Build prokect

## Mac

- Install `gcc-14` via ([Homebrew](https://formulae.brew.sh/formula/gcc))

- Make sure your conda environment is deactivate with `conda deactivate`

- Build the project

```
cmake -S . -B ./build -DCMAKE_CXX_COMPILER=/usr/local/bin/g++-14  && cd ./build
```

and compile it

```
make
```

## Docker Development environment

To ensure a standardized environment for development the project has to be built within a Docker container.

In order to do so it requires:

- Docker
- An X11 server to forward the application UI 

### Install Docker

- Download and install Docker Desktop ([Windows](https://docs.docker.com/desktop/install/windows-install/) or [Mac](https://docs.docker.com/desktop/install/mac-install/)).

- Open Docker Desktop to start the Docker daemon.

- (On Windows) If the Docker engine cannot run make sure that you have enabled Hardware Virtualization (see [this guide](https://docs.docker.com/desktop/troubleshoot/topics/#virtualization))

### Install X11 Server

#### Windows

- Download and install [XServer](https://sourceforge.net/projects/vcxsrv/)

#### Mac

- Download and install [XQuartz](https://www.xquartz.org/)
- In XQuartz settings, under Security tab, check "Allow connections from network clients"
- Restart XQuartz
- In a terminal on the host, run `xhost +localhost`

### Create Dev Container Image

First clone the main repo and enter root dir of project
```
git clone https://gitlab.ethz.ch/tmarcato/oledgf.git
cd oledgf
```

Open terminal and run
```
docker build -t test:0.0.1 -f Dockerfile .
```
to build the docker image `test` that will contain the dev environment.

### Run the container

In the terminal

```
docker run -it --rm --name=test --mount type=bind,source=${PWD},target=/src test:0.0.1 bash
```
If you are running docker from Git Bash on Windows you should run 

```
winpty docker run -it --rm --name=test --mount type=bind,source=${PWD},target=/src test:0.0.1 bash
```

You will be now in the bash terminal of the container (based on Ubuntu).

Navigate to the `src` directory which is mapped to the project root.

```
cd ./src
```

### Build the project

Build the project

```
cmake -S . -B ./build && cd ./build
```

and compile it

```
make
```

# Build Details

The build system is based on the [cmake_template](https://github.com/cpp-best-practices/cmake_template) by Jason Turner.

For now it includes

- High level warnings and Warnings as Errors (`OFF` by default)
- [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
- [CPM](https://github.com/cpm-cmake/CPM.cmake) for external dependencies
- Sanitizers
- Static check with CPPcheck

## External dependencies

The project relies on the following dependencies

- [fmt](https://github.com/fmtlib/fmt): For formatting
- [Eigen](https://gitlab.com/libeigen/eigen): For Matrices and linear algebra
- [Matplot++](https://github.com/alandefreitas/matplotplusplus): For plotting (gnuplot backend)

## TO DOs

- Adjust Simulation configuration from JSON input file after refactoring (Tommaso)
- We should probably support some standard input files as given from our experimental setups
- Work on a standardized output format for all the important simulation and fitting results
- Turn application into CLI with [CLI11](https://github.com/CLIUtils/CLI11)
- Test and build on different platforms. Maybe experiment with Workflows as well
