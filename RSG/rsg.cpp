#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>
#include <stack>
#include <cctype>

using namespace std;

// Structure to represent a production (sequence of items)
struct Production {
    vector<string> items;
};

// Structure to represent a non-terminal and its productions
struct NonTerminal {
    vector<Production> productions;
};

// Function to trim leading and trailing whitespace
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t");
    size_t last = str.find_last_not_of(" \t");
    if (first == string::npos || last == string::npos)
        return "";
    return str.substr(first, last - first + 1);
}

// Function to parse a grammar file and build the CFG
map<string, NonTerminal> parseGrammar(const string& filename) {
    ifstream file(filename);
    map<string, NonTerminal> grammar;

    if (!file.is_open()) {
        cerr << "Error: Unable to open file " << filename << endl;
        exit(1);
    }

    string line;
    string currentNonTerminal;

    while (getline(file, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#' || line[0] == '{') {
            continue; // Skip empty lines, comments, and irrelevant text
        }

        size_t pos = line.find_first_of("}");
        if (pos != string::npos) {
            line = line.substr(0, pos); // Trim off any trailing comment
        }

        istringstream iss(line);
        string token;
        vector<string> tokens;

        while (iss >> token) {
            if (token == ";") {
                break; // End of production
            }
            tokens.push_back(token);
        }

        if (!currentNonTerminal.empty() && !tokens.empty()) {
            if (currentNonTerminal == "<start>") {
                for (const string& t : tokens) {
                    if (t != "<start>") {
                        grammar[currentNonTerminal].productions.push_back({tokens});
                        break;
                    }
                }
            }
            grammar[currentNonTerminal].productions.push_back({tokens});
        } else if (!tokens.empty() && tokens[0] == "<start>") {
            currentNonTerminal = tokens[0]; // Update current non-terminal
        }
    }

    file.close();
    return grammar;
}

// Function to generate a random sentence using the CFG
// Function to generate a random sentence using the CFG
vector<string> generateRandomSentence(const map<string, NonTerminal>& grammar, const string& startSymbol) {
    vector<string> sentence;

    if (grammar.find(startSymbol) == grammar.end()) {
        cerr << "Error: Start symbol '" << startSymbol << "' not found in grammar." << endl;
        return sentence;
    }

    srand(time(0)); // Seed random number generator

    stack<string> symbols;
    symbols.push(startSymbol);

    while (!symbols.empty()) {
        string symbol = symbols.top();
        symbols.pop();

        if (grammar.find(symbol) != grammar.end()) {
            // Non-terminal symbol, choose a random production
            const NonTerminal& nt = grammar.at(symbol);
            const Production& prod = nt.productions[rand() % nt.productions.size()];

            // Push items of the production onto the stack in reverse order
            for (auto it = prod.items.rbegin(); it != prod.items.rend(); ++it) {
                symbols.push(*it);
            }
        } else {
            // Terminal symbol, add to the sentence
            sentence.push_back(symbol);

            // Check if we've formed a complete sentence
            if (sentence.size() >= 5) {
                break; // Stop once we have at least 5 words in the sentence
            }
        }
    }

    return sentence;
}

int main() {
    // Specify the full path to your grammar file
    string grammarFile = "C:\\Users\\Artur\\Documents\\c++ projects\\grammar.txt";
    
    // Parse the grammar file and build the CFG
    map<string, NonTerminal> grammar = parseGrammar(grammarFile);

    // Specify the start symbol
    string startSymbol = "<start>";

    // Generate a random sentence using the CFG
    if (grammar.find(startSymbol) != grammar.end()) {
        vector<string> sentence = generateRandomSentence(grammar, startSymbol);

        // Print the generated sentence
        for (const string& word : sentence) {
            cout << word << " ";
        }
        cout << endl;
    } else {
        cerr << "Error: Start symbol '<start>' not found in grammar." << endl;
    }

    return 0;
}

