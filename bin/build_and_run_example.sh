#!/bin/sh

if [ -z "$DD_API_KEY" ]; then
  >&2 echo "Please set environment variable DD_API_KEY."
  exit 1
fi

# `cd` to the directory containing `docker-compose.yml`.
cd "$(dirname "$0")"/..

DD_API_KEY="$DD_API_KEY" docker-compose up --build --detach
container_id=$(docker-compose ps --quiet dd-opentracing-cpp-example)

# The example command line tool can only see its own file system
# (good choices for a directory are "/var/log" and "/src").
printf "enter a directory (ctrl+d to quit): "
docker attach "$container_id"

# Now that the example command line tool has exited, give the agent a few
# seconds before we shut everything down.
printf "Shutting down in"
for n in $(seq 3 -1 1); do
    printf " $n..."
    sleep 1
done;
printf " here we go!\n"

# Shut everything down.
docker-compose down
