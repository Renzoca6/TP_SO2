#TPE_SO

Como correrlo 

docker run --name tp-so  -v "${PWD}:/root" --privileged -ti agodio/itba-so-multiarch:3.1 
./compile.sh 
./run.sh 