# syntax=docker/dockerfile:1

# Stage 1: Build crfsuite
FROM debian:bookworm AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    autoconf \
    automake \
    libtool \
    git

ADD https://github.com/chokkan/crfsuite.git#v0.12 /tmp/crfsuite

RUN cd /tmp/crfsuite && \
    ./autogen.sh && \
    ./configure && \
    make && \
    make install

# Stage 2: Build the Postgres extension
ARG PG_MAJOR=16
ARG DEBIAN_CODENAME=bookworm
FROM postgres:$PG_MAJOR-$DEBIAN_CODENAME

# Copy crfsuite from the builder stage
COPY --from=builder /usr/local/lib/ /usr/local/lib/
COPY --from=builder /usr/local/include/ /usr/local/include/

# Install build dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    postgresql-server-dev-$PG_MAJOR && \
    ldconfig

# Copy the extension source code
COPY . /usr/src/pg_probablepeople

# Build and install the extension
RUN cd /usr/src/pg_probablepeople && \
    make clean && \
    make && \
    make install

# Clean up build dependencies
RUN apt-get remove -y build-essential postgresql-server-dev-$PG_MAJOR && \
    apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*
