FROM postgres:17

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    postgresql-server-dev-17 \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy extension source
COPY . /usr/src/pg_probablepeople
WORKDIR /usr/src/pg_probablepeople

# Clean any existing build artifacts
RUN make clean

# Compile and install
RUN make && make install
