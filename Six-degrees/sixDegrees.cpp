#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <set>
#include "imdb.h"

// The BFS algorithm to generate the shortest path between two actors
void generateShortestPath(imdb &db, const std::string &startActor, const std::string &endActor) {
    // Define a structure to represent a path consisting of actors and movies
    struct path {
        std::vector<std::string> actors;
        std::vector<film> movies;
    };

    // List to function as a queue for partial paths during BFS
    std::list<path> partialPaths;
    // Set to keep track of actors that have been seen
    std::set<std::string> previouslySeenActors;
    // Set to keep track of movies that have been seen
    std::set<film> previouslySeenFilms;

    // Initialize the starting path with the start actor
    path initialPath;
    initialPath.actors.push_back(startActor);
    partialPaths.push_back(initialPath);
    previouslySeenActors.insert(startActor);

    // Perform BFS until the queue is empty or the path length exceeds 6
    while (!partialPaths.empty() && partialPaths.front().actors.size() <= 6) {
        // Get the current path from the front of the queue
        path currentPath = partialPaths.front();
        partialPaths.pop_front();

        // Retrieve the list of movies the last actor in the current path has appeared in
        std::vector<film> movies;
        if (!db.getCredits(currentPath.actors.back(), movies)) {
            // Print an error if the actor's credits cannot be retrieved
            std::cerr << "Failed to get credits for actor: " << currentPath.actors.back() << std::endl;
            continue;
        }

        // Iterate through the movies of the current actor
        for (const film &movie : movies) {
            // Skip the movie if it has already been seen
            if (previouslySeenFilms.find(movie) != previouslySeenFilms.end()) continue;
            previouslySeenFilms.insert(movie);

            // Retrieve the cast of the current movie
            std::vector<std::string> cast;
            if (!db.getCast(movie, cast)) {
                // Print an error if the movie's cast cannot be retrieved
                std::cerr << "Failed to get cast for movie: " << movie.title << std::endl;
                continue;
            }

            // Iterate through the cast members of the current movie
            for (const std::string &actor : cast) {
                // Skip the actor if they have already been seen
                if (previouslySeenActors.find(actor) != previouslySeenActors.end()) continue;
                previouslySeenActors.insert(actor);

                // Create a new path including the current movie and actor
                path newPath = currentPath;
                newPath.movies.push_back(movie);
                newPath.actors.push_back(actor);

                // If the end actor is found, print the path and return
                if (actor == endActor) {
                    for (size_t i = 0; i < newPath.actors.size(); ++i) {
                        if (i > 0) std::cout << " --> ";
                        std::cout << newPath.actors[i];
                        if (i < newPath.movies.size()) {
                            std::cout << " (" << newPath.movies[i].title << ")";
                        }
                    }
                    std::cout << std::endl;
                    return;
                }
                // Add the new path to the queue for further exploration
                partialPaths.push_back(newPath);
            }
        }
    }
    // Print a message if no connection is found between the start and end actors
    std::cout << "No connection found." << std::endl;
}

int main() {
    // Initialize the imdb object with the directory containing the database files
    std::string directory = "path/to/database";  // Change this to the actual path
    imdb db(directory);

    // Input the names of the start and end actors
    std::string startActor, endActor;
    std::cout << "Enter the name of the starting actor: ";
    std::getline(std::cin, startActor);
    std::cout << "Enter the name of the ending actor: ";
    std::getline(std::cin, endActor);

    // Generate and print the shortest path
    std::cout << "Finding the shortest path between \"" << startActor << "\" and \"" << endActor << "\"..." << std::endl;
    generateShortestPath(db, startActor, endActor);

    return 0;
}
