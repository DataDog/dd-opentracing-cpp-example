# Build the dependencies.
FROM ubuntu:20.04 as build-dd-opentracing

# Don't prompt the user for time zone information.
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y build-essential cmake wget tar

RUN mkdir -p /build
COPY bin/install_dd_opentracing.sh /build
WORKDIR /build

RUN ./install_dd_opentracing.sh

# Build the example.
FROM ubuntu:20.04
# Copy over headers/libraries built in the previous stage.
COPY --from=build-dd-opentracing /usr/local/include /usr/local/include
COPY --from=build-dd-opentracing /usr/local/lib /usr/local/lib

RUN apt-get update && apt-get install -y build-essential wget coreutils tar ca-certificates

RUN mkdir -p /src/tracer_example
COPY src/ /src/tracer_example
WORKDIR /src/tracer_example

RUN make -j $(nproc) && mkdir -p /bin && cp /src/tracer_example/tracer_example /bin

# After `docker-compose up --detach`, you can `docker attach` to this
# container in order to interact with the example command line tool.
CMD /bin/tracer_example
