# Use Python 3.10 base image (includes gcc and build essentials)
FROM python:3.10

# Set working directory
WORKDIR /app

# Install dependencies: CMake, OpenMP, and build tools
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    libomp-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code into the container
COPY . .

# Create a build directory and run CMake to configure the project
RUN mkdir -p container_build

# Create a folder to store the simulation results
RUN mkdir -p simulation_results

# Set working directory to the build directory
WORKDIR /app/container_build

# Prepare the build with Python bindings enabled
RUN cmake -DENABLE_PYTHON_BINDINGS=TRUE \
          -DPYTHON_EXECUTABLE=$(which python3) \
          -DCMAKE_BUILD_TYPE=Release ..

# Build the project using all available cores
RUN make -j$(nproc)

# Define the command to run when the container starts
# Use the correct path to the executable
ENTRYPOINT ["/app/container_build/src/app/simucell3d"]