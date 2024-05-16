// Header guard to prevent multiple inclusions of the header file
#ifndef IMDB_H
#define IMDB_H

// Include necessary standard library headers
#include <string>
#include <vector>

// Definition of the film structure
struct film {
    // Title of the film
    std::string title;
    // Year the film was released
    int year;

    // Comparison operator to determine if one film is less than another
    bool operator<(const film &other) const {
        // If the titles are the same, compare by year
        if (title == other.title) {
            return year < other.year;
        }
        // Otherwise, compare by title
        return title < other.title;
    }

    // Equality operator to determine if two films are the same
    bool operator==(const film &other) const {
        // Films are equal if both the title and year are the same
        return title == other.title && year == other.year;
    }
};

// Definition of the imdb class
class imdb {
public:
    // Constructor that initializes the imdb object using a directory path
    imdb(const std::string& directory);
    // Destructor to clean up any resources used by the imdb object
    ~imdb();

    // Retrieves the list of films an actor has appeared in
    bool getCredits(const std::string& player, std::vector<film>& films) const;
    // Retrieves the list of actors in a given film
    bool getCast(const film& movie, std::vector<std::string>& players) const;

private:
    // Pointer to the block of memory containing actor data
    const void *actorFile;
    // Pointer to the block of memory containing movie data
    const void *movieFile;

    // Helper function to extract data from the actor file
    bool getCreditsFromActorFile(const std::string& player, std::vector<film>& films) const;
    // Helper function to extract data from the movie file
    bool getCastFromMovieFile(const film& movie, std::vector<std::string>& players) const;
};

// End of header guard
#endif // IMDB_H
