#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iterator>
#include <algorithm>
#include "imdb.h"
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
const size_t BYTE_SIZE = 8;
const unsigned int MOVIE_BASE_YEAR = 1900;
const unsigned int NUM_BYTES_FOR_YEAR = 1;


imdb::imdb(const string& directory) {
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

imdb::~imdb() {
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// Helper function : extracts "film" from binary image given offset
film extractFilm(const void *movieFile, int offSet) {
    film movie;
    movie.title = ((char*)movieFile + offSet);
    char yr = *((char*)movieFile + offSet + movie.title.length() + 1);
    movie.year = MOVIE_BASE_YEAR + yr;
    // yr = MOVIE_BASE_YEAR + (yr >> ((sizeof(int) - NUM_BYTES_FOR_YEAR)*BYTE_SIZE));
    // movie.year = yr;
    return movie;        
}

bool imdb::getCredits(const string& player, vector<film>& films) const {
    int numActors = *(int*)actorFile;
    int *iterStart = (int*)actorFile + 1;
    int *iterEnd = (int*)actorFile + numActors;
    film temp = extractFilm(movieFile, 60062304);
    //search the required actor name
    int *elemPos = std::lower_bound(iterStart, iterEnd, player,
                                    [&](int currPos, const string playa) {
                                        return ((char*)actorFile + currPos)<playa;
                                    });
    
    //ACTOR DOESN'T EXIST
    if(((char*)actorFile + *elemPos)!=player) {
        films.clear();
        return false;
    }
    
    //IF ACTOR EXISTS:
    // 1. Retrieve corresponding movies' offset values
    int nameLen = strlen((char*)actorFile + *elemPos);
    (nameLen % 2 == 0) ? nameLen += 2 : nameLen += 1;  //padding
    short actorsNumMovies = *(short*)((char*)actorFile + (*elemPos) + nameLen);
    //pointer to first movie's offset value in actorFile 
    int *firstMovOffset = (int*) ((char*)actorFile + *elemPos +
                        (((nameLen+2) % 4 == 0) ? nameLen+2 : nameLen + 4));
    
    // 2. Retrieve movie name and fill up vector
    for(int i = 0; i < actorsNumMovies; i++) {
        films.push_back(extractFilm(movieFile, *(firstMovOffset + i)));
    }
    return true;
}

bool imdb::getCast(const film& movie, vector<string>& players) const {
    int numMovies = *(int*)movieFile;
    int *iterStart = (int*)movieFile + 1;
    int *iterEnd = (int*)movieFile + numMovies;
    
    // Search the required actor name
    int *elemPos = std::lower_bound(iterStart, iterEnd, movie,
                                    [&](int currPos, const film& movi) {
                                        return extractFilm(movieFile, currPos).operator<(movi);
                                    });
    
    // IF MOVIE DOESN'T EXIST
    if(! extractFilm(movieFile, *elemPos).operator==(movie)) {
        players.clear();
        return false;
    }

    // IF MOVIE EXISTS
    // 1. Retrieve corresponding actors' offset values
    int nameLen = extractFilm(movieFile, *elemPos).title.length();
    (nameLen % 2 == 0) ? nameLen += 2 : nameLen += 3;   //padding
    short numActorsInMovie = *(short*)((char*)movieFile + *elemPos + nameLen);
    // Pointer to first actors's offset value in movieFile 
    int *firstActorOffset = (int*) ((char*)movieFile + *elemPos +
                        (((nameLen+2) % 4 == 0) ? nameLen+2 : nameLen + 4));
    
    // 2. Retrieve movie name and fill up vector
    for(int i = 0; i < numActorsInMovie; i++) {
        string actor = ((char*)actorFile + *(firstActorOffset + i)) ;
        players.push_back(actor);
    }
    
    return true; 
}

const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info) {
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info) {
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
